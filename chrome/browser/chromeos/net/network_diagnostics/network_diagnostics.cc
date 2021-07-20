// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics.h"

#include <memory>
#include <utility>

#include "base/bind.h"
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
  auto routine = std::make_unique<LanConnectivityRoutine>();
  auto* const routine_ptr = routine.get();
  // RunRoutine() takes a lambda callback that takes ownership of the routine.
  // This ensures that the routine stays alive when it makes asynchronous mojo
  // calls. The routine will be destroyed when the lambda exits.
  routine_ptr->RunRoutine(base::BindOnce(
      [](std::unique_ptr<LanConnectivityRoutine> routine,
         LanConnectivityCallback callback,
         mojom::RoutineVerdict verdict) { std::move(callback).Run(verdict); },
      std::move(routine), std::move(callback)));
}

void NetworkDiagnostics::SignalStrength(SignalStrengthCallback callback) {
  auto routine = std::make_unique<SignalStrengthRoutine>();
  auto* const routine_ptr = routine.get();
  // RunRoutine() takes a lambda callback that takes ownership of the routine.
  // This ensures that the routine stays alive when it makes asynchronous mojo
  // calls. The routine will be destroyed when the lambda exits.
  routine_ptr->RunRoutine(base::BindOnce(
      [](std::unique_ptr<SignalStrengthRoutine> routine,
         SignalStrengthCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::SignalStrengthProblem>& problems) {
        std::move(callback).Run(verdict, std::move(problems));
      },
      std::move(routine), std::move(callback)));
}

void NetworkDiagnostics::GatewayCanBePinged(
    GatewayCanBePingedCallback callback) {
  auto routine =
      std::make_unique<GatewayCanBePingedRoutine>(debug_daemon_client_);
  auto* const routine_ptr = routine.get();
  // RunRoutine() takes a lambda callback that takes ownership of the routine.
  // This ensures that the routine stays alive when it makes asynchronous mojo
  // calls. The routine will be destroyed when the lambda exits.
  routine_ptr->RunRoutine(base::BindOnce(
      [](std::unique_ptr<GatewayCanBePingedRoutine> routine,
         GatewayCanBePingedCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::GatewayCanBePingedProblem>& problems) {
        std::move(callback).Run(verdict, std::move(problems));
      },
      std::move(routine), std::move(callback)));
}

void NetworkDiagnostics::HasSecureWiFiConnection(
    HasSecureWiFiConnectionCallback callback) {
  auto routine = std::make_unique<HasSecureWiFiConnectionRoutine>();
  auto* const routine_ptr = routine.get();
  // RunRoutine() takes a lambda callback that takes ownership of the routine.
  // This ensures that the routine stays alive when it makes asynchronous mojo
  // calls. The routine will be destroyed when the lambda exits.
  routine_ptr->RunRoutine(base::BindOnce(
      [](std::unique_ptr<HasSecureWiFiConnectionRoutine> routine,
         HasSecureWiFiConnectionCallback callback,
         mojom::RoutineVerdict verdict,
         const std::vector<mojom::HasSecureWiFiConnectionProblem>& problems) {
        std::move(callback).Run(verdict, std::move(problems));
      },
      std::move(routine), std::move(callback)));
}

void NetworkDiagnostics::DnsResolverPresent(
    DnsResolverPresentCallback callback) {
  auto routine = std::make_unique<DnsResolverPresentRoutine>();
  auto* const routine_ptr = routine.get();
  // RunRoutine() takes a lambda callback that takes ownership of the routine.
  // This ensures that the routine stays alive when it makes asynchronous mojo
  // calls. The routine will be destroyed when the lambda exits.
  routine_ptr->RunRoutine(base::BindOnce(
      [](std::unique_ptr<DnsResolverPresentRoutine> routine,
         DnsResolverPresentCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::DnsResolverPresentProblem>& problems) {
        std::move(callback).Run(verdict, std::move(problems));
      },
      std::move(routine), std::move(callback)));
}

void NetworkDiagnostics::DnsLatency(DnsLatencyCallback callback) {
  auto routine = std::make_unique<DnsLatencyRoutine>();
  auto* const routine_ptr = routine.get();
  // RunRoutine() takes a lambda callback that takes ownership of the routine.
  // This ensures that the routine stays alive when it makes asynchronous mojo
  // calls. The routine will be destroyed when the lambda exits.
  routine_ptr->RunRoutine(base::BindOnce(
      [](std::unique_ptr<DnsLatencyRoutine> routine,
         DnsLatencyCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::DnsLatencyProblem>& problems) {
        std::move(callback).Run(verdict, std::move(problems));
      },
      std::move(routine), std::move(callback)));
}

