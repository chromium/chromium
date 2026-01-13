// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_GOOGLE_SERVICES_CONNECTIVITY_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_GOOGLE_SERVICES_CONNECTIVITY_ROUTINE_H_

#include <vector>

#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"

namespace ash::network_diagnostics {

class GoogleServicesConnectivityRoutine : public NetworkDiagnosticsRoutine {
 public:
  explicit GoogleServicesConnectivityRoutine(
      chromeos::network_diagnostics::mojom::RoutineCallSource source);
  GoogleServicesConnectivityRoutine(const GoogleServicesConnectivityRoutine&) =
      delete;
  GoogleServicesConnectivityRoutine& operator=(
      const GoogleServicesConnectivityRoutine&) = delete;
  ~GoogleServicesConnectivityRoutine() override;

  // NetworkDiagnosticsRoutine:
  bool CanRun() override;
  chromeos::network_diagnostics::mojom::RoutineType Type() override;
  void Run() override;
  void AnalyzeResultsAndExecuteCallback() override;

 private:
  std::vector<chromeos::network_diagnostics::mojom::
                  GoogleServicesConnectivityProblemPtr>
      problems_;
};

}  // namespace ash::network_diagnostics

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_GOOGLE_SERVICES_CONNECTIVITY_ROUTINE_H_
