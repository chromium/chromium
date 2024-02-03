// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/video_conferencing_routine.h"

#include <optional>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_util.h"
#include "chrome/browser/ash/net/network_diagnostics/udp_prober.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

const char kDefaultStunServer[] = "stun.l.google.com";

}  // namespace

// TODO(crbug/1227877): Move support details to the UI.
const char kSupportDetails[] = "https://support.google.com/a/answer/1279090";
const base::TimeDelta kTimeoutAfterHostResolution = base::Seconds(10);

VideoConferencingRoutine::VideoConferencingRoutine(
    mojom::RoutineCallSource source)
    : NetworkDiagnosticsRoutine(source),
      stun_server_hostname_(kDefaultStunServer),
      udp_prober_getter_callback_(base::BindRepeating(
          &VideoConferencingRoutine::CreateAndExecuteUdpProber)),
      tls_prober_getter_callback_(base::BindRepeating(
          &VideoConferencingRoutine::CreateAndExecuteTlsProber)),
      udp_ports_(util::GetUdpPortsForGoogleStunServer()),
      tcp_ports_(util::GetTcpPortsForGoogleStunServer()),
      media_hostnames_(util::GetDefaultMediaUrls()) {}

VideoConferencingRoutine::VideoConferencingRoutine(
    mojom::RoutineCallSource source,
    const std::string& stun_server_hostname)
    : NetworkDiagnosticsRoutine(source),
      stun_server_hostname_(stun_server_hostname),
      udp_prober_getter_callback_(base::BindRepeating(
          &VideoConferencingRoutine::CreateAndExecuteUdpProber)),
      tls_prober_getter_callback_(base::BindRepeating(
          &VideoConferencingRoutine::CreateAndExecuteTlsProber)),
      udp_ports_(util::GetUdpPortsForCustomStunServer()),
      tcp_ports_(util::GetTcpPortsForCustomStunServer()),
      media_hostnames_(util::GetDefaultMediaUrls()) {}

VideoConferencingRoutine::~VideoConferencingRoutine() = default;

mojom::RoutineType VideoConferencingRoutine::Type() {
  return mojom::RoutineType::kVideoConferencing;
}

void VideoConferencingRoutine::Run() {
  ProbeStunServerOverUdp();
}

void VideoConferencingRoutine::AnalyzeResultsAndExecuteCallback() {
  std::optional<std::string> support_details = kSupportDetails;
  set_verdict(mojom::RoutineVerdict::kProblem);
  if (!open_udp_port_found_) {
    problems_.push_back(mojom::VideoConferencingProblem::kUdpFailure);
  }
  if (!open_tcp_port_found_) {
    problems_.push_back(mojom::VideoConferencingProblem::kTcpFailure);
  }
  if (!media_hostnames_reachable_) {
    problems_.push_back(mojom::VideoConferencingProblem::kMediaFailure);
  }
  if (problems_.empty()) {
    set_verdict(mojom::RoutineVerdict::kNoProblem);
    support_details = std::nullopt;
  }
  set_problems(mojom::RoutineProblems::NewVideoConferencingProblems(problems_));
  ExecuteCallback();
}


void VideoConferencingRoutine::ProbeStunServerOverUdp() {
  if (udp_ports_.empty()) {
    ProbeStunServerOverTcp();
    return;
  }
  int port = udp_ports_.back();
  udp_ports_.pop_back();
  AttemptUdpProbe(net::HostPortPair(stun_server_hostname_, port));
}

void VideoConferencingRoutine::ProbeStunServerOverTcp() {
  if (tcp_ports_.empty()) {
    ProbeMediaHostnames();
    return;
  }
  int port = tcp_ports_.back();
  tcp_ports_.pop_back();
  AttemptTcpProbe(net::HostPortPair(stun_server_hostname_, port));
}

void VideoConferencingRoutine::ProbeMediaHostnames() {
  if (media_hostnames_.empty()) {
    AnalyzeResultsAndExecuteCallback();
    return;
  }
  auto host_pair = net::HostPortPair::FromURL(media_hostnames_.back());
  media_hostnames_.pop_back();
  AttemptTlsProbe(host_pair);
}

