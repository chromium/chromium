// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
class DebugDaemonClient;

namespace network_diagnostics {

class NetworkDiagnostics : public mojom::NetworkDiagnosticsRoutines {
 public:
  explicit NetworkDiagnostics(chromeos::DebugDaemonClient* debug_daemon_client);
  NetworkDiagnostics(const NetworkDiagnostics&) = delete;
  NetworkDiagnostics& operator=(const NetworkDiagnostics&) = delete;
  ~NetworkDiagnostics() override;

  // Binds this instance to |receiver|.
  void BindReceiver(
      mojo::PendingReceiver<mojom::NetworkDiagnosticsRoutines> receiver);

  // mojom::NetworkDiagnostics
  void LanConnectivity(LanConnectivityCallback callback) override;
  void SignalStrength(SignalStrengthCallback callback) override;
  void GatewayCanBePinged(GatewayCanBePingedCallback callback) override;
  void HttpFirewall(HttpFirewallCallback callback) override;
  void HttpsFirewall(HttpsFirewallCallback callback) override;
  void HasSecureWiFiConnection(
      HasSecureWiFiConnectionCallback callback) override;
  void DnsResolverPresent(DnsResolverPresentCallback callback) override;
  void DnsLatency(DnsLatencyCallback callback) override;
  void DnsResolution(DnsResolutionCallback callback) override;
  void CaptivePortal(CaptivePortalCallback callback) override;
  void HttpsLatency(HttpsLatencyCallback callback) override;
  void VideoConferencing(const base::Optional<std::string>& stun_server_name,
                         VideoConferencingCallback callback) override;

 private:
  // An unowned pointer to the DebugDaemonClient instance.
  chromeos::DebugDaemonClient* debug_daemon_client_;
  // Receivers for external requests (WebUI, Feedback, CrosHealthdClient).
  mojo::ReceiverSet<mojom::NetworkDiagnosticsRoutines> receivers_;
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_H_