void NetworkDiagnostics::DnsResolution(DnsResolutionCallback callback) {
  auto routine = std::make_unique<DnsResolutionRoutine>();
  auto* const routine_ptr = routine.get();
  // RunRoutine() takes a lambda callback that takes ownership of the routine.
  // This ensures that the routine stays alive when it makes asynchronous mojo
  // calls. The routine will be destroyed when the lambda exits.
  routine_ptr->RunRoutine(base::BindOnce(
      [](std::unique_ptr<DnsResolutionRoutine> routine,
         DnsResolutionCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::DnsResolutionProblem>& problems) {
        std::move(callback).Run(verdict, std::move(problems));
      },
      std::move(routine), std::move(callback)));
}

void NetworkDiagnostics::CaptivePortal(CaptivePortalCallback callback) {
  auto routine = std::make_unique<CaptivePortalRoutine>();
  auto* const routine_ptr = routine.get();
  // RunRoutine() takes a lambda callback that takes ownership of the routine.
  // This ensures that the routine stays alive when it makes asynchronous mojo
  // calls. The routine will be destroyed when the lambda exits.
  routine_ptr->RunRoutine(base::BindOnce(
      [](std::unique_ptr<CaptivePortalRoutine> routine,
         CaptivePortalCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::CaptivePortalProblem>& problems) {
        std::move(callback).Run(verdict, problems);
      },
      std::move(routine), std::move(callback)));
}

void NetworkDiagnostics::HttpFirewall(HttpFirewallCallback callback) {
  auto routine = std::make_unique<HttpFirewallRoutine>();
  auto* const routine_ptr = routine.get();
  // RunRoutine() takes a lambda callback that takes ownership of the routine.
  // This ensures that the routine stays alive when it makes asynchronous mojo
  // calls. The routine will be destroyed when the lambda exits.
  routine_ptr->RunRoutine(base::BindOnce(
      [](std::unique_ptr<HttpFirewallRoutine> routine,
         HttpFirewallCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::HttpFirewallProblem>& problems) {
        std::move(callback).Run(verdict, std::move(problems));
      },
      std::move(routine), std::move(callback)));
}

void NetworkDiagnostics::HttpsFirewall(HttpsFirewallCallback callback) {
  auto routine = std::make_unique<HttpsFirewallRoutine>();
  auto* const routine_ptr = routine.get();
  // RunRoutine() takes a lambda callback that takes ownership of the routine.
  // This ensures that the routine stays alive when it makes asynchronous mojo
  // calls. The routine will be destroyed when the lambda exits.
  routine_ptr->RunRoutine(base::BindOnce(
      [](std::unique_ptr<HttpsFirewallRoutine> routine,
         HttpsFirewallCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::HttpsFirewallProblem>& problems) {
        std::move(callback).Run(verdict, std::move(problems));
      },
      std::move(routine), std::move(callback)));
}

void NetworkDiagnostics::HttpsLatency(HttpsLatencyCallback callback) {
  auto routine = std::make_unique<HttpsLatencyRoutine>();
  auto* const routine_ptr = routine.get();
  // RunRoutine() takes a lambda callback that takes ownership of the routine.
  // This ensures that the routine stays alive when it makes asynchronous mojo
  // calls. The routine will be destroyed when the lambda exits.
  routine_ptr->RunRoutine(base::BindOnce(
      [](std::unique_ptr<HttpsLatencyRoutine> routine,
         HttpsLatencyCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::HttpsLatencyProblem>& problems) {
        std::move(callback).Run(verdict, problems);
      },
      std::move(routine), std::move(callback)));
}

void NetworkDiagnostics::VideoConferencing(
    const absl::optional<std::string>& stun_server_name,
    VideoConferencingCallback callback) {
  auto routine = std::make_unique<VideoConferencingRoutine>();
  if (stun_server_name.has_value()) {
    routine =
        std::make_unique<VideoConferencingRoutine>(stun_server_name.value());
  }

  auto* const routine_ptr = routine.get();
  // RunRoutine() takes a lambda callback that takes ownership of the routine.
  // This ensures that the routine stays alive when it makes asynchronous mojo
  // calls. The routine will be destroyed when the lambda exits.
  routine_ptr->RunRoutine(base::BindOnce(
      [](std::unique_ptr<VideoConferencingRoutine> routine,
         VideoConferencingCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::VideoConferencingProblem>& problems,
         const absl::optional<std::string>& support_details) {
        std::move(callback).Run(verdict, problems, support_details);
      },
      std::move(routine), std::move(callback)));
}

void NetworkDiagnostics::RunLanConnectivity(
    RunLanConnectivityCallback callback) {
  LanConnectivity(base::BindOnce(
      [](RunLanConnectivityCallback callback, mojom::RoutineVerdict verdict) {
        std::vector<mojom::LanConnectivityProblem> problems;
        if (verdict == mojom::RoutineVerdict::kProblem)
          problems.push_back(mojom::LanConnectivityProblem::kNoLanConnectivity);

        std::move(callback).Run(CreateResult(
            verdict, mojom::RoutineProblems::NewLanConnectivityProblems(
                         std::move(problems))));
      },
      std::move(callback)));
}

