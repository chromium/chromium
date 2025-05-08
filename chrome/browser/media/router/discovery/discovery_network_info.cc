// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/discovery_network_info.h"

namespace media_router {

DiscoveryNetworkInfo::DiscoveryNetworkInfo() = default;

DiscoveryNetworkInfo::DiscoveryNetworkInfo(const std::string& name,
                                           const std::string& network_id)
    : name(name), network_id(network_id) {}

DiscoveryNetworkInfo::~DiscoveryNetworkInfo() = default;

DiscoveryNetworkInfo::DiscoveryNetworkInfo(const DiscoveryNetworkInfo&) =
    default;

DiscoveryNetworkInfo& DiscoveryNetworkInfo::operator=(
    const DiscoveryNetworkInfo&) = default;

}  // namespace media_router
