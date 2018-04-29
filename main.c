#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <malloc.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdlib.h>

char* getHostname(char* URL){
    char* pnt = strchr(URL, '/');
    if(pnt == NULL){
        return URL;
    }
    int poz = pnt-URL;
    if(poz < 0){
        return URL;
    }
    char *host;
    host = malloc(poz);
    strncpy(host, URL, poz);
    host[poz] = NULL;
    return host;
}

char* getPath(char* URL){
    char* pnt = strchr(URL, '/');
    if(pnt == NULL){
        return "/";
    }
    int poz = pnt-URL;
    if(poz < 0){
        return "/";
    }
    return URL+poz;
}

char* cdir = "./downloads/";

char buffer[50000];

int getSocket(char* URL){
    int rv,sockfd;
    struct addrinfo hints,*servinfo,*p;
    memset(&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;

    char* hostName = getHostname(URL);

    if ((rv = getaddrinfo(hostName, "80", &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }
    char host[1000];

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }
        getnameinfo(p->ai_addr, p->ai_addrlen, host, sizeof(host), NULL, 0, NI_NUMERICHOST);
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
            perror("connect");
            close(sockfd);
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "failed to connect\n");
        return -1;
    }

    freeaddrinfo(servinfo);
    return sockfd;
}

bool validateFile(char* filename){
    FILE* file = fopen(filename, "r");
    fread(buffer, 50, 1, file);
    fclose(file);
    if(strstr(buffer, "200 OK")){
        return true;
    }

    return false;
}

int minim(int a, int b){
    if(a <= b){
        return a;
    }
    return b;
}

bool receiveReply(int sockfd ,FILE* file, int timeout, char* URL) {
    char server_reply[10005],aux[10005];
    int total_len = 0, received_len, received_len2;
    struct timeval begin , now;
    double timediff;

    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    gettimeofday(&begin , NULL);
    bool status = false, good = true, found = false;
    char *pnt,*poc;
    int poz, poz2,i;

    int length;
    bool are = false;
    int offset;

    while(1) // GET HEADERS
    {
        gettimeofday(&now , NULL);

        timediff = (now.tv_sec - begin.tv_sec);

        if(timediff > timeout*2)
        {
            break;
        }

        memset(server_reply ,0 , sizeof(server_reply));  //clear the variable
        bzero(server_reply, sizeof(server_reply));
        int received = recv(sockfd, server_reply , 1000 , 0);
        if( received <= 0 ){
            usleep(100000);
        }
        else
        {
           // printf("%s\n\n", server_reply);
            if(strstr(server_reply, "200 OK")){
                status = true;
            }
            char *pk = strstr(server_reply, "\r\n\r\n");
            if(pk == NULL){
                return false;
            }
            int fhd = pk - server_reply;
            char *pw = strstr(server_reply, "Content-Length:");
             offset = 4;
            if(pw == NULL){ // GET CONTENT-LENGTH VALUE
               while((*(pk+offset) == '\r') || (*(pk+offset) == '\n')){
                   ++offset;
               }
                while((*(pk+offset) != '\r') && (*(pk+offset) != '\n')){
                    ++offset;
                }
                while((*(pk+offset) == '\r') || (*(pk+offset) == '\n')){
                    ++offset;
                }
               fwrite(pk+offset, received-(fhd+offset), 1, file);
                break;
            }
            int pz = pw-server_reply;
            if(pz > fhd){
                while((*(pk+offset) == '\r') || (*(pk+offset) == '\n')){
                    ++offset;
                }
                while((*(pk+offset) != '\r') && (*(pk+offset) != '\n')){
                    ++offset;
                }
                while((*(pk+offset) == '\r') || (*(pk+offset) == '\n')){
                    ++offset;
                }
               fwrite(pk+offset, received-(fhd+offset), 1, file);
                break;
            }
            char ch;
            length = 0;
            while(ch = (*pw++)){ // CALCULATE CONTENT-LENGTH VALUE
                if(ch >= '0' && ch <= '9'){
                    length=length*10+(ch-'0');
                }
                if(ch == '\n'){
                    break;
                }
            }
            fwrite(pk+4 , received-fhd-4 , 1, file);
            length-=(received-(fhd+4));
            are = true;
            break;
        }
    }
    int to_take;
     good = true;
    received_len = 0;

    while(1) // GET CONTENT
    {
        gettimeofday(&now , NULL);

        timediff = (now.tv_sec - begin.tv_sec);

        if(timediff > timeout*2)
        {

            if(good){
                break;
            }
            if(are){
                break;
            }
            offset = received_len2-1;
            if(strlen(aux) <= offset){
                break;
            }
            // remove html footer
            while((aux[offset] == '\r') || (aux[offset] == '\n')){
                --offset;
            }
            while((aux[offset] != '\r') && (aux[offset] != '\n')){
                --offset;
            }
            while((aux[offset] == '\r') || (aux[offset] == '\n')){
                --offset;
            }

            fwrite(aux , offset+1, 1, file);
            break;
        }
        memset(server_reply ,0 , sizeof(server_reply));
        bzero(server_reply, sizeof(server_reply));
        to_take = 10000;
        if(are){
            to_take = minim(10000, length);
        }
        int received_len = recv(sockfd, server_reply , 10000 , 0);

        if( received_len <= 0 ){
            usleep(100000);
        }
        else
        {
            if(!are){
                if(!good){
                    fwrite(aux , received_len2, 1, file);
                }
                else{
                    good = false;
                }
            }
            length-=received_len;
            total_len += received_len;
            if(are) {
                fwrite(server_reply, received_len, 1, file);
            }
            if(!are){
                received_len2 = received_len;
                memset(aux, 0, sizeof(aux));
                strcpy(aux, server_reply);
            }
            if(are && length <= 0){
                break;
            }
            gettimeofday(&begin , NULL);
        }
    }
    return status;
}

