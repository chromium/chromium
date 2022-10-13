// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_health/network_health_manager.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/services/network_health/network_health_service.h"

namespace ash {
namespace network_health {

NetworkHealthManager::NetworkHealthManager() {
  network_health_service_ =
      std::make_unique<chromeos::network_health::NetworkHealthService>();
  network_diagnostics_ =
      std::make_unique<network_diagnostics::NetworkDiagnostics>(
          DebugDaemonClient::Get());
}

mojo::PendingRemote<chromeos::network_health::mojom::NetworkHealthService>
NetworkHealthManager::GetHealthRemoteAndBindReceiver() {
  mojo::PendingRemote<chromeos::network_health::mojom::NetworkHealthService>
      remote;
  BindHealthReceiver(remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

mojo::PendingRemote<
    chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
NetworkHealthManager::GetDiagnosticsRemoteAndBindReceiver() {
  mojo::PendingRemote<
      chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
      remote;
  BindDiagnosticsReceiver(remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void NetworkHealthManager::BindHealthReceiver(
    mojo::PendingReceiver<chromeos::network_health::mojom::NetworkHealthService>
        receiver) {
  network_health_service_->BindReceiver(std::move(receiver));
}

void NetworkHealthManager::BindDiagnosticsReceiver(
    mojo::PendingReceiver<
        chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
        receiver) {
  network_diagnostics_->BindReceiver(std::move(receiver));
}

void NetworkHealthManager::AddObserver(
    mojo::PendingRemote<chromeos::network_health::mojom::NetworkEventsObserver>
        observer) {
  network_health_service_->AddObserver(std::move(observer));
}

NetworkHealthManager* NetworkHealthManager::GetInstance() {
  static base::NoDestructor<NetworkHealthManager> instance;
  return instance.get();
}

}  // namespace network_health
}  // namespace ash
