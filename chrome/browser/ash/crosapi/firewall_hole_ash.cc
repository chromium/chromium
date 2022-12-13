// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/firewall_hole_ash.h"

#include "chromeos/ash/components/network/firewall_hole.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace crosapi {

FirewallHoleAsh::FirewallHoleAsh(
    std::unique_ptr<ash::FirewallHole> firewall_hole)
    : firewall_hole_(std::move(firewall_hole)) {}

FirewallHoleAsh::~FirewallHoleAsh() = default;

FirewallHoleServiceAsh::FirewallHoleServiceAsh() = default;

FirewallHoleServiceAsh::~FirewallHoleServiceAsh() = default;

void FirewallHoleServiceAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::FirewallHoleService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FirewallHoleServiceAsh::OpenTCPFirewallHole(
    const std::string& interface_name,
    uint16_t port,
    OpenTCPFirewallHoleCallback callback) {
  ash::FirewallHole::Open(
      ash::FirewallHole::PortType::TCP, port, interface_name,
      base::BindOnce(&FirewallHoleServiceAsh::OnFirewallHoleOpened,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FirewallHoleServiceAsh::OpenUDPFirewallHole(
    const std::string& interface_name,
    uint16_t port,
    OpenUDPFirewallHoleCallback callback) {
  ash::FirewallHole::Open(
      ash::FirewallHole::PortType::UDP, port, interface_name,
      base::BindOnce(&FirewallHoleServiceAsh::OnFirewallHoleOpened,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

// This function wraps the resulting |firewall_hole| as `FirewallHoleAsh` and
// yields a controlling remote. Resetting this remote will close the firewall
// hole.
void FirewallHoleServiceAsh::OnFirewallHoleOpened(
    base::OnceCallback<void(mojo::PendingRemote<crosapi::mojom::FirewallHole>)>
        callback,
    std::unique_ptr<ash::FirewallHole> firewall_hole) {
  if (!firewall_hole) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }
  mojo::PendingRemote<crosapi::mojom::FirewallHole> firewall_hole_remote;
  firewall_hole_receivers_.Add(
      std::make_unique<FirewallHoleAsh>(std::move(firewall_hole)),
      firewall_hole_remote.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(std::move(firewall_hole_remote));
}

}  // namespace crosapi
