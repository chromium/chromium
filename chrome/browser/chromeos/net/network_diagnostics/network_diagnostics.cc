// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/chromeos/net/network_diagnostics/arc_dns_resolution_routine.h"
#include "chrome/browser/chromeos/net/network_diagnostics/arc_http_routine.h"
#include "chrome/browser/chromeos/net/network_diagnostics/captive_portal_routine.h"
#include "chrome/browser/chromeos/net/network_diagnostics/dns_latency_routine.h"
#include "chrome/browser/chromeos/net/network_diagnostics/dns_resolution_routine.h"
#include "chrome/browser/chromeos/net/network_diagnostics/dns_resolver_present_routine.h"
#include "chrome/browser/chromeos/net/network_diagnostics/gateway_can_be_pinged_routine.h"
#include "chrome/browser/chromeos/net/network_diagnostics/has_secure_wifi_connection_routine.h"
#include "chrome/browser/chromeos/net/network_diagnostics/http_firewall_routine.h"
#include "chrome/browser/chromeos/net/network_diagnostics/https_firewall_routine.h"
#include "chrome/browser/chromeos/net/network_diagnostics/https_latency_routine.h"
#include "chrome/browser/chromeos/net/network_diagnostics/lan_connectivity_routine.h"
#include "chrome/browser/chromeos/net/network_diagnostics/signal_strength_routine.h"
#include "chrome/browser/chromeos/net/network_diagnostics/video_conferencing_routine.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace network_diagnostics {

namespace {

mojom::RoutineResultPtr CreateResult(mojom::RoutineVerdict verdict,
                                     mojom::RoutineProblemsPtr problems) {
  return mojom::RoutineResult::New(verdict, std::move(problems),
                                   base::Time::Now());
}

}  // namespace

NetworkDiagnostics::NetworkDiagnostics(
    chromeos::DebugDaemonClient* debug_daemon_client) {
  DCHECK(debug_daemon_client);
  if (debug_daemon_client) {
    debug_daemon_client_ = debug_daemon_client;
  }
}

NetworkDiagnostics::~NetworkDiagnostics() {}

void NetworkDiagnostics::BindReceiver(
    mojo::PendingReceiver<mojom::NetworkDiagnosticsRoutines> receiver) {
  NET_LOG(EVENT) << "NetworkDiagnostics::BindReceiver()";
  receivers_.Add(this, std::move(receiver));
}

void NetworkDiagnostics::LanConnectivity(LanConnectivityCallback callback) {
  RunLanConnectivity(base::BindOnce(
      [](LanConnectivityCallback callback, mojom::RoutineResultPtr result) {
        std::move(callback).Run(result->verdict);
      },
      std::move(callback)));
}

void NetworkDiagnostics::SignalStrength(SignalStrengthCallback callback) {
  RunSignalStrength(base::BindOnce(
      [](SignalStrengthCallback callback, mojom::RoutineResultPtr result) {
        std::move(callback).Run(
            result->verdict,
            std::move(result->problems->get_signal_strength_problems()));
      },
      std::move(callback)));
}

void NetworkDiagnostics::GatewayCanBePinged(
    GatewayCanBePingedCallback callback) {
  RunGatewayCanBePinged(base::BindOnce(
      [](GatewayCanBePingedCallback callback, mojom::RoutineResultPtr result) {
        std::move(callback).Run(
            result->verdict,
            std::move(result->problems->get_gateway_can_be_pinged_problems()));
      },
      std::move(callback)));
}

void NetworkDiagnostics::HasSecureWiFiConnection(
    HasSecureWiFiConnectionCallback callback) {
  RunHasSecureWiFiConnection(base::BindOnce(
      [](HasSecureWiFiConnectionCallback callback,
         mojom::RoutineResultPtr result) {
        std::move(callback).Run(
            result->verdict,
            std::move(
                result->problems->get_has_secure_wifi_connection_problems()));
      },
      std::move(callback)));
}

void NetworkDiagnostics::DnsResolverPresent(
    DnsResolverPresentCallback callback) {
  RunDnsResolverPresent(base::BindOnce(
      [](DnsResolverPresentCallback callback, mojom::RoutineResultPtr result) {
        std::move(callback).Run(
            result->verdict,
            std::move(result->problems->get_dns_resolver_present_problems()));
      },
      std::move(callback)));
}

void NetworkDiagnostics::DnsLatency(DnsLatencyCallback callback) {
  RunDnsLatency(base::BindOnce(
      [](DnsLatencyCallback callback, mojom::RoutineResultPtr result) {
        std::move(callback).Run(
            result->verdict,
            std::move(result->problems->get_dns_latency_problems()));
      },
      std::move(callback)));
}

void NetworkDiagnostics::DnsResolution(DnsResolutionCallback callback) {
  RunDnsResolution(base::BindOnce(
      [](DnsResolutionCallback callback, mojom::RoutineResultPtr result) {
        std::move(callback).Run(
            result->verdict,
            std::move(result->problems->get_dns_resolution_problems()));
      },
      std::move(callback)));
}

