// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "chrome/browser/nearby_sharing/firewall_hole/nearby_connections_firewall_hole_factory.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/nearby_sharing/firewall_hole/nearby_connections_firewall_hole.h"
#include "chromeos/ash/services/nearby/public/cpp/tcp_server_socket_port.h"
#include "chromeos/components/firewall_hole/firewall_hole.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

NearbyConnectionsFirewallHoleFactory::NearbyConnectionsFirewallHoleFactory() =
    default;

NearbyConnectionsFirewallHoleFactory::~NearbyConnectionsFirewallHoleFactory() =
    default;

void NearbyConnectionsFirewallHoleFactory::OpenFirewallHole(
    const ash::nearby::TcpServerSocketPort& port,
    OpenFirewallHoleCallback callback) {
  chromeos::FirewallHole::Open(
      chromeos::FirewallHole::PortType::kTcp, port.port(),
      /*interface=*/std::string(),
      base::BindOnce(
          &NearbyConnectionsFirewallHoleFactory::OnFirewallHoleOpened,
          weak_ptr_factory_.GetWeakPtr(), port, std::move(callback)));
}

void NearbyConnectionsFirewallHoleFactory::OnFirewallHoleOpened(
    const ash::nearby::TcpServerSocketPort& port,
    OpenFirewallHoleCallback callback,
    std::unique_ptr<chromeos::FirewallHole> firewall_hole) {
  if (!firewall_hole) {
    LOG(ERROR) << "NearbyConnectionsFirewallHoleFactory::" << __func__
               << ": Failed to open TCP firewall hole on port " << port.port();
    std::move(callback).Run(/*firewall_hole=*/mojo::NullRemote());
    return;
  }

  mojo::PendingRemote<::sharing::mojom::FirewallHole> firewall_hole_remote;
  firewall_hole_receivers_.Add(
      std::make_unique<NearbyConnectionsFirewallHole>(std::move(firewall_hole)),
      firewall_hole_remote.InitWithNewPipeAndPassReceiver());

  std::move(callback).Run(std::move(firewall_hole_remote));
}
