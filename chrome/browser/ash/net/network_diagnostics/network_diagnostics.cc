// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ash/net/network_diagnostics/arc_dns_resolution_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/arc_http_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/arc_ping_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/captive_portal_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/dns_latency_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/dns_resolution_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/dns_resolver_present_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/gateway_can_be_pinged_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/has_secure_wifi_connection_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/http_firewall_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/https_firewall_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/https_latency_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/lan_connectivity_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/signal_strength_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/video_conferencing_routine.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash {
namespace network_diagnostics {

namespace mojom = ::chromeos::network_diagnostics::mojom;

NetworkDiagnostics::NetworkDiagnostics(DebugDaemonClient* debug_daemon_client) {
  DCHECK(debug_daemon_client);
  if (debug_daemon_client) {
    debug_daemon_client_ = debug_daemon_client;
  }
  if (mojo_service_manager::IsServiceManagerBound()) {
    mojo_service_manager::GetServiceManagerProxy()->Register(
        chromeos::mojo_services::kChromiumNetworkDiagnosticsRoutines,
        provider_receiver_.BindNewPipeAndPassRemote());
  }
}

NetworkDiagnostics::~NetworkDiagnostics() {}

void NetworkDiagnostics::BindReceiver(
    mojo::PendingReceiver<mojom::NetworkDiagnosticsRoutines> receiver) {
  NET_LOG(EVENT) << "NetworkDiagnostics::BindReceiver()";
  receivers_.Add(this, std::move(receiver));
}

void NetworkDiagnostics::GetResult(mojom::RoutineType type,
                                   GetResultCallback callback) {
  mojom::RoutineResultPtr result;
  if (results_.count(type)) {
    result = results_[type].Clone();
  }
  std::move(callback).Run(std::move(result));
}

void NetworkDiagnostics::GetAllResults(GetAllResultsCallback callback) {
  base::flat_map<mojom::RoutineType, mojom::RoutineResultPtr> response;
  for (auto& r : results_) {
    response[r.first] = r.second.Clone();
  }
  std::move(callback).Run(std::move(response));
}

void NetworkDiagnostics::RunLanConnectivity(
    std::optional<mojom::RoutineCallSource> source,
    RunLanConnectivityCallback callback) {
  mojom::RoutineCallSource src = mojom::RoutineCallSource::kUnknown;
  if (source.has_value()) {
    src = source.value();
  }
  auto routine = std::make_unique<LanConnectivityRoutine>(src);
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunSignalStrength(
    std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
        source,
    RunSignalStrengthCallback callback) {
  mojom::RoutineCallSource src = mojom::RoutineCallSource::kUnknown;
  if (source.has_value()) {
    src = source.value();
  }
  auto routine = std::make_unique<SignalStrengthRoutine>(src);
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunGatewayCanBePinged(
    std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
        source,
    RunGatewayCanBePingedCallback callback) {
  mojom::RoutineCallSource src = mojom::RoutineCallSource::kUnknown;
  if (source.has_value()) {
    src = source.value();
  }
  auto routine =
      std::make_unique<GatewayCanBePingedRoutine>(src, debug_daemon_client_);
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunHttpFirewall(
    std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
        source,
    RunHttpFirewallCallback callback) {
  mojom::RoutineCallSource src = mojom::RoutineCallSource::kUnknown;
  if (source.has_value()) {
    src = source.value();
  }
  auto routine = std::make_unique<HttpFirewallRoutine>(src);
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunHttpsFirewall(
    std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
        source,
    RunHttpsFirewallCallback callback) {
  mojom::RoutineCallSource src = mojom::RoutineCallSource::kUnknown;
  if (source.has_value()) {
    src = source.value();
  }
  auto routine = std::make_unique<HttpsFirewallRoutine>(src);
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunHasSecureWiFiConnection(
    std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
        source,
    RunHasSecureWiFiConnectionCallback callback) {
  mojom::RoutineCallSource src = mojom::RoutineCallSource::kUnknown;
  if (source.has_value()) {
    src = source.value();
  }
  auto routine = std::make_unique<HasSecureWiFiConnectionRoutine>(src);
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunDnsResolverPresent(
    std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
        source,
    RunDnsResolverPresentCallback callback) {
  mojom::RoutineCallSource src = mojom::RoutineCallSource::kUnknown;
  if (source.has_value()) {
    src = source.value();
  }
  auto routine = std::make_unique<DnsResolverPresentRoutine>(src);
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunDnsLatency(
    std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
        source,
    RunDnsLatencyCallback callback) {
  mojom::RoutineCallSource src = mojom::RoutineCallSource::kUnknown;
  if (source.has_value()) {
    src = source.value();
  }
  auto routine = std::make_unique<DnsLatencyRoutine>(src);
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunDnsResolution(
    std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
        source,
    RunDnsResolutionCallback callback) {
  mojom::RoutineCallSource src = mojom::RoutineCallSource::kUnknown;
  if (source.has_value()) {
    src = source.value();
  }
  auto routine = std::make_unique<DnsResolutionRoutine>(src);
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunCaptivePortal(
    std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
        source,
    RunCaptivePortalCallback callback) {
  mojom::RoutineCallSource src = mojom::RoutineCallSource::kUnknown;
  if (source.has_value()) {
    src = source.value();
  }
  auto routine = std::make_unique<CaptivePortalRoutine>(src);
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunHttpsLatency(
    std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
        source,
    RunHttpsLatencyCallback callback) {
  mojom::RoutineCallSource src = mojom::RoutineCallSource::kUnknown;
  if (source.has_value()) {
    src = source.value();
  }
  auto routine = std::make_unique<HttpsLatencyRoutine>(src);
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunVideoConferencing(
    const std::optional<std::string>& stun_server_name,
    std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
        source,
    RunVideoConferencingCallback callback) {
  mojom::RoutineCallSource src = mojom::RoutineCallSource::kUnknown;
  if (source.has_value()) {
    src = source.value();
  }
  std::unique_ptr<NetworkDiagnosticsRoutine> routine;
  if (stun_server_name) {
    routine = std::make_unique<VideoConferencingRoutine>(
        src, stun_server_name.value());
  } else {
    routine = std::make_unique<VideoConferencingRoutine>(src);
  }
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunArcHttp(
    std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
        source,
    RunArcHttpCallback callback) {
  mojom::RoutineCallSource src = mojom::RoutineCallSource::kUnknown;
  if (source.has_value()) {
    src = source.value();
  }
  auto routine = std::make_unique<ArcHttpRoutine>(src);
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunArcDnsResolution(
    std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
        source,
    RunArcDnsResolutionCallback callback) {
  mojom::RoutineCallSource src = mojom::RoutineCallSource::kUnknown;
  if (source.has_value()) {
    src = source.value();
  }
  auto routine = std::make_unique<ArcDnsResolutionRoutine>(src);
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunArcPing(
    std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
        source,
    RunArcPingCallback callback) {
  mojom::RoutineCallSource src = mojom::RoutineCallSource::kUnknown;
  if (source.has_value()) {
    src = source.value();
  }
  auto routine = std::make_unique<ArcPingRoutine>(src);
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::Request(
    chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
    mojo::ScopedMessagePipeHandle receiver) {
  BindReceiver(mojo::PendingReceiver<mojom::NetworkDiagnosticsRoutines>(
      std::move(receiver)));
}

void NetworkDiagnostics::RunRoutine(
    std::unique_ptr<NetworkDiagnosticsRoutine> routine,
    RoutineResultCallback callback) {
  auto* const routine_ptr = routine.get();
  routine_ptr->RunRoutine(base::BindOnce(
      &NetworkDiagnostics::HandleResult, weak_ptr_factory_.GetWeakPtr(),
      std::move(routine), std::move(callback)));
}

void NetworkDiagnostics::HandleResult(
    std::unique_ptr<NetworkDiagnosticsRoutine> routine,
    RoutineResultCallback callback,
    mojom::RoutineResultPtr result) {
  results_[routine->Type()] = result->Clone();
  std::move(callback).Run(std::move(result));
}

}  // namespace network_diagnostics
}  // namespace ash