void NetworkDiagnostics::CaptivePortal(CaptivePortalCallback callback) {
  RunCaptivePortal(base::BindOnce(
      [](CaptivePortalCallback callback, mojom::RoutineResultPtr result) {
        std::move(callback).Run(
            result->verdict,
            std::move(result->problems->get_captive_portal_problems()));
      },
      std::move(callback)));
}

void NetworkDiagnostics::HttpFirewall(HttpFirewallCallback callback) {
  RunHttpFirewall(base::BindOnce(
      [](HttpFirewallCallback callback, mojom::RoutineResultPtr result) {
        std::move(callback).Run(
            result->verdict,
            std::move(result->problems->get_http_firewall_problems()));
      },
      std::move(callback)));
}

void NetworkDiagnostics::HttpsFirewall(HttpsFirewallCallback callback) {
  RunHttpsFirewall(base::BindOnce(
      [](HttpsFirewallCallback callback, mojom::RoutineResultPtr result) {
        std::move(callback).Run(
            result->verdict,
            std::move(result->problems->get_https_firewall_problems()));
      },
      std::move(callback)));
}

void NetworkDiagnostics::HttpsLatency(HttpsLatencyCallback callback) {
  RunHttpsLatency(base::BindOnce(
      [](HttpsLatencyCallback callback, mojom::RoutineResultPtr result) {
        std::move(callback).Run(
            result->verdict,
            std::move(result->problems->get_https_latency_problems()));
      },
      std::move(callback)));
}

void NetworkDiagnostics::VideoConferencing(
    const absl::optional<std::string>& stun_server_name,
    VideoConferencingCallback callback) {
  RunVideoConferencing(
      std::move(stun_server_name),
      base::BindOnce(
          [](VideoConferencingCallback callback,
             mojom::RoutineResultPtr result) {
            std::move(callback).Run(
                result->verdict,
                result->problems->get_video_conferencing_problems(), "");
          },
          std::move(callback)));
}

void NetworkDiagnostics::RunLanConnectivity(
    RunLanConnectivityCallback callback) {
  auto routine = std::make_unique<LanConnectivityRoutine>();
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunSignalStrength(RunSignalStrengthCallback callback) {
  auto routine = std::make_unique<SignalStrengthRoutine>();
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunGatewayCanBePinged(
    RunGatewayCanBePingedCallback callback) {
  auto routine =
      std::make_unique<GatewayCanBePingedRoutine>(debug_daemon_client_);
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunHttpFirewall(RunHttpFirewallCallback callback) {
  auto routine = std::make_unique<HttpFirewallRoutine>();
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunHttpsFirewall(RunHttpsFirewallCallback callback) {
  auto routine = std::make_unique<HttpsFirewallRoutine>();
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunHasSecureWiFiConnection(
    RunHasSecureWiFiConnectionCallback callback) {
  auto routine = std::make_unique<HasSecureWiFiConnectionRoutine>();
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunDnsResolverPresent(
    RunDnsResolverPresentCallback callback) {
  auto routine = std::make_unique<DnsResolverPresentRoutine>();
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunDnsLatency(RunDnsLatencyCallback callback) {
  auto routine = std::make_unique<DnsLatencyRoutine>();
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunDnsResolution(RunDnsResolutionCallback callback) {
  auto routine = std::make_unique<DnsResolutionRoutine>();
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunCaptivePortal(RunCaptivePortalCallback callback) {
  auto routine = std::make_unique<CaptivePortalRoutine>();
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunHttpsLatency(RunHttpsLatencyCallback callback) {
  auto routine = std::make_unique<HttpsLatencyRoutine>();
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunVideoConferencing(
    const absl::optional<std::string>& stun_server_name,
    RunVideoConferencingCallback callback) {
  std::unique_ptr<NetworkDiagnosticsRoutine> routine;
  if (stun_server_name) {
    routine =
        std::make_unique<VideoConferencingRoutine>(stun_server_name.value());
  } else {
    routine = std::make_unique<VideoConferencingRoutine>();
  }
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunArcHttp(RunArcHttpCallback callback) {
  auto routine = std::make_unique<ArcHttpRoutine>();
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunArcDnsResolution(
    RunArcDnsResolutionCallback callback) {
  auto routine = std::make_unique<ArcDnsResolutionRoutine>();
  RunRoutine(std::move(routine), std::move(callback));
}

void NetworkDiagnostics::RunRoutine(
    std::unique_ptr<NetworkDiagnosticsRoutine> routine,
    RoutineResultCallback callback) {
  auto* const routine_ptr = routine.get();
  routine_ptr->RunRoutine(base::BindOnce(
      &NetworkDiagnostics::HandleResult, weak_ptr_factory_.GetWeakPtr(),
      std::move(routine), std::move(callback)));
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

void NetworkDiagnostics::HandleResult(
    std::unique_ptr<NetworkDiagnosticsRoutine> routine,
    RoutineResultCallback callback,
    mojom::RoutineResultPtr result) {
  results_[routine->Type()] = result->Clone();
  std::move(callback).Run(std::move(result));
}

}  // namespace network_diagnostics
}  // namespace chromeos
