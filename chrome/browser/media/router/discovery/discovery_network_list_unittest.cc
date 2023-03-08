// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/discovery_network_list.h"

#include <iterator>
#include <set>

#include "base/ranges/algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

TEST(DiscoveryNetworkListTest, GetDiscoveryNetworkInfoList) {
  auto network_ids = GetDiscoveryNetworkInfoList();
  for (const auto& network_id : network_ids) {
    // We can't mock out the OS layer used by GetDiscoveryNetworkIdList, so
    // instead just check that each returned interface name is non-empty.
    EXPECT_FALSE(network_id.name.empty());
  }

  // Also check that at most one ID is returned per interface name.
  std::set<std::string> interface_name_set;
  base::ranges::transform(network_ids,
                          std::insert_iterator<std::set<std::string>>{
                              interface_name_set, end(interface_name_set)},
                          &DiscoveryNetworkInfo::name);

  EXPECT_EQ(interface_name_set.size(), network_ids.size());
}

TEST(DiscoveryNetworkListTest, StableSortDiscoveryNetworkInfoUnique) {
  std::vector<DiscoveryNetworkInfo> network_info({
      {"wlan0", "ssid0"}, {"wlan1", "ssid1"}, {"eth0", "de:ad:be:ef:00:11"},
  });
  std::vector<DiscoveryNetworkInfo> sorted_network_info({
      {"eth0", "de:ad:be:ef:00:11"}, {"wlan0", "ssid0"}, {"wlan1", "ssid1"},
  });

  StableSortDiscoveryNetworkInfo(network_info.begin(), network_info.end());

  EXPECT_EQ(sorted_network_info, network_info);
}

TEST(DiscoveryNetworkListTest, StableSortDiscoveryNetworkInfoDuplicates) {
  std::vector<DiscoveryNetworkInfo> network_info({
      {"wlan1", "ssid0"}, {"eth0", "de:ad:be:ef:00:11"}, {"wlan0", "ssid0"},
  });
  std::vector<DiscoveryNetworkInfo> sorted_network_info({
      {"eth0", "de:ad:be:ef:00:11"}, {"wlan1", "ssid0"}, {"wlan0", "ssid0"},
  });

  StableSortDiscoveryNetworkInfo(network_info.begin(), network_info.end());

  EXPECT_EQ(sorted_network_info, network_info);
}

}  // namespace media_router
