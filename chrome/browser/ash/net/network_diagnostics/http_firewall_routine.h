// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_HTTP_FIREWALL_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_HTTP_FIREWALL_ROUTINE_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/tls_prober.h"
#include "net/base/host_port_pair.h"
#include "services/network/public/cpp/network_context_getter.h"

namespace ash {
namespace network_diagnostics {

// Tests whether a firewall is blocking HTTP port 80.
class HttpFirewallRoutine : public NetworkDiagnosticsRoutine {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Creates an instance of TlsProber.
    virtual std::unique_ptr<TlsProber> CreateAndExecuteTlsProber(
        network::NetworkContextGetter network_context_getter,
        net::HostPortPair host_port_pair,
        bool negotiate_tls,
        TlsProber::TlsProbeCompleteCallback callback) = 0;
  };

  using TlsProberGetterCallback =
      base::RepeatingCallback<std::unique_ptr<TlsProber>(
          network::NetworkContextGetter network_context_getter,
          net::HostPortPair host_port_pair,
          bool negotiate_tls,
          TlsProber::TlsProbeCompleteCallback callback)>;

  explicit HttpFirewallRoutine(
      chromeos::network_diagnostics::mojom::RoutineCallSource source);
  HttpFirewallRoutine(const HttpFirewallRoutine&) = delete;
  HttpFirewallRoutine& operator=(const HttpFirewallRoutine&) = delete;
  ~HttpFirewallRoutine() override;

  // NetworkDiagnosticsRoutine:
  chromeos::network_diagnostics::mojom::RoutineType Type() override;
  void Run() override;
  void AnalyzeResultsAndExecuteCallback() override;

  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate) {
    delegate_ = std::move(delegate);
  }

  // Number of retry attempts.
  static constexpr int kTotalNumRetries = 3;

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

  // Returns the weak pointer to |this|.
  base::WeakPtr<HttpFirewallRoutine> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

  std::unique_ptr<Delegate> delegate_;
  std::vector<GURL> urls_to_query_;
  int num_urls_to_query_ = 0;
  int num_retries_ = 0;
  int dns_resolution_failures_ = 0;
  int tls_probe_failures_ = 0;
  int num_no_dns_failure_tls_probes_attempted_ = 0;
  std::unique_ptr<TlsProber> tls_prober_;
  std::vector<chromeos::network_diagnostics::mojom::HttpFirewallProblem>
      problems_;

  base::WeakPtrFactory<HttpFirewallRoutine> weak_factory_{this};
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_HTTP_FIREWALL_ROUTINE_H_
