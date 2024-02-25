// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_DNS_RESOLUTION_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_DNS_RESOLUTION_ROUTINE_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"
#include "net/dns/public/host_resolver_results.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

class Profile;

namespace network {
class SimpleHostResolver;
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace ash::network_diagnostics {

// Tests whether a DNS resolution can be completed successfully.
class DnsResolutionRoutine : public NetworkDiagnosticsRoutine {
 public:
  explicit DnsResolutionRoutine(
      chromeos::network_diagnostics::mojom::RoutineCallSource source);
  DnsResolutionRoutine(const DnsResolutionRoutine&) = delete;
  DnsResolutionRoutine& operator=(const DnsResolutionRoutine&) = delete;
  ~DnsResolutionRoutine() override;

  // NetworkDiagnosticsRoutine:
  chromeos::network_diagnostics::mojom::RoutineType Type() override;
  void Run() override;
  void AnalyzeResultsAndExecuteCallback() override;

  void set_network_context_for_testing(
      network::mojom::NetworkContext* network_context) {
    network_context_ = network_context;
  }
  void set_profile_for_testing(Profile* profile) { profile_ = profile; }

  network::mojom::NetworkContext* network_context() { return network_context_; }
  Profile* profile() { return profile_; }

 private:
  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const std::optional<net::AddressList>& resolved_addresses,
                  const std::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata);
  void CreateHostResolver();
  void AttemptResolution();

  // Unowned
  raw_ptr<Profile> profile_ = nullptr;
  // Unowned
  raw_ptr<network::mojom::NetworkContext> network_context_ = nullptr;
  static constexpr int kTotalNumRetries = 1;
  int num_retries_ = kTotalNumRetries;
  bool resolved_address_received_ = false;
  std::vector<chromeos::network_diagnostics::mojom::DnsResolutionProblem>
      problems_;
  std::unique_ptr<network::SimpleHostResolver> host_resolver_;
};

}  // namespace ash::network_diagnostics

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_DNS_RESOLUTION_ROUTINE_H_
