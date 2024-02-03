// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_DNS_RESOLVER_PRESENT_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_DNS_RESOLVER_PRESENT_ROUTINE_H_

#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace network_diagnostics {

// Tests whether the current network is set up correctly for performing DNS
// resolution.
class DnsResolverPresentRoutine : public NetworkDiagnosticsRoutine {
 public:
  explicit DnsResolverPresentRoutine(
      chromeos::network_diagnostics::mojom::RoutineCallSource source);
  DnsResolverPresentRoutine(const DnsResolverPresentRoutine&) = delete;
  DnsResolverPresentRoutine& operator=(const DnsResolverPresentRoutine&) =
      delete;
  ~DnsResolverPresentRoutine() override;

  // NetworkDiagnosticsRoutine:
  bool CanRun() override;
  chromeos::network_diagnostics::mojom::RoutineType Type() override;
  void Run() override;
  void AnalyzeResultsAndExecuteCallback() override;

 private:
  void FetchActiveNetworks();
  void FetchManagedProperties(const std::string& guid);
  void OnManagedPropertiesReceived(
      chromeos::network_config::mojom::ManagedPropertiesPtr managed_properties);
  void OnNetworkStateListReceived(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  bool connected_network_ = false;
  bool name_servers_are_found_ = false;
  bool name_servers_are_valid_ = false;
  std::vector<chromeos::network_diagnostics::mojom::DnsResolverPresentProblem>
      problems_;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_DNS_RESOLVER_PRESENT_ROUTINE_H_