bool getFile(char* filename, char* URL, int sockfd){
    char* hostName  = getHostname(URL);
    char* path      = getPath(URL);

    char* message = malloc(strlen(hostName)+strlen(path)+100);

    message[0] = NULL;
    strcat(message, "GET ");
    strcat(message, path);
    strcat(message, " HTTP/1.1\r\nHost: ");
    strcat(message, hostName);
    strcat(message, "\r\n\r\n");

    if( send(sockfd , message , strlen(message) , 0) < 0) {
        return;
    }

    remove(filename);

    FILE* file = fopen(filename, "ab");
    if(file == NULL){
        return false;
    }

    bool status = receiveReply(sockfd,file, 4, URL);
    fclose(file);
    if(!status){
        //remove(filename);
    }
    return status;
}

bool isLetter(char ch){
    if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')){
        return true;
    }
    return false;
}

char targetName[100];

char* getExtension(char* URL){
    char* path = getPath(URL);
    char* pnt;
    int poz = -1,poz2,p3;
    do{
        pnt = strstr(path+poz+1, ".");
        if(pnt != NULL){
            poz = pnt-path;
        }


    }while(pnt);

    if(!strcmp(path, "/")){
        strcpy(targetName, "index");
        return "html";
    }
    if(path[strlen(path)-1] == '/'){
        path[strlen(path)-1] = NULL;
    }
    if(poz < 0){
        p3 = strlen(path)-1;
        while(p3 >= 0 && path[p3--] != '/');
        ++p3;
        strncpy(targetName, path+p3+1, strlen(path)-p3-1);
        return "html";
    }
    poz2 = 0,p3 = poz;
    while(p3 >= 0 && path[p3--] != '/');
    ++p3;
    strncpy(targetName, path+p3+1, poz-p3-1);

    int nr = 1;
    int poz5 = poz;
    poz++;
    while(poz < strlen(path) && isLetter(path[poz])){
        ++poz;++nr;
    }
    poz--;
    char* ext = malloc(nr+1);
    memset(ext, 0, sizeof(ext));
    strncpy(ext, path+poz5+1, nr);
    for(int i = 0; i < strlen(ext); ++i){
        if(!isLetter(ext[i])){
            return "html";
        }
    }
    return ext;
}

char* getURL(char* URL){

    char *newURL = malloc(strlen(URL));

    memset(newURL, 0, sizeof(newURL));

    if(strlen(URL)>=11 && URL[0] == 'h' && URL[1] == 't' && URL[2] == 't' && URL[3] == 'p' && URL[4] == ':'
            && URL[5] == '/' && URL[6] == '/'){
        if(URL[7] == 'w' && URL[8] == 'w' && URL[9] == 'w' && URL[10] == '.'){
            free(newURL);
            return URL+7;
        }
        strcat(newURL, "www.");
        strcat(newURL, URL+7);
        return newURL;
    }
    if(strlen(URL)>=12 && URL[0] == 'h' && URL[1] == 't' && URL[2] == 't' && URL[3] == 'p' && URL[4] == 's'
       && URL[5] == ':' && URL[6] == '/' && URL[7] == '/'){
        if(URL[8] == 'w' && URL[9] == 'w' && URL[10] == 'w' && URL[11] == '.'){
            free(newURL);
            return URL+8;
        }
        strcat(newURL, "www.");
        strcat(newURL, URL+8);
        return newURL;
    }
    return URL;
}

