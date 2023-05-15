// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_HEALTH_NETWORK_HEALTH_MANAGER_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_HEALTH_NETWORK_HEALTH_MANAGER_H_

#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {

namespace network_diagnostics {
class NetworkDiagnostics;
}

namespace network_health {

class NetworkHealthHelper;
class NetworkHealthService;

class NetworkHealthManager {
 public:
  static NetworkHealthManager* GetInstance();

  // These functions create or retrieve an existing NetworkHealthManager
  // instance and bind a `receiver` to it.
  static void NetworkDiagnosticsServiceCallback(
      mojo::PendingReceiver<
          chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
          receiver);
  static void NetworkHealthServiceCallback(
      mojo::PendingReceiver<
          chromeos::network_health::mojom::NetworkHealthService> receiver);

  NetworkHealthManager();
  ~NetworkHealthManager() = delete;

  mojo::PendingRemote<chromeos::network_health::mojom::NetworkHealthService>
  GetHealthRemoteAndBindReceiver();
  mojo::PendingRemote<
      chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
  GetDiagnosticsRemoteAndBindReceiver();

  void BindHealthReceiver(
      mojo::PendingReceiver<
          chromeos::network_health::mojom::NetworkHealthService> receiver);
  void BindDiagnosticsReceiver(
      mojo::PendingReceiver<
          chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
          receiver);

  void AddObserver(
      mojo::PendingRemote<
          chromeos::network_health::mojom::NetworkEventsObserver> observer);

  NetworkHealthHelper* helper() { return helper_.get(); }

 private:
  std::unique_ptr<network_diagnostics::NetworkDiagnostics> network_diagnostics_;
  std::unique_ptr<NetworkHealthHelper> helper_;
};

}  // namespace network_health
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_HEALTH_NETWORK_HEALTH_MANAGER_H_
