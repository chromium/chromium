// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_DNS_LATENCY_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_DNS_LATENCY_ROUTINE_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/public/host_resolver_results.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace base {
class TickClock;
}

namespace ash {
namespace network_diagnostics {

// Tests whether the DNS latency is below an acceptable threshold.
class DnsLatencyRoutine : public NetworkDiagnosticsRoutine,
                          public network::ResolveHostClientBase {
 public:
  DnsLatencyRoutine();
  DnsLatencyRoutine(const DnsLatencyRoutine&) = delete;
  DnsLatencyRoutine& operator=(const DnsLatencyRoutine&) = delete;
  ~DnsLatencyRoutine() override;

  // NetworkDiagnosticsRoutine:
  chromeos::network_diagnostics::mojom::RoutineType Type() override;
  void Run() override;
  void AnalyzeResultsAndExecuteCallback() override;

  // network::mojom::ResolveHostClient:
  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const absl::optional<net::AddressList>& resolved_addresses,
                  const absl::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override;

  void set_network_context_for_testing(
      network::mojom::NetworkContext* network_context) {
    network_context_ = network_context;
  }
  void set_profile_for_testing(Profile* profile) { profile_ = profile; }
  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }
  network::mojom::NetworkContext* network_context() { return network_context_; }

  Profile* profile() { return profile_; }

  const base::TickClock* tick_clock() { return tick_clock_; }

 private:
  void CreateHostResolver();
  void OnMojoConnectionError();
  void AttemptNextResolution();
  bool ProblemDetected();

  // Unowned
  Profile* profile_ = nullptr;
  // Unowned
  network::mojom::NetworkContext* network_context_ = nullptr;
  // Unowned
  const base::TickClock* tick_clock_ = nullptr;
  bool successfully_resolved_all_addresses_ = false;
  base::TimeTicks start_resolution_time_;
  base::TimeTicks resolution_complete_time_;
  std::vector<std::string> hostnames_to_query_;
  std::vector<base::TimeDelta> latencies_;
  net::AddressList resolved_addresses_;
  std::vector<chromeos::network_diagnostics::mojom::DnsLatencyProblem>
      problems_;
  mojo::Remote<network::mojom::HostResolver> host_resolver_;
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_DNS_LATENCY_ROUTINE_H_
