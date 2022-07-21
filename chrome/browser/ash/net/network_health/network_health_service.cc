// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_health/network_health_service.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics.h"
#include "chrome/browser/ash/net/network_health/network_health.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"

namespace ash {
namespace network_health {

// TODO(https://crbug.com/1164001): remove when migrated to namespace ash.
namespace mojom = ::chromeos::network_health::mojom;

NetworkHealthService::NetworkHealthService() {
  network_health_ = std::make_unique<NetworkHealth>();
  network_diagnostics_ =
      std::make_unique<network_diagnostics::NetworkDiagnostics>(
          chromeos::DebugDaemonClient::Get());
}

mojo::PendingRemote<mojom::NetworkHealthService>
NetworkHealthService::GetHealthRemoteAndBindReceiver() {
  mojo::PendingRemote<mojom::NetworkHealthService> remote;
  BindHealthReceiver(remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

mojo::PendingRemote<
    chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
NetworkHealthService::GetDiagnosticsRemoteAndBindReceiver() {
  mojo::PendingRemote<
      chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
      remote;
  BindDiagnosticsReceiver(remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void NetworkHealthService::BindHealthReceiver(
    mojo::PendingReceiver<mojom::NetworkHealthService> receiver) {
  network_health_->BindReceiver(std::move(receiver));
}

void NetworkHealthService::BindDiagnosticsReceiver(
    mojo::PendingReceiver<
        chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
        receiver) {
  network_diagnostics_->BindReceiver(std::move(receiver));
}

void NetworkHealthService::AddObserver(
    mojo::PendingRemote<mojom::NetworkEventsObserver> observer) {
  network_health_->AddObserver(std::move(observer));
}

NetworkHealthService* NetworkHealthService::GetInstance() {
  static base::NoDestructor<NetworkHealthService> instance;
  return instance.get();
}

}  // namespace network_health
}  // namespace ash
