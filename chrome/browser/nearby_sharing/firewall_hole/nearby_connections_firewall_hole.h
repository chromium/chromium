// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_FIREWALL_HOLE_NEARBY_CONNECTIONS_FIREWALL_HOLE_H_
#define CHROME_BROWSER_NEARBY_SHARING_FIREWALL_HOLE_NEARBY_CONNECTIONS_FIREWALL_HOLE_H_

#include <memory>

#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"

namespace chromeos {
class FirewallHole;
}  // namespace chromeos

// An implementation of the mojo interface representing a firewall hole for
// Nearby Connections WifiLan TCP sockets. This implementation is essentially a
// wrapper around a ash::FirewallHole.
class NearbyConnectionsFirewallHole : public ::sharing::mojom::FirewallHole {
 public:
  explicit NearbyConnectionsFirewallHole(
      std::unique_ptr<chromeos::FirewallHole> firewall_hole);
  NearbyConnectionsFirewallHole(const NearbyConnectionsFirewallHole&) = delete;
  NearbyConnectionsFirewallHole& operator=(
      const NearbyConnectionsFirewallHole&) = delete;
  ~NearbyConnectionsFirewallHole() override;

 private:
  std::unique_ptr<chromeos::FirewallHole> firewall_hole_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_FIREWALL_HOLE_NEARBY_CONNECTIONS_FIREWALL_HOLE_H_
