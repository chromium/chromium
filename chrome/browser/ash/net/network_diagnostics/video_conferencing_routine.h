// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_VIDEO_CONFERENCING_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_VIDEO_CONFERENCING_ROUTINE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/tls_prober.h"
#include "chrome/browser/ash/net/network_diagnostics/udp_prober.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "url/gurl.h"

namespace ash {
namespace network_diagnostics {

extern const char kSupportDetails[];
extern const base::TimeDelta kTimeoutAfterHostResolution;

// Tests the device's video conferencing capabilities by testing the connection
// to a sample of Google servers used in various GVC offerings. See the
// README.md file for more details.
class VideoConferencingRoutine : public NetworkDiagnosticsRoutine {
 public:
  using UdpProberGetterCallback =
      base::RepeatingCallback<std::unique_ptr<UdpProber>(
          network::NetworkContextGetter network_context_getter,
          net::HostPortPair host_port_pair,
          base::span<const uint8_t> data,
          net::NetworkTrafficAnnotationTag tag,
          base::TimeDelta timeout_after_host_resolution,
          UdpProber::UdpProbeCompleteCallback callback)>;
  using TlsProberGetterCallback =
      base::RepeatingCallback<std::unique_ptr<TlsProber>(
          network::NetworkContextGetter network_context_getter,
          net::HostPortPair host_port_pair,
          bool negotiate_tls,
          TlsProber::TlsProbeCompleteCallback callback)>;

  // Creates a routine using a default STUN server.
  explicit VideoConferencingRoutine(
      chromeos::network_diagnostics::mojom::RoutineCallSource source);
  // Creates a routine using a custom STUN server.
  VideoConferencingRoutine(
      chromeos::network_diagnostics::mojom::RoutineCallSource source,
      const std::string& stun_server_hostname);
  VideoConferencingRoutine(const VideoConferencingRoutine&) = delete;
  VideoConferencingRoutine& operator=(const VideoConferencingRoutine&) = delete;
  ~VideoConferencingRoutine() override;

  // NetworkDiagnosticsRoutine:
  chromeos::network_diagnostics::mojom::RoutineType Type() override;
  void Run() override;
  void AnalyzeResultsAndExecuteCallback() override;

  void set_udp_prober_getter_callback_for_testing(
      UdpProberGetterCallback udp_prober_getter_callback) {
    udp_prober_getter_callback_ = std::move(udp_prober_getter_callback);
  }

  void set_tls_prober_getter_callback_for_testing(
      TlsProberGetterCallback tls_prober_getter_callback) {
    tls_prober_getter_callback_ = std::move(tls_prober_getter_callback);
  }

 private:
  // Probes the STUN server over UDP to determine whether an open port
  // connection exists.
  void ProbeStunServerOverUdp();

  // Probes the STUN server over TCP to determine whether an open port
  // connection exists.
  void ProbeStunServerOverTcp();

  // Probes media endpoints over TCP with TLS.
  void ProbeMediaHostnames();

  // Returns the network context.
  static network::mojom::NetworkContext* GetNetworkContext();

  // Creates and instance of UdpProber.
  static std::unique_ptr<UdpProber> CreateAndExecuteUdpProber(
      network::NetworkContextGetter network_context_getter,
      net::HostPortPair host_port_pair,
      base::span<const uint8_t> data,
      net::NetworkTrafficAnnotationTag tag,
      base::TimeDelta timeout_after_host_resolution,
      UdpProber::UdpProbeCompleteCallback callback);

  // Creates an instance of TlsProber.
  static std::unique_ptr<TlsProber> CreateAndExecuteTlsProber(
      network::NetworkContextGetter network_context_getter,
      net::HostPortPair host_port_pair,
      bool negotiate_tls,
      TlsProber::TlsProbeCompleteCallback callback);

  // Launches a UDP probe.
  void AttemptUdpProbe(net::HostPortPair host_port_pair);

  // Launches a TCP probe.
  void AttemptTcpProbe(net::HostPortPair host_port_pair);

  // Launches a TLS probe.
  void AttemptTlsProbe(net::HostPortPair host_port_pair);

  // Handles UDP probe completion.
  void OnUdpProbeComplete(int result, UdpProber::ProbeExitEnum probe_exit_enum);

  // Handles TCP probe completion.
  void OnTcpProbeComplete(int result, TlsProber::ProbeExitEnum probe_exit_enum);

  // Handles TLS probe completion.
  void OnTlsProbeComplete(int result, TlsProber::ProbeExitEnum probe_exit_enum);

  // Returns the weak pointer to |this|.
  base::WeakPtr<VideoConferencingRoutine> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

  std::vector<chromeos::network_diagnostics::mojom::VideoConferencingProblem>
      problems_;
  std::string stun_server_hostname_;
  bool open_udp_port_found_ = false;
  bool open_tcp_port_found_ = false;
  bool media_hostnames_reachable_ = true;
  UdpProberGetterCallback udp_prober_getter_callback_;
  TlsProberGetterCallback tls_prober_getter_callback_;
  std::unique_ptr<UdpProber> udp_prober_;
  std::unique_ptr<TlsProber> tls_prober_;
  std::vector<int> udp_ports_;
  std::vector<int> tcp_ports_;
  std::vector<GURL> media_hostnames_;

  base::WeakPtrFactory<VideoConferencingRoutine> weak_factory_{this};
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_VIDEO_CONFERENCING_ROUTINE_H_
