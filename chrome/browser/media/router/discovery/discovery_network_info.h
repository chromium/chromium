// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_INFO_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_INFO_H_

#include <string>

namespace media_router {

// Represents a single network interface that can be used for local discovery.
struct DiscoveryNetworkInfo {
 public:
  DiscoveryNetworkInfo();
  DiscoveryNetworkInfo(const std::string& name, const std::string& network_id);
  ~DiscoveryNetworkInfo();

  DiscoveryNetworkInfo(const DiscoveryNetworkInfo&);
  DiscoveryNetworkInfo& operator=(const DiscoveryNetworkInfo&);

  bool operator==(const DiscoveryNetworkInfo&) const;
  bool operator!=(const DiscoveryNetworkInfo&) const;

  // The name of the network interface.  e.g. eth0, wlan0
  std::string name;
  // Some form of identifier for the network to which this interface is
  // connected.  For WiFi, we assume the associated SSID identifies the
  // connected network.  For Ethernet, we assume that the network remains the
  // same for the interface (until disconnected), so we use the interface's MAC
  // address to identify the network.
  std::string network_id;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_INFO_H_
