// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_HTTPS_FIREWALL_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_HTTPS_FIREWALL_ROUTINE_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/tls_prober.h"
#include "net/base/host_port_pair.h"
#include "services/network/public/cpp/network_context_getter.h"

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace ash {
namespace network_diagnostics {

// Number of retry attempts.
extern const int kTotalNumRetries;

// Tests whether a firewall is blocking HTTPS port 443.
class HttpsFirewallRoutine : public NetworkDiagnosticsRoutine {
 public:
  using TlsProberGetterCallback =
      base::RepeatingCallback<std::unique_ptr<TlsProber>(
          network::NetworkContextGetter network_context_getter,
          net::HostPortPair host_port_pair,
          bool negotiate_tls,
          TlsProber::TlsProbeCompleteCallback callback)>;

  explicit HttpsFirewallRoutine(
      chromeos::network_diagnostics::mojom::RoutineCallSource source);
  HttpsFirewallRoutine(const HttpsFirewallRoutine&) = delete;
  HttpsFirewallRoutine& operator=(const HttpsFirewallRoutine&) = delete;
  ~HttpsFirewallRoutine() override;

  // NetworkDiagnosticsRoutine:
  chromeos::network_diagnostics::mojom::RoutineType Type() override;
  void Run() override;
  void AnalyzeResultsAndExecuteCallback() override;

  void set_tls_prober_getter_callback_for_testing(
      TlsProberGetterCallback tls_prober_getter_callback) {
    tls_prober_getter_callback_ = std::move(tls_prober_getter_callback);
  }

 private:
  // Gets the next URL to probe.
  void ProbeNextUrl();

  // Helper function to launch a TLS probe.
  void AttemptProbe(const GURL& url);

  // Callback invoked once probe is complete. |url| is only relevant in case
  // of probe retries.
  void OnProbeComplete(const GURL& url,
                       int result,
                       TlsProber::ProbeExitEnum probe_exit_enum);

  // Returns the network context.
  static network::mojom::NetworkContext* GetNetworkContext();

  // Creates an instance of TlsProber.
  static std::unique_ptr<TlsProber> CreateAndExecuteTlsProber(
      network::NetworkContextGetter network_context_getter,
      net::HostPortPair host_port_pair,
      bool negotiate_tls,
      TlsProber::TlsProbeCompleteCallback callback);

  // Returns the weak pointer to |this|.
  base::WeakPtr<HttpsFirewallRoutine> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

  std::vector<GURL> urls_to_query_;
  int num_urls_to_query_ = 0;
  int num_retries_ = 0;
  int dns_resolution_failures_ = 0;
  int tls_probe_failures_ = 0;
  int num_no_dns_failure_tls_probes_attempted_ = 0;
  TlsProberGetterCallback tls_prober_getter_callback_;
  std::unique_ptr<TlsProber> tls_prober_;
  std::vector<chromeos::network_diagnostics::mojom::HttpsFirewallProblem>
      problems_;

  base::WeakPtrFactory<HttpsFirewallRoutine> weak_factory_{this};
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_HTTPS_FIREWALL_ROUTINE_H_
