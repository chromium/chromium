// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_HTTPS_FIREWALL_ROUTINE_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_HTTPS_FIREWALL_ROUTINE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics_routine.h"
#include "chrome/browser/chromeos/net/network_diagnostics/tls_prober.h"
#include "net/base/host_port_pair.h"

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace chromeos {
namespace network_diagnostics {

// Number of retry attempts.
extern const int kTotalNumRetries;

// Tests whether a firewall is blocking HTTPS port 443.
class HttpsFirewallRoutine : public NetworkDiagnosticsRoutine {
 public:
  using HttpsFirewallRoutineCallback =
      mojom::NetworkDiagnosticsRoutines::HttpsFirewallCallback;
  using TlsProberGetterCallback =
      base::RepeatingCallback<std::unique_ptr<TlsProber>(
          TlsProber::NetworkContextGetter network_context_getter,
          net::HostPortPair host_port_pair,
          bool negotiate_tls,
          TlsProber::TlsProbeCompleteCallback callback)>;

  HttpsFirewallRoutine();
  HttpsFirewallRoutine(const HttpsFirewallRoutine&) = delete;
  HttpsFirewallRoutine& operator=(const HttpsFirewallRoutine&) = delete;
  ~HttpsFirewallRoutine() override;

  // NetworkDiagnosticsRoutine:
  void AnalyzeResultsAndExecuteCallback() override;

  // Run the core logic of this routine. Set |callback| to
  // |routine_completed_callback_|, which is to be executed in
  // AnalyzeResultsAndExecuteCallback().
  void RunRoutine(HttpsFirewallRoutineCallback callback);

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
      TlsProber::NetworkContextGetter network_context_getter,
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
  std::vector<mojom::HttpsFirewallProblem> problems_;
  HttpsFirewallRoutineCallback routine_completed_callback_;

  base::WeakPtrFactory<HttpsFirewallRoutine> weak_factory_{this};
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_HTTPS_FIREWALL_ROUTINE_H_
