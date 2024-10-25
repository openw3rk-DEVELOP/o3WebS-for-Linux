#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>

// o3WebS - Version: 1.9
// Copyright (c) openw3rk INVENT

#define PORT 8080
#define DOCS_DIR "/var/o3docs"
#define BUFFER_SIZE 1024
#define PID_FILE "/var/run/o3webs.pid"

int server_running = 1;
int server_socket = -1;

void *handle_client(void *client_socket_ptr) {
    int client_socket = *(int *)client_socket_ptr;
    free(client_socket_ptr);
    
    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        const char *response_header = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
        write(client_socket, response_header, strlen(response_header));
        char index_file[BUFFER_SIZE];
        snprintf(index_file, sizeof(index_file), "%s/index.html", DOCS_DIR);
        FILE *file = fopen(index_file, "r");
        
        if (file) {
            // Serve index.html
            char file_buffer[BUFFER_SIZE];
            while (fgets(file_buffer, sizeof(file_buffer), file)) {
                write(client_socket, file_buffer, strlen(file_buffer));
            }
            fclose(file);
        } else {
            DIR *dir = opendir(DOCS_DIR);
            struct dirent *entry;
            char dir_list[BUFFER_SIZE] = "<html><body><h2>Directory listing:</h2><ul>";
            write(client_socket, dir_list, strlen(dir_list));
            
            while ((entry = readdir(dir)) != NULL) {
                char item[BUFFER_SIZE];
                snprintf(item, sizeof(item), "<li>%s</li>", entry->d_name);
                write(client_socket, item, strlen(item));
            }
            closedir(dir);
            
            const char *footer = "</ul><hr><center><h3>o3WebS</h3><h4>Copyright (c) openw3rk INVENT</h4></center></body></html>";
            write(client_socket, footer, strlen(footer));
        }
    }

    close(client_socket);
    return NULL;
}

void start_server(int port) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size = sizeof(client_addr);
    int client_socket;
    pthread_t client_thread;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Failed to bind socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) == -1) {
        perror("Failed to listen on socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("o3WebS: Server started on port %d\n", port);

    while (server_running) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_socket == -1) {
            perror("Failed to accept connection");
            continue;
        }

        int *client_socket_ptr = malloc(sizeof(int));
        *client_socket_ptr = client_socket;
        pthread_create(&client_thread, NULL, handle_client, client_socket_ptr);
        pthread_detach(client_thread);
    }

    close(server_socket);
}

void stop_server() {
    if (server_socket != -1) {
        close(server_socket);
    }
    printf("o3WebS - Copyright openw3rk INVENT\no3WebS: Server stopped\n");
}

void daemonize() {
    pid_t pid, sid;

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    umask(0);

    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }

    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
}

void create_docs_dir() {
    struct stat st = {0};
    if (stat(DOCS_DIR, &st) == -1) {
        if (mkdir(DOCS_DIR, 0755) == -1) {
            perror("o3WebS: Failed to create /var/o3docs directory");
            exit(EXIT_FAILURE);
        }
        printf("Created directory: %s\n", DOCS_DIR);
    }
}

void write_pid_file() {
    FILE *pid_file = fopen(PID_FILE, "w");
    if (pid_file == NULL) {
        perror("Failed to open PID file");
        exit(EXIT_FAILURE);
    }
    fprintf(pid_file, "%d\n", getpid());
    fclose(pid_file);
}

void remove_pid_file() {
    unlink(PID_FILE);
}

void stop_running_server() {
    FILE *pid_file = fopen(PID_FILE, "r");
    if (pid_file == NULL) {
        perror("o3WebS: Could not open PID file. Is the server running?");
        exit(EXIT_FAILURE);
    }

    int pid;
    fscanf(pid_file, "%d", &pid);
    fclose(pid_file);

    if (kill(pid, SIGTERM) == -1) {
        perror("o3WebS: Failed to stop server");
        exit(EXIT_FAILURE);
    } else {
        printf("o3WebS: Server stopped\n");
    }
}

int main(int argc, char *argv[]) {
    int port = PORT;

    if (argc < 3) {
        fprintf(stderr, "\nWelcome to o3WebS\no3WebS - Copyright (c) openw3rk INVENT\n\nThe directory for hosting files is /var/o3docs\n\nUsage: %s -server --start [--stop] [-port]\n\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "-server") == 0) {
        if (strcmp(argv[2], "--start") == 0) {
            if (argc == 4 && argv[3][0] == '-') {
                port = atoi(argv[3] + 1);
            }

            daemonize();

            create_docs_dir();
            write_pid_file();

            start_server(port);
            remove_pid_file();

        } else if (strcmp(argv[2], "--stop") == 0) {
            stop_running_server();
        } else {
            fprintf(stderr, "Invalid option: %s\n", argv[2]);
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
