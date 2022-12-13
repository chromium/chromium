// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FIREWALL_HOLE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_FIREWALL_HOLE_ASH_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/firewall_hole.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace ash {
class FirewallHole;
}  // namespace ash

namespace crosapi {

// FirewallHoleAsh wraps the ash::FirewallHole object and resets it upon
// destruction.
class FirewallHoleAsh : public crosapi::mojom::FirewallHole {
 public:
  explicit FirewallHoleAsh(std::unique_ptr<ash::FirewallHole> firewall_hole);
  ~FirewallHoleAsh() override;

 private:
  std::unique_ptr<ash::FirewallHole> firewall_hole_;

  mojo::Receiver<crosapi::mojom::FirewallHole> receiver_{this};
};

// Ash implementation of crosapi::mojom::FirewallHoleService.
class FirewallHoleServiceAsh : public crosapi::mojom::FirewallHoleService {
 public:
  FirewallHoleServiceAsh();
  ~FirewallHoleServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::FirewallHoleService> receiver);

  // crosapi::mojom::FirewallHoleService:
  void OpenTCPFirewallHole(const std::string& interface_name,
                           uint16_t port,
                           OpenTCPFirewallHoleCallback) override;
  void OpenUDPFirewallHole(const std::string& interface_name,
                           uint16_t port,
                           OpenUDPFirewallHoleCallback) override;

 private:
  void OnFirewallHoleOpened(
      base::OnceCallback<
          void(mojo::PendingRemote<crosapi::mojom::FirewallHole>)> callback,
      std::unique_ptr<ash::FirewallHole> firewall_hole);

  // Supports any number of receivers.
  mojo::ReceiverSet<crosapi::mojom::FirewallHoleService> receivers_;

  mojo::UniqueReceiverSet<crosapi::mojom::FirewallHole>
      firewall_hole_receivers_;
  base::WeakPtrFactory<FirewallHoleServiceAsh> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_FIREWALL_HOLE_ASH_H_
