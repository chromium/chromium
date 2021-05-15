// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_DNS_RESOLUTION_ROUTINE_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_DNS_RESOLUTION_ROUTINE_H_

#include <vector>

#include "base/callback.h"
#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics_routine.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace chromeos {
namespace network_diagnostics {

// Tests whether a DNS resolution can be completed successfully.
class DnsResolutionRoutine : public NetworkDiagnosticsRoutine,
                             public network::ResolveHostClientBase {
 public:
  using DnsResolutionRoutineCallback =
      mojom::NetworkDiagnosticsRoutines::DnsResolutionCallback;

  DnsResolutionRoutine();
  DnsResolutionRoutine(const DnsResolutionRoutine&) = delete;
  DnsResolutionRoutine& operator=(const DnsResolutionRoutine&) = delete;
  ~DnsResolutionRoutine() override;

  // NetworkDiagnosticsRoutine:
  void AnalyzeResultsAndExecuteCallback() override;

  // network::mojom::ResolveHostClient:
  void OnComplete(
      int result,
      const net::ResolveErrorInfo& resolve_error_info,
      const absl::optional<net::AddressList>& resolved_addresses) override;

  // Run the core logic of this routine. Set |callback| to
  // |routine_completed_callback_|, which is to be executed in
  // AnalyzeResultsAndExecuteCallback().
  void RunRoutine(DnsResolutionRoutineCallback callback);

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
  std::vector<mojom::DnsResolutionProblem> problems_;
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
  mojo::Remote<network::mojom::HostResolver> host_resolver_;
  DnsResolutionRoutineCallback routine_completed_callback_;
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_DNS_RESOLUTION_ROUTINE_H_
