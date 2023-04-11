// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/firewall_hole/nearby_connections_firewall_hole.h"

#include "chromeos/components/firewall_hole/firewall_hole.h"

NearbyConnectionsFirewallHole::NearbyConnectionsFirewallHole(
    std::unique_ptr<chromeos::FirewallHole> firewall_hole)
    : firewall_hole_(std::move(firewall_hole)) {}

NearbyConnectionsFirewallHole::~NearbyConnectionsFirewallHole() = default;
