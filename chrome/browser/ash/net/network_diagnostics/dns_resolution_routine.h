// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_DNS_RESOLUTION_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_DNS_RESOLUTION_ROUTINE_H_

#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/public/host_resolver_results.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace ash {
namespace network_diagnostics {

// Tests whether a DNS resolution can be completed successfully.
class DnsResolutionRoutine : public NetworkDiagnosticsRoutine,
                             public network::ResolveHostClientBase {
 public:
  DnsResolutionRoutine();
  DnsResolutionRoutine(const DnsResolutionRoutine&) = delete;
  DnsResolutionRoutine& operator=(const DnsResolutionRoutine&) = delete;
  ~DnsResolutionRoutine() override;

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

  network::mojom::NetworkContext* network_context() { return network_context_; }
  Profile* profile() { return profile_; }

 private:
  void CreateHostResolver();
  void OnMojoConnectionError();
  void AttemptResolution();

  // Unowned
  Profile* profile_ = nullptr;
  // Unowned
  network::mojom::NetworkContext* network_context_ = nullptr;
  static constexpr int kTotalNumRetries = 1;
  int num_retries_ = kTotalNumRetries;
  bool resolved_address_received_ = false;
  net::AddressList resolved_addresses_;
  std::vector<chromeos::network_diagnostics::mojom::DnsResolutionProblem>
      problems_;
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
  mojo::Remote<network::mojom::HostResolver> host_resolver_;
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_DNS_RESOLUTION_ROUTINE_H_
