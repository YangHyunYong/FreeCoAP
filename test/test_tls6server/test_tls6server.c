/*
 * Copyright (c) 2010 Keith Cullen.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 *  @file test_tls6server.c
 *
 *  @brief Source file for the FreeCoAP TLS/IPv6 server test application
 */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "tls6sock.h"
#include "tls.h"
#include "sock.h"

#define IO
#define REHANDSHAKE
#define TRUST_FILE_NAME  "root_client_cert.pem"
#define CERT_FILE_NAME   "server_cert.pem"
#define KEY_FILE_NAME    "server_privkey.pem"
#define PORT             "9999"
#define BUF_SIZE         (1 << 4)
#define TIMEOUT          30
#define BACKLOG          10
#define NUM_ITER         2

/* ignore broken pipe signal, i.e. don't terminate if client terminates */
static void set_signal()
{
    struct sigaction sa = {{0}};
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPIPE, &sa, NULL);
}

static int server()
{
    tls6ssock_t ss = {0};
    tls6sock_t s = {0};
    char addr_str[INET6_ADDRSTRLEN] = {0};
    char in_buf[BUF_SIZE] = {0};
    char out_buf[BUF_SIZE] = {0};
    int ret = 0;
    int i = 0;

    ret = tls6ssock_open(&ss, PORT, TIMEOUT, BACKLOG);
    if (ret != SOCK_OK)
    {
        return ret;
    }

    printf("...ready\n");

    ret = tls6ssock_accept(&ss, &s);
    if (ret != SOCK_OK)
    {
        tls6ssock_close(&ss);
        return ret;
    }

    printf("accept\n");
    if (tls6sock_is_resumed(&s))
        printf("session resumed\n");
    else
        printf("session not resumed\n");
    tls6sock_get_addr_string(&s, addr_str, sizeof(addr_str));
    printf("addr: %s\n", addr_str);
    printf("port: %d\n", tls6sock_get_port(&s));

    ret = tls6sock_read_full(&s, in_buf, BUF_SIZE);
    if (ret <= 0)
    {
        tls6sock_close(&s);
        tls6ssock_close(&ss);
        return ret;
    }

#ifdef IO
    printf("in_buf[] =");
    for (i = 0; i < BUF_SIZE; i++)
        printf(" %d", in_buf[i]);
    printf("\n");
#endif

#ifdef REHANDSHAKE
    /* re-handshake */
    ret = tls6sock_rehandshake(&s);
    if (ret != SOCK_OK)
    {
        tls6sock_close(&s);
        tls6ssock_close(&ss);
        return ret;
    }
#endif

    for (i = 0; i < BUF_SIZE; i++)
    {
        out_buf[i] = -in_buf[i];
    }

#ifdef IO
    printf("out_buf[] =");
    for (i = 0; i < BUF_SIZE; i++)
        printf(" %d", out_buf[i]);
    printf("\n");
#endif

    ret = tls6sock_write_full(&s, out_buf, BUF_SIZE);
    if (ret <= 0)
    {
        tls6sock_close(&s);
        tls6ssock_close(&ss);
        return ret < 0 ? ret : SOCK_WRITE_ERROR;
    }

    tls6sock_close(&s);
    tls6ssock_close(&ss);
    return SOCK_OK;
}

int main()
{
    time_t start = 0;
    time_t end = 0;
    int ret = 0;
    int i = 0;

    /* initialise signal handling */
    set_signal();

    ret = tls_init();
    if (ret != SOCK_OK)
    {
        fprintf(stderr, "Error: %s\n", sock_strerror(ret));
        return EXIT_FAILURE;
    }

    ret = tls_server_init(TRUST_FILE_NAME, CERT_FILE_NAME, KEY_FILE_NAME);
    if (ret != SOCK_OK)
    {
        tls_deinit();
        fprintf(stderr, "Error: %s\n", sock_strerror(ret));
        return EXIT_FAILURE;
    }

    for (i = 0; i < NUM_ITER; i++)
    {
        start = time(NULL);
        ret = server();
        end = time(NULL);
        if (ret != SOCK_OK)
        {
            tls_server_deinit();
            tls_deinit();
            fprintf(stderr, "Error: %s\n", sock_strerror(ret));
            return EXIT_FAILURE;
        }
        printf("Result: %s\n", sock_strerror(ret));
        printf("Time: %d sec\n", (int)(end - start));
    }

    tls_server_deinit();
    tls_deinit();
    return EXIT_SUCCESS;
}
