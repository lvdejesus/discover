#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef enum {
  DIR_NONE,
  DIR_IN,
  DIR_OUT,
} Direction;

typedef enum {
  DMSG_BEACON,
  DMSG_CONNECT,
} MessageType;

typedef struct {
  MessageType type;
  union {
    struct {
      uint8_t has_password;
    };
    struct {
      uint8_t hash[32];
    };
  };
} Message;

static inline void print_help_text() {
  FILE *fp = fopen("./help.txt", "r");
  if (fp == NULL) {
    fprintf(stderr, "Help text not found.\n");
    exit(EXIT_FAILURE);
  }

  char buf[4096];
  size_t bytes_read;

  while ((bytes_read = fread(buf, 1, sizeof(buf), fp)) > 0)
    fwrite(buf, 1, bytes_read, stderr);

  fclose(fp);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  Direction dir = DIR_NONE;
  if (!isatty(0)) {
    dir = DIR_IN;
  }

  if (!isatty(1)) {
    if (dir == DIR_IN) {
      fprintf(stderr, "Should only be piping in or piping out.\n");
      exit(EXIT_FAILURE);
    }

    dir = DIR_OUT;
  }

  size_t idx = 1;
  size_t aidx = 0;
  char option;
  while (idx < argc) {
    const char *arg = argv[idx++];
    if (aidx > 0) {
      switch (option) {
      case 'c':
      case 'i':
      case 'p':
      case 'r':
      case 's':
      case 't':
        break;
      }

      aidx--;
    }

    if (arg[0] != '-') {
      fprintf(stderr, "Unexpected argument.\n");
    }

    switch (arg[1]) {
    case 'c':
    case 'i':
    case 'p':
    case 'r':
    case 's':
    case 't':
      option = arg[1];
      aidx = 1;
      break;
    default:
      print_help_text();
      exit(EXIT_FAILURE);
      break;
    }
  }

  if (aidx > 0) {
    fprintf(stderr, "Expected an argument.\n");
  }

  // -c
  //   Number of times to send the broadcast message. Infinite if 0.
  // -i
  //   Interval for sending the broadcast message. This defaults to 1 second.
  // -p
  //   A code that should be equal between the sender and receiver.
  // -r
  //   The receiver sends the broadcast
  // -s
  //   The one which sends the broadcast selects a destination through a tty
  //   after a timeout
  // -t
  //   Sets the timeout for -s. This defaults at 3 seconds.

  if (dir == DIR_NONE) {
    print_help_text();
  } else if (dir == DIR_IN) {
    char *password = NULL;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &(int[]){1}, sizeof(int));

    struct sockaddr_in broadcastAddr = {
        .sin_family = AF_INET,
        .sin_port = htons(8888),
        .sin_addr.s_addr = inet_addr("255.255.255.255"),
    };

    Message msg = {
        .type = DMSG_BEACON,
        .has_password = password != NULL,
    };

    sendto(sock, &msg, sizeof(msg), 0, (struct sockaddr *)&broadcastAddr,
           sizeof(broadcastAddr));

    Message retmsg;
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    int len = recvfrom(sock, &retmsg, sizeof(retmsg), 0,
                       (struct sockaddr *)&clientAddr, &addrLen);
    if (len > 0) {
      if (retmsg.type == DMSG_CONNECT) {
        printf("%s\n", inet_ntoa(clientAddr.sin_addr));
      }
    }
  } else if (dir == DIR_OUT) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in broadcastAddr = {
        .sin_family = AF_INET,
        .sin_port = htons(8888),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (bind(sock, (struct sockaddr *)&broadcastAddr, sizeof(broadcastAddr)) <
        0) {
      perror("bind failed");
      exit(EXIT_FAILURE);
    }

    char buf[256];
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    int len = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                       (struct sockaddr *)&clientAddr, &addrLen);
    if (len > 0) {
      buf[len] = '\0';
      printf("%s: %s\n", inet_ntoa(clientAddr.sin_addr), buf);
    }

    const char *msg = "ack";
    sendto(sock, msg, strlen(msg), 0, (const struct sockaddr *)&clientAddr,
           addrLen);
  }
}
