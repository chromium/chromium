// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  void GetResult(const mojom::RoutineType type,
                 GetResultCallback callback) override;
  void GetAllResults(GetAllResultsCallback callback) override;
  void RunLanConnectivity(RunLanConnectivityCallback callback) override;
  void RunSignalStrength(RunSignalStrengthCallback callback) override;
  void RunGatewayCanBePinged(RunGatewayCanBePingedCallback callback) override;
  void RunHttpFirewall(RunHttpFirewallCallback callback) override;
  void RunHttpsFirewall(RunHttpsFirewallCallback callback) override;
  void RunHasSecureWiFiConnection(
      RunHasSecureWiFiConnectionCallback callback) override;
  void RunDnsResolverPresent(RunDnsResolverPresentCallback callback) override;
  void RunDnsLatency(RunDnsLatencyCallback callback) override;
  void RunDnsResolution(RunDnsResolutionCallback callback) override;
  void RunCaptivePortal(RunCaptivePortalCallback callback) override;
  void RunHttpsLatency(RunHttpsLatencyCallback callback) override;
  void RunVideoConferencing(const absl::optional<std::string>& stun_server_name,
                            RunVideoConferencingCallback callback) override;
  void RunArcHttp(RunArcHttpCallback callback) override;
  void RunArcDnsResolution(RunArcDnsResolutionCallback callback) override;
  void RunArcPing(RunArcPingCallback callback) override;

 private:
  void RunRoutine(std::unique_ptr<NetworkDiagnosticsRoutine> routine,
                  RoutineResultCallback callback);
  void HandleResult(std::unique_ptr<NetworkDiagnosticsRoutine> routine,
                    RoutineResultCallback callback,
                    mojom::RoutineResultPtr result);
  // An unowned pointer to the DebugDaemonClient instance.
  chromeos::DebugDaemonClient* debug_daemon_client_;
  // Receivers for external requests (WebUI, Feedback, CrosHealthdClient).
  mojo::ReceiverSet<mojom::NetworkDiagnosticsRoutines> receivers_;
  // Holds the results of the routines.
  base::flat_map<mojom::RoutineType, mojom::RoutineResultPtr> results_;

  base::WeakPtrFactory<NetworkDiagnostics> weak_ptr_factory_{this};
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_H_
