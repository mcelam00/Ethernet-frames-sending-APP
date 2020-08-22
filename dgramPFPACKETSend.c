/*
 * Conceptual Computer Networks textbook
 * CN course 2020
 * Express Practice: Simple Datagram PF_PACKET DGRAM send program
 * dgramPFPACKETSend.c
 * All rights reserved:
 * (C) 2020 by José María Foces Morán & José María Foces Vivancos
 * Modification and extra functionalities: Mario Celada Matías 2nd Course
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <fcntl.h>
#include <memory.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>

#include <signal.h>
#include <errno.h>
#include <sys/time.h>

#define byte u_char
#define TRUE 1
#define ETHERTYPE_EXPERIMENTAL 0x07ff
#define DEFAULT_MTU 1500

void setDestinationMAC(byte *p) {

    /*
     * Fill the byte array pointed to by variable p with the
     * bytes that comprise the broadcast address (All 1's)
     */
    p[0] = (byte) 0xff;
    p[1] = (byte) 0xff;
    p[2] = (byte) 0xff;
    p[3] = (byte) 0xff;
    p[4] = (byte) 0xff;
    p[5] = (byte) 0xff;

}

/*
 * Create a "large" payload of size payloadSize by cloning baseDataToClone[] as 
 * many times as it fits including the remainder size
 */
char * buildPayload(char *baseDataToClone, unsigned baseDataSize, unsigned payloadSize) {

    char *p = (char *) '\0';

    if (payloadSize == 0 || baseDataSize == 0) {
        fprintf(stderr, "Error: Requested payload size and base data size must be both greater than 0.\n");
        return p;
    }

    if (payloadSize > DEFAULT_MTU) {
        fprintf(stderr, "Error: Requested payload size exceeds Ethernet maximum MTU.\n");
        return p;
    }

    if (payloadSize < baseDataSize) {
        fprintf(stderr, "Error: Payload size cannot fit the base data to be cloned.\n");
        return p;
    }

    /*
     * Request dynamic memory space for the payload to be built
     * p is a sentinel marking the start of the payload being built
     * We'll return p to our calling function
     */
    p = malloc(payloadSize);

    /*
     * Copy p into q and use the latter for indexing the payload as
     * we fill it in the for loop below
     */
    char *q = p;

    /*
     * Copy baseDataToClone as many times as it fits payloadSize
     * Each copy of baseDataSize bytes is made by calling memcpy()     
     */
    for (int i = 0; i < (payloadSize / baseDataSize); i++) {
        memcpy(q, baseDataToClone, baseDataSize);
        //Move q pointer forward baseDataSize bytes
        q += baseDataSize;
    }

    /* If the integer division (payloadSize / baseDataSize) produces a
     * remainder (payloadSize % baseDataSize), copy the number of bytes
     * represented by the remainder from baseDataToClone to q
     */
    memcpy(q, baseDataToClone, payloadSize % baseDataSize);

    return p;
}

void printProgramLegend(char *payload) {

    printf("Send a frame with PF_PACKET/SOCK_DGRAM\n");
    printf("\tDMAC = ff:ff:ff:ff:ff:ff\n");
    printf("\tSMAC = Network Interface's MAC\n");
    printf("\tEtherytpe = %hx\n", ETHERTYPE_EXPERIMENTAL);
    //printf("\tPayload=\"%s\"", payload);

    fflush(stdout);

}

/*
 * This function fills the fields of the socket address structure
 * Some of the come from the command line arguments passed by the user
 * 
 * u_int16_t is  used for representing the ethertype
 * u_int16_t is declared int /usr/include/x86_64-linux-gnu/sys/types.h
 * with #include <sys/types.h>
 * 
 * __be16 is defined in /usr/include/linux/types.h and is also used for
 * representing ethernet's ethertype field
 */
struct sockaddr_ll fillSocketAddress(char *ifName, u_int16_t ethertype) {

    /*
     * sockaddr_ll has slightly different used when sending than are used when
     * receiving.
     * When sending, sockaddr_ll stores the Destination MAC and the 
     * multiplexing key (Ethertype) and the index of the interface to be used for
     * actually transmitting the frame
     * 
     * When receiving, sockaddr_ll stores the Source MAC address, the received
     * Ethertype and the interface index the frame was received onto
     */
    struct sockaddr_ll socketAddress;

    //T
    socketAddress.sll_family = PF_PACKET;