unsigned long getHash(char* filename) {
    unsigned long hash = 5381;
    int c;
    FILE* file = fopen(filename, "r+");
    if(file == NULL){
        return 0;
    }
    while (fread(buffer, 1, 1, file) > 0) {
        c = buffer[0];
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}
unsigned long simpleHash(unsigned char *str) {
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c;

    return hash;
}

unsigned long visited[1000];
int number, pages;
char dir_id[100];
char cmd[1001], bf[1005], fn[1005];

int the_one;
bool from_cache;

bool checkCache(unsigned long hash){
    FILE* file = fopen("./cachingSystem/auxiliary/current.txt", "r+");
    int nr = 0;
    while (fread(buffer, 1, 1, file) > 0) {
        nr = nr*10+(buffer[0]-'0');
    }
    int found = 0;
    FILE* file2;
    unsigned long chash=0;
    for(int i = 1; i <= nr; ++i){
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "./cachingSystem/%d/hash_is_here.txt", i);
        file2 = fopen(buffer, "r+");
        if(file2 == NULL){
            continue;
        }
        chash=0;
        while (fread(buffer, 1, 1, file2) > 0){
            chash = chash*10+(buffer[0]-'0');
        }
        if(chash == hash){
            found = true;
            memset(buffer, 0, sizeof(buffer));
            sprintf(buffer, "./cachingSystem/%d", i);
            fclose(file2);
            break;
        }
        fclose(file2);
    }

    if(!found){
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "./cachingSystem/%d", nr+1);
        mkdir(buffer, 0700);
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "./cachingSystem/%d/hash_is_here.txt", nr+1);
        remove(buffer);
        FILE* file2 = fopen(buffer, "ab");
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "%lu", hash);
        fwrite(buffer, 1, strlen(buffer), file2);
        fclose(file2);
        fclose(file);
        remove("./cachingSystem/auxiliary/current.txt");
        FILE* file = fopen("./cachingSystem/auxiliary/current.txt", "ab");
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "%d", nr+1);
        fwrite(buffer, 1, strlen(buffer), file);
        fclose(file);
        memset(dir_id, 0, sizeof(dir_id));
        sprintf(dir_id, "%d", nr+1);
        the_one = nr+1;
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "mkdir ./cachingSystem/%d/level0", nr+1);
        system(buffer);
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "cp %s ./cachingSystem/%d/level0", cmd, nr+1);
        system(buffer);
        return false;
    }
    DIR* dir = opendir("./RESULTS");
    if (dir) {
        system("rm -r RESULTS/");
        close(dir);
    }
    system("mkdir RESULTS");

    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd, "cp -R %s ./RESULTS", buffer);
    system(cmd);

    fclose(file);
    return true;
}

bool success;

bool Valid(char* URL){
    unsigned long cHash = simpleHash(URL);
    for(int i = 1; i <= pages; ++i){
        if(cHash == visited[i]){
            return false;
        }
    }
    return true;
}

void parseIndex(char* filename, char* URL, int deep){

    FILE* file = fopen(filename, "r");
    if(file == NULL){
        return;
    }

    char  tx[500], parse[5010];

    char* pnt;
    char* target = malloc(150);
    char* gh;
    int poz = 0, fpoz, ind;
    int cnt = 0;
    char* hostName = getHostname(URL);

    memset(parse, 0, sizeof(parse));
    while(fread(parse, 1, 5000, file) > 0){
        if(cnt > 9)continue;
        poz=0;
        do {
            pnt = strstr(parse + poz + 1, "href=\"");
            if (pnt == NULL) {
                break;
            }
            poz = pnt - parse;

            gh = strstr(parse + poz + 6, "\"");
            if(gh == NULL){
                break;
            }

            fpoz = gh - parse;
            memset(target, 0, sizeof(target));
            strncpy(target, parse + poz, fpoz - poz + 1);
            target[fpoz - poz + 1] = NULL;

            if (*(target + 6) == 'h' || *(target + 6) == 'w') {
                strcpy(tx, target + 6);
            } else {
                if(*(target+6) == '/')
                    snprintf(tx, 100, "%s%s", hostName, target + 6);
                else {
                    ind = 6;
                    while(*(target+ind)=='.'){++ind;}
                    if(*(target+ind) == '/'){
                        snprintf(tx, 100, "%s%s", hostName, target + ind);
                    }else {
                        snprintf(tx, 100, "%s/%s", hostName, target + ind);
                    }
                }
            }
            tx[strlen(tx) - 1] = NULL;
            if (Valid(tx)) {
                //printf("%s\n", tx);
                myDownloader(tx, deep + 1);
                ++cnt;
            }

        } while (poz >= 0);
        cnt = poz = 0;
        do {
            pnt = strstr(parse + poz + 1, "img src=\"");
            if (pnt == NULL) {
                break;
            }
            poz = pnt - parse;

            gh = strstr(parse + poz + 9, "\"");
            if(gh == NULL){
                break;
            }

            fpoz = gh - parse;
            memset(target, 0, sizeof(target));
            strncpy(target, parse + poz, fpoz - poz + 1);
            target[fpoz - poz + 1] = NULL;

            if (*(target + 9) == 'h' || *(target + 9) == 'w') {
                strcpy(tx, target + 9);
            } else {
                if(*(target+9) == '/')
                    snprintf(tx, 100, "%s%s", hostName, target + 9);
                else
                    snprintf(tx, 100, "%s/%s", hostName, target + 9);
            }
            tx[strlen(tx) - 1] = NULL;
            if (Valid(tx)) {
                //printf("%s\n", tx);
                myDownloader(tx, deep + 1);
                ++cnt;
            }

        } while (poz >= 0);
    }
    fclose(file);
}

