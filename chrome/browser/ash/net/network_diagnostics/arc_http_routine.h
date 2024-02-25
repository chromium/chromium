// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_ARC_HTTP_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_ARC_HTTP_ROUTINE_H_

#include <vector>

#include "ash/components/arc/mojom/net.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"

namespace ash {
namespace network_diagnostics {

// Performs HTTP GET requests from within ARC to a random set of URLs
// and returns the result.
class ArcHttpRoutine : public NetworkDiagnosticsRoutine {
 public:
  using RunArcHttpCallback = chromeos::network_diagnostics::mojom::
      NetworkDiagnosticsRoutines::RunArcHttpCallback;

  explicit ArcHttpRoutine(
      chromeos::network_diagnostics::mojom::RoutineCallSource source);
  ArcHttpRoutine(const ArcHttpRoutine&) = delete;
  ArcHttpRoutine& operator=(const ArcHttpRoutine&) = delete;
  ~ArcHttpRoutine() override;

  // NetworkDiagnosticsRoutine:
  chromeos::network_diagnostics::mojom::RoutineType Type() override;
  void Run() override;
  void AnalyzeResultsAndExecuteCallback() override;

  void set_net_instance_for_testing(arc::mojom::NetInstance* net_instance) {
    net_instance_ = net_instance;
  }

 private:
  // Attempts the next HTTP request.
  void AttemptNextRequest();

  // Gets NetInstance service if it is not already set for testing
  // purposes as |net_instance_|.
  arc::mojom::NetInstance* GetNetInstance();

  // Processes the |result| of an HttpTest API request.
  void OnRequestComplete(arc::mojom::ArcHttpTestResultPtr result);

  std::vector<std::string> hostnames_to_request_http_;
  std::vector<chromeos::network_diagnostics::mojom::ArcHttpProblem> problems_;
  bool successfully_requested_targets_ = true;
  bool failed_to_get_arc_service_manager_ = false;
  bool failed_to_get_net_instance_service_for_http_test_ = false;
  raw_ptr<arc::mojom::NetInstance, DanglingUntriaged> net_instance_ = nullptr;
  int64_t max_latency_ = 0;
  base::WeakPtrFactory<ArcHttpRoutine> weak_ptr_factory_{this};
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  //  CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_ARC_HTTP_ROUTINE_H_
