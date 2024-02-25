// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_ARC_DNS_RESOLUTION_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_ARC_DNS_RESOLUTION_ROUTINE_H_

#include <string>
#include <vector>

#include "ash/components/arc/mojom/net.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"

namespace ash {
namespace network_diagnostics {

// Performs DNS queries from within ARC to resolve a hardcoded set of hostnames
// and returns the result.
class ArcDnsResolutionRoutine : public NetworkDiagnosticsRoutine {
 public:
  using RunArcDnsResolutionCallback = chromeos::network_diagnostics::mojom::
      NetworkDiagnosticsRoutines::RunArcDnsResolutionCallback;

  explicit ArcDnsResolutionRoutine(
      chromeos::network_diagnostics::mojom::RoutineCallSource source);
  ArcDnsResolutionRoutine(const ArcDnsResolutionRoutine&) = delete;
  ArcDnsResolutionRoutine& operator=(const ArcDnsResolutionRoutine&) = delete;
  ~ArcDnsResolutionRoutine() override;

  // NetworkDiagnosticsRoutine:
  chromeos::network_diagnostics::mojom::RoutineType Type() override;
  void Run() override;
  void AnalyzeResultsAndExecuteCallback() override;

  void set_net_instance_for_testing(arc::mojom::NetInstance* net_instance) {
    net_instance_ = net_instance;
  }

 private:
  // Attempts the next DNS query.
  void AttemptNextQuery();

  // Gets NetInstance service if it is not already set for testing
  // purposes as |net_instance_|.
  arc::mojom::NetInstance* GetNetInstance();

  // Processes the |result| of an DnsResolutionTest API request.
  void OnQueryComplete(arc::mojom::ArcDnsResolutionTestResultPtr result);

  std::vector<std::string> hostnames_to_resolve_dns_;
  std::vector<chromeos::network_diagnostics::mojom::ArcDnsResolutionProblem>
      problems_;
  bool successfully_resolved_hostnames_ = true;
  bool failed_to_get_arc_service_manager_ = false;
  bool failed_to_get_net_instance_service_for_dns_resolution_test_ = false;
  raw_ptr<arc::mojom::NetInstance, DanglingUntriaged> net_instance_ = nullptr;
  int64_t max_latency_ = 0;
  base::WeakPtrFactory<ArcDnsResolutionRoutine> weak_ptr_factory_{this};
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  //  CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_ARC_DNS_RESOLUTION_ROUTINE_H_