void NetworkDiagnostics::RunSignalStrength(RunSignalStrengthCallback callback) {
  SignalStrength(base::BindOnce(
      [](RunSignalStrengthCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::SignalStrengthProblem>& problems) {
        std::move(callback).Run(CreateResult(
            verdict, mojom::RoutineProblems::NewSignalStrengthProblems(
                         std::move(problems))));
      },
      std::move(callback)));
}

void NetworkDiagnostics::RunGatewayCanBePinged(
    RunGatewayCanBePingedCallback callback) {
  GatewayCanBePinged(base::BindOnce(
      [](RunGatewayCanBePingedCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::GatewayCanBePingedProblem>& problems) {
        std::move(callback).Run(CreateResult(
            verdict, mojom::RoutineProblems::NewGatewayCanBePingedProblems(
                         std::move(problems))));
      },
      std::move(callback)));
}

void NetworkDiagnostics::RunHttpFirewall(RunHttpFirewallCallback callback) {
  HttpFirewall(base::BindOnce(
      [](RunHttpFirewallCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::HttpFirewallProblem>& problems) {
        std::move(callback).Run(CreateResult(
            verdict, mojom::RoutineProblems::NewHttpFirewallProblems(
                         std::move(problems))));
      },
      std::move(callback)));
}

void NetworkDiagnostics::RunHttpsFirewall(RunHttpsFirewallCallback callback) {
  HttpsFirewall(base::BindOnce(
      [](RunHttpsFirewallCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::HttpsFirewallProblem>& problems) {
        std::move(callback).Run(CreateResult(
            verdict, mojom::RoutineProblems::NewHttpsFirewallProblems(
                         std::move(problems))));
      },
      std::move(callback)));
}

void NetworkDiagnostics::RunHasSecureWiFiConnection(
    RunHasSecureWiFiConnectionCallback callback) {
  HasSecureWiFiConnection(base::BindOnce(
      [](RunHasSecureWiFiConnectionCallback callback,
         mojom::RoutineVerdict verdict,
         const std::vector<mojom::HasSecureWiFiConnectionProblem>& problems) {
        std::move(callback).Run(CreateResult(
            verdict, mojom::RoutineProblems::NewHasSecureWifiConnectionProblems(
                         std::move(problems))));
      },
      std::move(callback)));
}

void NetworkDiagnostics::RunDnsResolverPresent(
    RunDnsResolverPresentCallback callback) {
  DnsResolverPresent(base::BindOnce(
      [](RunDnsResolverPresentCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::DnsResolverPresentProblem>& problems) {
        std::move(callback).Run(CreateResult(
            verdict, mojom::RoutineProblems::NewDnsResolverPresentProblems(
                         std::move(problems))));
      },
      std::move(callback)));
}

void NetworkDiagnostics::RunDnsLatency(RunDnsLatencyCallback callback) {
  DnsLatency(base::BindOnce(
      [](RunDnsLatencyCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::DnsLatencyProblem>& problems) {
        std::move(callback).Run(
            CreateResult(verdict, mojom::RoutineProblems::NewDnsLatencyProblems(
                                      std::move(problems))));
      },
      std::move(callback)));
}

void NetworkDiagnostics::RunDnsResolution(RunDnsResolutionCallback callback) {
  DnsResolution(base::BindOnce(
      [](RunDnsResolutionCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::DnsResolutionProblem>& problems) {
        std::move(callback).Run(CreateResult(
            verdict, mojom::RoutineProblems::NewDnsResolutionProblems(
                         std::move(problems))));
      },
      std::move(callback)));
}

void NetworkDiagnostics::RunCaptivePortal(RunCaptivePortalCallback callback) {
  CaptivePortal(base::BindOnce(
      [](RunCaptivePortalCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::CaptivePortalProblem>& problems) {
        std::move(callback).Run(CreateResult(
            verdict, mojom::RoutineProblems::NewCaptivePortalProblems(
                         std::move(problems))));
      },
      std::move(callback)));
}

void NetworkDiagnostics::RunHttpsLatency(RunHttpsLatencyCallback callback) {
  HttpsLatency(base::BindOnce(
      [](RunHttpsLatencyCallback callback, mojom::RoutineVerdict verdict,
         const std::vector<mojom::HttpsLatencyProblem>& problems) {
        std::move(callback).Run(CreateResult(
            verdict, mojom::RoutineProblems::NewHttpsLatencyProblems(
                         std::move(problems))));
      },
      std::move(callback)));
}

void NetworkDiagnostics::RunVideoConferencing(
    const absl::optional<std::string>& stun_server_name,
    RunVideoConferencingCallback callback) {
  VideoConferencing(
      stun_server_name,
      base::BindOnce(
          [](RunVideoConferencingCallback callback,
             mojom::RoutineVerdict verdict,
             const std::vector<mojom::VideoConferencingProblem>& problems,
             const absl::optional<std::string>& support_details) {
            std::move(callback).Run(CreateResult(
                verdict, mojom::RoutineProblems::NewVideoConferencingProblems(
                             std::move(problems))));
          },
          std::move(callback)));
}

}  // namespace network_diagnostics
}  // namespace chromeos
