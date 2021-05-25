// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_DNS_RESOLVER_PRESENT_ROUTINE_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_DNS_RESOLVER_PRESENT_ROUTINE_H_

#include <vector>

#include "base/callback.h"
#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics_routine.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace network_diagnostics {

// Tests whether the current network is set up correctly for performing DNS
// resolution.
class DnsResolverPresentRoutine : public NetworkDiagnosticsRoutine {
 public:
  using DnsResolverPresentRoutineCallback =
      mojom::NetworkDiagnosticsRoutines::DnsResolverPresentCallback;

  DnsResolverPresentRoutine();
  DnsResolverPresentRoutine(const DnsResolverPresentRoutine&) = delete;
  DnsResolverPresentRoutine& operator=(const DnsResolverPresentRoutine&) =
      delete;
  ~DnsResolverPresentRoutine() override;

  // NetworkDiagnosticsRoutine:
  bool CanRun() override;
  void AnalyzeResultsAndExecuteCallback() override;

  // Run the core logic of this routine. Set |callback| to
  // |routine_completed_callback_|, which is to be executed in
  // AnalyzeResultsAndExecuteCallback().
  void RunRoutine(DnsResolverPresentRoutineCallback callback);

 private:
  void FetchActiveNetworks();
  void FetchManagedProperties(const std::string& guid);
  void OnManagedPropertiesReceived(
      chromeos::network_config::mojom::ManagedPropertiesPtr managed_properties);
  void OnNetworkStateListReceived(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  bool connected_network_ = false;
  bool name_servers_found_ = false;
  bool non_empty_name_servers_ = false;
  bool well_formed_name_servers_ = false;
  std::vector<mojom::DnsResolverPresentProblem> problems_;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  DnsResolverPresentRoutineCallback routine_completed_callback_;
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_DNS_RESOLVER_PRESENT_ROUTINE_H_
