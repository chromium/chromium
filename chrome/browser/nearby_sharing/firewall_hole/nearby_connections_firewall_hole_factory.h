// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_FIREWALL_HOLE_NEARBY_CONNECTIONS_FIREWALL_HOLE_FACTORY_H_
#define CHROME_BROWSER_NEARBY_SHARING_FIREWALL_HOLE_NEARBY_CONNECTIONS_FIREWALL_HOLE_FACTORY_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace ash::nearby {
class TcpServerSocketPort;
}  // namespace ash::nearby

namespace chromeos {
class FirewallHole;
}  // namespace chromeos

// An implementation of the mojo service used to open firewall holes for Nearby
// Connections WifiLan TCP sockets. This implementation is essentially a wrapper
// around ash::FirewallHole::Open(). The lifetime of a firewall hole
// mirrors the lifetime of the mojo remote provided by OpenFirewallHole(); the
// corresponding mojo receivers are owned by |firewall_hole_receivers_|.
class NearbyConnectionsFirewallHoleFactory
    : public ::sharing::mojom::FirewallHoleFactory {
 public:
  NearbyConnectionsFirewallHoleFactory();
  NearbyConnectionsFirewallHoleFactory(
      const NearbyConnectionsFirewallHoleFactory&) = delete;
  NearbyConnectionsFirewallHoleFactory& operator=(
      const NearbyConnectionsFirewallHoleFactory&) = delete;
  ~NearbyConnectionsFirewallHoleFactory() override;

  void OpenFirewallHole(const ash::nearby::TcpServerSocketPort& port,
                        OpenFirewallHoleCallback callback) override;

 private:
  void OnFirewallHoleOpened(
      const ash::nearby::TcpServerSocketPort& port,
      OpenFirewallHoleCallback callback,
      std::unique_ptr<chromeos::FirewallHole> firewall_hole);

  mojo::UniqueReceiverSet<::sharing::mojom::FirewallHole>
      firewall_hole_receivers_;
  base::WeakPtrFactory<NearbyConnectionsFirewallHoleFactory> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_FIREWALL_HOLE_NEARBY_CONNECTIONS_FIREWALL_HOLE_FACTORY_H_
