// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {

class DebugDaemonClient;

namespace network_diagnostics {

class NetworkDiagnostics
    : public chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines,
      public chromeos::mojo_service_manager::mojom::ServiceProvider {
 public:
  explicit NetworkDiagnostics(DebugDaemonClient* debug_daemon_client);
  NetworkDiagnostics(const NetworkDiagnostics&) = delete;
  NetworkDiagnostics& operator=(const NetworkDiagnostics&) = delete;
  ~NetworkDiagnostics() override;

  // Binds this instance to |receiver|.
  void BindReceiver(
      mojo::PendingReceiver<
          chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
          receiver);

  // chromeos::network_diagnostics::mojom::NetworkDiagnostics
  void GetResult(const chromeos::network_diagnostics::mojom::RoutineType type,
                 GetResultCallback callback) override;
  void GetAllResults(GetAllResultsCallback callback) override;
  void RunLanConnectivity(
      std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
          source,
      RunLanConnectivityCallback callback) override;
  void RunSignalStrength(
      std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
          source,
      RunSignalStrengthCallback callback) override;
  void RunGatewayCanBePinged(
      std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
          source,
      RunGatewayCanBePingedCallback callback) override;
  void RunHttpFirewall(
      std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
          source,
      RunHttpFirewallCallback callback) override;
  void RunHttpsFirewall(
      std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
          source,
      RunHttpsFirewallCallback callback) override;
  void RunHasSecureWiFiConnection(
      std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
          source,
      RunHasSecureWiFiConnectionCallback callback) override;
  void RunDnsResolverPresent(
      std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
          source,
      RunDnsResolverPresentCallback callback) override;
  void RunDnsLatency(
      std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
          source,
      RunDnsLatencyCallback callback) override;
  void RunDnsResolution(
      std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
          source,
      RunDnsResolutionCallback callback) override;
  void RunCaptivePortal(
      std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
          source,
      RunCaptivePortalCallback callback) override;
  void RunHttpsLatency(
      std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
          source,
      RunHttpsLatencyCallback callback) override;
  void RunVideoConferencing(
      const std::optional<std::string>& stun_server_name,
      std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
          source,
      RunVideoConferencingCallback callback) override;
  void RunArcHttp(
      std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
          source,
      RunArcHttpCallback callback) override;
  void RunArcDnsResolution(
      std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
          source,
      RunArcDnsResolutionCallback callback) override;
  void RunArcPing(
      std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
          source,
      RunArcPingCallback callback) override;

 private:
  // chromeos::mojo_service_manager::mojom::ServiceProvider overrides.
  void Request(
      chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
      mojo::ScopedMessagePipeHandle receiver) override;

  void RunRoutine(std::unique_ptr<NetworkDiagnosticsRoutine> routine,
                  RoutineResultCallback callback);
  void HandleResult(
      std::unique_ptr<NetworkDiagnosticsRoutine> routine,
      RoutineResultCallback callback,
      chromeos::network_diagnostics::mojom::RoutineResultPtr result);
  // An unowned pointer to the DebugDaemonClient instance.
  raw_ptr<DebugDaemonClient, LeakedDanglingUntriaged> debug_daemon_client_;
  // Receiver for mojo service manager service provider.
  mojo::Receiver<chromeos::mojo_service_manager::mojom::ServiceProvider>
      provider_receiver_{this};
  // Receivers for external requests (WebUI, Feedback, CrosHealthdClient).
  mojo::ReceiverSet<
      chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
      receivers_;
  // Holds the results of the routines.
  base::flat_map<chromeos::network_diagnostics::mojom::RoutineType,
                 chromeos::network_diagnostics::mojom::RoutineResultPtr>
      results_;

  base::WeakPtrFactory<NetworkDiagnostics> weak_ptr_factory_{this};
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_H_
