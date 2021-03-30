// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_NETWORK_HEALTH_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_NETWORK_HEALTH_SERVICE_H_

#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {

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

  mojo::PendingRemote<mojom::NetworkHealthService>
  GetHealthRemoteAndBindReceiver();
  mojo::PendingRemote<network_diagnostics::mojom::NetworkDiagnosticsRoutines>
  GetDiagnosticsRemoteAndBindReceiver();

  void BindHealthReceiver(
      mojo::PendingReceiver<mojom::NetworkHealthService> receiver);
  void BindDiagnosticsReceiver(
      mojo::PendingReceiver<
          network_diagnostics::mojom::NetworkDiagnosticsRoutines> receiver);

  void AddObserver(mojo::PendingRemote<mojom::NetworkEventsObserver> observer);

 private:
  std::unique_ptr<NetworkHealth> network_health_;
  std::unique_ptr<network_diagnostics::NetworkDiagnostics> network_diagnostics_;
};

}  // namespace network_health
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_NETWORK_HEALTH_SERVICE_H_