int deep_top;
char drp[1005], directory[1005];

void myDownloader(char* URL, int deep){

    if(deep > deep_top){
        return;
    }
    number++;
    strcpy(URL, getURL(URL));

    if(!Valid(URL)){
        return;
    }
    printf("TRYING TO DOWNLOAD:  %s  FROM LEVEL %d    ", URL, deep);
    int sockfd = getSocket(URL);

    if(sockfd >= 0) {
        visited[++pages] = simpleHash(URL);
        char* ext = getExtension(URL);
        memset(drp, 0, sizeof(drp));
        sprintf(drp, "./cachingSystem/%s/level%d", dir_id, deep);
            memset(buffer, 0, sizeof(buffer));
            sprintf(buffer, "mkdir -p %s", drp);
            system(buffer);

        if(deep == 0 && (ext == NULL || !strcmp(ext, "html"))) {
            sprintf(directory, "%s/index.html", drp);
            memset(fn, 0, sizeof(fn));
            strcpy(fn, "index.html");
        }
        else{
            sprintf(directory, "%s/%s.%s", drp, targetName, ext);
            memset(fn, 0, sizeof(fn));
            sprintf(fn, "%s.%s", targetName, ext);
        }
        bool downloaded = getFile(directory, URL, sockfd);
        if(!downloaded){
            printf("FAILED\n");
            close(sockfd);
            return;
        }
        else{
            printf("SUCCESS\n");
        }
        memset(cmd, 0, sizeof(cmd));
        strcpy(cmd, directory);

        if(!strcmp(ext, "html")||!strcmp(ext, "php")) {
            if(number == 1){
                if(!downloaded){
                    close(sockfd);
                    return;
                }
                unsigned long h = getHash(directory);

                if(h > 0){
                    success = true;
                    from_cache = checkCache(h);
                    if(from_cache == true){
                        return;
                    }
                }
                else{
                    printf("Error calculating hash...\n");
                    close(sockfd);
                    return;
                }
            }
            if(deep == deep_top){
                return;
            }
            parseIndex(directory, URL, deep);
        }else{

            if(number == 1){
                if(!downloaded){
                    close(sockfd);
                    return;
                }
                unsigned long h = getHash(directory);
                if(h > 0){
                    success = true;
                    from_cache = checkCache(h);
                    if(from_cache == true){
                        return;
                    }
                }
                else{
                    printf("FAILED\n");
                    printf("Error calculating hash...\n");
                    close(sockfd);
                    return;
                }
            }
        }
        close(sockfd);
    }else{
        printf("FAILED\n");
    }
}

void process_results(){
    DIR* dir = opendir("./RESULTS");
    if (dir) {
        system("rm -r RESULTS/");
        close(dir);
    }

    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "cp -R ./cachingSystem/%d .", the_one);
    system(buffer);
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "mv ./%d ./RESULTS", the_one);
    system(buffer);
    remove(".//RESULTS/hash_is_here.txt");
}

int main(int argc , char *argv[])
{
    printf("-----------------Starting-----------------\n\n\n");
    if(argc != 3){
        printf("Invalid number of arguments.\n");
        return 0;
    }
    strcpy(dir_id, "auxiliary");
    deep_top =  atoi(argv[2]);

    myDownloader(argv[1], 0);
    if(!success){
        printf("\n\nDownload failed.\n");
        return 0;
    }
    printf("\n\nDownloaded successful. All files are in 'RESULTS' directory\n\n");
    if(from_cache){
        printf("CACHING SYSTEM WAS USED!\n\n");
        return 0;
    }
    printf("ADDED TO CACHE!\n\n");
    process_results();
    return 0;
}