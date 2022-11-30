// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_LIST_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_LIST_H_

#include <algorithm>
#include <vector>

#include "chrome/browser/media/router/discovery/discovery_network_info.h"

namespace media_router {

// Returns a list of information about each network to which the host is
// connected, stable-sorted by network ID (stable relative to the order they are
// returned by the OS during enumeration).
std::vector<DiscoveryNetworkInfo> GetDiscoveryNetworkInfoList();

// Stable sort a sequence of DiscoveryNetworkInfo objects given by the iterators
// [first, last).
template <typename InputIt>
void StableSortDiscoveryNetworkInfo(InputIt first, InputIt last) {
  std::stable_sort(
      first, last,
      [](const DiscoveryNetworkInfo& info1, const DiscoveryNetworkInfo& info2) {
        return info1.network_id < info2.network_id;
      });
}

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_LIST_H_
