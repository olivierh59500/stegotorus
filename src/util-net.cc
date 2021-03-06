/* Copyright 2011 Nick Mathewson, George Kadianakis
 * Copyright 2011, 2012 SRI International
 * See LICENSE for other credits and copying information
 */

#include "util.h"

#include <event2/dns.h>

#include <errno.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#ifdef AF_LOCAL
#include <sys/un.h>
#endif

#ifdef _WIN32
const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt) {
	if (af == AF_INET) {
		struct sockaddr_in in;

		memset(&in, 0, sizeof(in));
		in.sin_family = AF_INET;
		memcpy(&in.sin_addr, src, sizeof(struct in_addr));
		getnameinfo(	(struct sockaddr *)&in, sizeof(struct sockaddr_in),
				dst, cnt, NULL, 0, NI_NUMERICHOST);

		return dst;
	} else if (af == AF_INET6) {
		struct sockaddr_in6 in;

		memset(&in, 0, sizeof(in));
		in.sin6_family = AF_INET6;
		memcpy(&in.sin6_addr, src, sizeof(struct in_addr6));
		getnameinfo(	(struct sockaddr *)&in, sizeof(struct sockaddr_in6),
				dst, cnt, NULL, 0, NI_NUMERICHOST);
		return dst;
	}

	return NULL; 
}

int inet_pton(int af, const char *src, void *dst) 
{ 
	struct addrinfo hints, *res, *ressave;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = af;

	if (getaddrinfo(src, NULL, &hints, &res) != 0)
	{
		log_debug("couldn't resolve host %s\n", src);
		return -1;
	}

	ressave = res;

	while (res)
	{
		memcpy(dst, res->ai_addr, res->ai_addrlen);
		res = res->ai_next;
	}

	freeaddrinfo(ressave);
	return 0;
}
#endif

/**
   Accepts a string 'address' of the form ADDRESS:PORT and attempts to
   parse it into an 'evutil_addrinfo' structure.

   If 'nodns' is set it means that 'address' was an IP address.
   If 'passive' is set it means that the address is destined for
   listening and not for connecting.

   If no port was given in 'address', we set 'default_port' as the
   port.
*/
struct evutil_addrinfo *
resolve_address_port(const char *address, int nodns, int passive,
                     const char *default_port)
{
  struct evutil_addrinfo *ai = NULL;
  struct evutil_addrinfo ai_hints;
  int ai_res, ai_errno;
  char *a = xstrdup(address), *cp;
  const char *portstr;

  if ((cp = strchr(a, ':'))) {
    portstr = cp+1;
    *cp = '\0';
  } else if (default_port) {
    portstr = default_port;
  } else {
    log_debug("error in address %s: port required", address);
    free(a);
    return NULL;
  }

  memset(&ai_hints, 0, sizeof(ai_hints));
  ai_hints.ai_family = AF_UNSPEC;
  ai_hints.ai_socktype = SOCK_STREAM;
  ai_hints.ai_flags = EVUTIL_AI_ADDRCONFIG | EVUTIL_AI_NUMERICSERV;
  if (passive)
    ai_hints.ai_flags |= EVUTIL_AI_PASSIVE;
  if (nodns)
    ai_hints.ai_flags |= EVUTIL_AI_NUMERICHOST;

  ai_res = evutil_getaddrinfo(a, portstr, &ai_hints, &ai);
  ai_errno = errno;

  free(a);

  if (ai_res) {
    if (ai_res == EVUTIL_EAI_SYSTEM)
      log_warn("error resolving %s: %s [%s]",
               address, evutil_gai_strerror(ai_res), strerror(ai_errno));
    else
      log_warn("error resolving %s: %s", address, evutil_gai_strerror(ai_res));

    if (ai) {
      evutil_freeaddrinfo(ai);
      ai = NULL;
    }
  } else if (ai == NULL) {
    log_warn("address resolution failed for %s", address);
  }

  return ai;
}

char *
printable_address(struct sockaddr *addr, socklen_t addrlen)
{
  char apbuf[INET6_ADDRSTRLEN + 8]; /* []:65535 is 8 characters */

  switch (addr->sa_family) {
  case AF_INET: {
    char abuf[INET6_ADDRSTRLEN];
    struct sockaddr_in *sin = (struct sockaddr_in*)addr;
    log_assert(((unsigned int)addrlen) >= sizeof(struct sockaddr_in));
    if (!inet_ntop(AF_INET, &sin->sin_addr, abuf, INET6_ADDRSTRLEN))
      break;
    xsnprintf(apbuf, sizeof apbuf, "%s:%d", abuf, ntohs(sin->sin_port));
    return xstrdup(apbuf);
  }

  case AF_INET6: {
    char abuf[INET6_ADDRSTRLEN];
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)addr;
    log_assert(((unsigned int)addrlen) >= sizeof(struct sockaddr_in6));
    if (!inet_ntop(AF_INET, &sin6->sin6_addr, abuf, INET6_ADDRSTRLEN))
      break;
    xsnprintf(apbuf, sizeof apbuf, "[%s]:%d", abuf, ntohs(sin6->sin6_port));
    return xstrdup(apbuf);
  }

#ifdef AF_LOCAL
  case AF_LOCAL:
    return xstrdup(((struct sockaddr_un*)addr)->sun_path);
#endif
  default:
    break;
  }

  xsnprintf(apbuf, sizeof apbuf, "<addr family %d>", addr->sa_family);
  return xstrdup(apbuf);
}

static struct evdns_base *the_evdns_base = NULL;

struct evdns_base *
get_evdns_base(void)
{
  return the_evdns_base;
}

int
init_evdns_base(struct event_base *base)
{
  the_evdns_base = evdns_base_new(base, 1);
  return the_evdns_base == NULL ? -1 : 0;
}
