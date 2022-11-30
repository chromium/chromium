// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/discovery_network_info.h"

namespace media_router {

DiscoveryNetworkInfo::DiscoveryNetworkInfo() {}

DiscoveryNetworkInfo::DiscoveryNetworkInfo(const std::string& name,
                                           const std::string& network_id)
    : name(name), network_id(network_id) {}

DiscoveryNetworkInfo::~DiscoveryNetworkInfo() {}

DiscoveryNetworkInfo::DiscoveryNetworkInfo(const DiscoveryNetworkInfo&) =
    default;

DiscoveryNetworkInfo& DiscoveryNetworkInfo::operator=(
    const DiscoveryNetworkInfo&) = default;

bool DiscoveryNetworkInfo::operator==(const DiscoveryNetworkInfo& other) const {
  return name == other.name && network_id == other.network_id;
}

bool DiscoveryNetworkInfo::operator!=(const DiscoveryNetworkInfo& o) const {
  return !(*this == o);
}

}  // namespace media_router