network::mojom::NetworkContext* VideoConferencingRoutine::GetNetworkContext() {
  Profile* profile = util::GetUserProfile();
  DCHECK(profile);

  return profile->GetDefaultStoragePartition()->GetNetworkContext();
}

std::unique_ptr<UdpProber> VideoConferencingRoutine::CreateAndExecuteUdpProber(
    network::NetworkContextGetter network_context_getter,
    net::HostPortPair host_port_pair,
    base::span<const uint8_t> data,
    net::NetworkTrafficAnnotationTag tag,
    base::TimeDelta timeout_after_host_resolution,
    UdpProber::UdpProbeCompleteCallback callback) {
  return UdpProber::Start(std::move(network_context_getter), host_port_pair,
                          std::move(data), tag, timeout_after_host_resolution,
                          std::move(callback));
}

std::unique_ptr<TlsProber> VideoConferencingRoutine::CreateAndExecuteTlsProber(
    network::NetworkContextGetter network_context_getter,
    net::HostPortPair host_port_pair,
    bool negotiate_tls,
    TlsProber::TlsProbeCompleteCallback callback) {
  return std::make_unique<TlsProber>(std::move(network_context_getter),
                                     host_port_pair, negotiate_tls,
                                     std::move(callback));
}

void VideoConferencingRoutine::AttemptUdpProbe(
    net::HostPortPair host_port_pair) {
  // Store the instance of UdpProber.
  udp_prober_ = udp_prober_getter_callback_.Run(
      base::BindRepeating(&VideoConferencingRoutine::GetNetworkContext),
      host_port_pair, util::GetStunHeader(),
      util::GetStunNetworkAnnotationTag(), kTimeoutAfterHostResolution,
      base::BindOnce(&VideoConferencingRoutine::OnUdpProbeComplete,
                     weak_ptr()));
}

void VideoConferencingRoutine::AttemptTcpProbe(
    net::HostPortPair host_port_pair) {
  tls_prober_ = tls_prober_getter_callback_.Run(
      base::BindRepeating(&VideoConferencingRoutine::GetNetworkContext),
      host_port_pair,
      /*negotiate_tls=*/false,
      base::BindOnce(&VideoConferencingRoutine::OnTcpProbeComplete,
                     weak_ptr()));
}

void VideoConferencingRoutine::AttemptTlsProbe(
    net::HostPortPair host_port_pair) {
  // Store the instance of TlsProber.
  tls_prober_ = tls_prober_getter_callback_.Run(
      base::BindRepeating(&VideoConferencingRoutine::GetNetworkContext),
      host_port_pair,
      /*negotiate_tls=*/true,
      base::BindOnce(&VideoConferencingRoutine::OnTlsProbeComplete,
                     weak_ptr()));
}

void VideoConferencingRoutine::OnUdpProbeComplete(
    int result,
    UdpProber::ProbeExitEnum probe_exit_enum) {
  if (result == net::OK) {
    open_udp_port_found_ = true;
    // Only one open UDP port needs to be detected.
    ProbeStunServerOverTcp();
    return;
  }
  ProbeStunServerOverUdp();
}

void VideoConferencingRoutine::OnTcpProbeComplete(
    int result,
    TlsProber::ProbeExitEnum probe_exit_enum) {
  if (result == net::OK) {
    open_tcp_port_found_ = true;
    // Only one open TCP port needs to be detected.
    ProbeMediaHostnames();
    return;
  }
  ProbeStunServerOverTcp();
}

void VideoConferencingRoutine::OnTlsProbeComplete(
    int result,
    TlsProber::ProbeExitEnum probe_exit_enum) {
  if (result != net::OK) {
    media_hostnames_reachable_ = false;
    // All media hostnames must be reachable.
    AnalyzeResultsAndExecuteCallback();
    return;
  }
  ProbeMediaHostnames();
}

}  // namespace network_diagnostics
}  // namespace ash