    /* Index of network interface */
    socketAddress.sll_ifindex = if_nametoindex(ifName);
    if (socketAddress.sll_ifindex == 0) {
        perror("Error indexing interface name");
        exit(-2);
    }

    /* Address length*/
    socketAddress.sll_halen = ETH_ALEN;

    //Ethertype translated to Network Byte Order
    socketAddress.sll_protocol = htons(ethertype);

    //arp-related
    socketAddress.sll_hatype = 0;

    //arp-related
    socketAddress.sll_pkttype = 0;

    return socketAddress;

}

void start(char *ifName, char *baseDataToClone, int baseDataSize, int payloadSize) {

    /* 
     * Print the frame fields when program begins to run
     */
    printProgramLegend(baseDataToClone);

    /* 
     * This struct stores basic Raw socket parameters such as:
     * multiplexing key (Ethertype), Destination MAC, etc
     */
    struct sockaddr_ll socketAddress = fillSocketAddress(ifName, (u_short) ETHERTYPE_EXPERIMENTAL);
    setDestinationMAC(&(socketAddress.sll_addr[0]));

    /* 
     * Create a socket with the following three actual parameter values:
     * 
     * Arg 1: PF_PACKET is the address family used by Ethernet/Datalink sockets in Linux
     * 
     * Arg 2: SOCK_DGRAM is the type of communication style to be used with this socket:
     *  - SOCK_DGRAM means that the programmer is letting the building of the
     *    frame's header to the sockets layer (The service interface itself),
     *    i.e., the programmer is not building the header but is only the payload
     *    as we did above.
     *    
     *  - The other option available for this argument is constant SOCK_RAW 
     *    which means that the programmer is providing a full datalink header
     * 
     * Arg 3: Ethernet's multiplexing key (Ethertype); in this case we are using
     *  - ETHERTYPE_EXPERIMENTAL (0x07ff) which is not reserved. Since we already
     *    loaded value ETHERTYPE_EXPERIMENTAL into the socket address created above,
     *    we use it again, for consistency (socketAddress.sll_protocol) 
     * 
     */
    int sock = socket(PF_PACKET, SOCK_DGRAM, socketAddress.sll_protocol);

    /*
     * Call function to have the payload built from a base array of bytes that is
     * going to be cloned a number of times. baseDataToClone is entered by the user
     * on the command line. payloadSize is the total size of the payload
     */
    byte *p = buildPayload(baseDataToClone, baseDataSize, payloadSize);
    if (p == (byte *) '\0') {
        exit(-1);
    }

    /*
     * Finally, the data is ready to be sent onto socket sock
     * sock:    The socket created above
     * p:       Pointer to an array of bytes (unsigned char) that contains the
     *          data to be sent onto the socket
     * payloadSize:
     *          The size of the array pointed to by p in bytes
     * 0:       Options for this socket
     * socketAddress:
     *          Storage for the socket address which contains fields such as:
     *              · Dest MAC address
     *              · Ethertype (Ethernet's multiplexing key)
     *              · Consult /usr/include/linux
     */
    if (sendto(sock, p, payloadSize, 0, (struct sockaddr *) &socketAddress, sizeof (socketAddress)) == -1) {
        printf("\nsendto() call failed\n");
        perror("sendto: ");
        exit(-1);
    }
}

int main(int argc, char** argv) {

    /*
     * Command-line processing 
     */
    if (argc == 3) {

        //call start with default size of 128B
        start(argv[1], argv[2], strlen(argv[2]), 128);
        printf("Simple frame successfully sent to the broadcast address via %s.\n", argv[1]);

    } else if (argc == 4) {

        int paySize = atoi(argv[3]);
        if (paySize < 0 || paySize > DEFAULT_MTU) {
            fprintf(stderr, "Invalid value for Payload size to send; should be [1, 1500]\n", argv[0]);
            exit(-1);
        }
        //call start by passing no default parameter
        start(argv[1], argv[2], strlen(argv[2]), paySize);
        printf("Simple frame successfully sent to the broadcast address via %s.\n", argv[1]);

    } else {

        fprintf(stderr, "Usage: %s\n", argv[0]);
        fprintf(stderr, "\t<Network Interface>\n");
        fprintf(stderr, "\t<Data to be cloned between quotation marks (\"...\")>\n");
        fprintf(stderr, "\t[Payload size to send (Max. 1500B, Default 128B)]\n");

        exit(-1);
    }

}