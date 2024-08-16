// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/media/router/discovery/discovery_network_list.h"

#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <algorithm>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/discovery/discovery_network_list_wifi.h"
#include "net/base/net_errors.h"

#if !BUILDFLAG(IS_MAC)
#include <netpacket/packet.h>
#else
#include <net/if_dl.h>
#endif

namespace media_router {
namespace {

#if !BUILDFLAG(IS_MAC)
using sll = struct sockaddr_ll;
#define SOCKET_ARP_TYPE(s) ((s)->sll_hatype)
#define SOCKET_ADDRESS_LEN(s) ((s)->sll_halen)
#define SOCKET_ADDRESS(s) ((s)->sll_addr)
#else  // BUILDFLAG(IS_MAC)
#define AF_PACKET AF_LINK
using sll = struct sockaddr_dl;
#define SOCKET_ARP_TYPE(s) ((s)->sdl_type)
#define SOCKET_ADDRESS_LEN(s) ((s)->sdl_alen)
#define SOCKET_ADDRESS(s) (LLADDR(s))
#endif

void GetDiscoveryNetworkInfoListImpl(
    const struct ifaddrs* if_list,
    std::vector<DiscoveryNetworkInfo>* network_info_list) {
  std::string ssid;
  for (; if_list != nullptr; if_list = if_list->ifa_next) {
    if ((if_list->ifa_flags & IFF_RUNNING) == 0 ||
        (if_list->ifa_flags & IFF_UP) == 0) {
      continue;
    }

    const struct sockaddr* addr = if_list->ifa_addr;
    if (addr == nullptr) {
      continue;
    }
    if (addr->sa_family != AF_PACKET) {
      continue;
    }
    std::string name(if_list->ifa_name);
    if (name.empty()) {
      continue;
    }

    // |addr| will always be sockaddr_ll/sockaddr_dl when |sa_family| ==
    // AF_PACKET.
    const auto* ll_addr = reinterpret_cast<const sll*>(addr);
    // ARPHRD_ETHER is used to test for Ethernet, as in IEEE 802.3 MAC protocol.
    // This spec is used by both wired Ethernet and wireless (e.g. 802.11).
    // ARPHRD_IEEE802 is used to test for the 802.2 LLC protocol of Ethernet.
    if (SOCKET_ARP_TYPE(ll_addr) != ARPHRD_ETHER &&
        SOCKET_ARP_TYPE(ll_addr) != ARPHRD_IEEE802) {
      continue;
    }

    if (MaybeGetWifiSSID(name, &ssid)) {
      network_info_list->push_back({name, ssid});
      continue;
    }

    if (SOCKET_ADDRESS_LEN(ll_addr) == 0) {
      continue;
    }

    network_info_list->push_back(
        {name, base::HexEncode(reinterpret_cast<const unsigned char*>(
                                   SOCKET_ADDRESS(ll_addr)),
                               SOCKET_ADDRESS_LEN(ll_addr))});
  }
}

}  // namespace

std::vector<DiscoveryNetworkInfo> GetDiscoveryNetworkInfoList() {
  std::vector<DiscoveryNetworkInfo> network_ids;

  struct ifaddrs* if_list;
  if (getifaddrs(&if_list)) {
    return network_ids;
  }

  GetDiscoveryNetworkInfoListImpl(if_list, &network_ids);
  StableSortDiscoveryNetworkInfo(network_ids.begin(), network_ids.end());
  freeifaddrs(if_list);
  return network_ids;
}

}  // namespace media_router
