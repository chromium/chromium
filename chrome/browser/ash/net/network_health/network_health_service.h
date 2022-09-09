// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_HEALTH_NETWORK_HEALTH_SERVICE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_HEALTH_NETWORK_HEALTH_SERVICE_H_

#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {

namespace network_diagnostics {
class NetworkDiagnostics;
}

namespace network_health {

class NetworkHealth;

class NetworkHealthService {
 public:
  static NetworkHealthService* GetInstance();

  NetworkHealthService();
  ~NetworkHealthService() = delete;

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

 private:
  std::unique_ptr<NetworkHealth> network_health_;
  std::unique_ptr<network_diagnostics::NetworkDiagnostics> network_diagnostics_;
};

}  // namespace network_health
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos {
namespace network_health {
using ::ash::network_health::NetworkHealthService;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_HEALTH_NETWORK_HEALTH_SERVICE_H_
