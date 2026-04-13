// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_GOOGLE_SERVICES_CONNECTIVITY_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_GOOGLE_SERVICES_CONNECTIVITY_ROUTINE_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"

namespace ash {
class ShillManagerClient;
}  // namespace ash

namespace ash::network_diagnostics {

// Tests connectivity to Google services by attempting to connect to various
// Google endpoints.
class GoogleServicesConnectivityRoutine : public NetworkDiagnosticsRoutine {
 public:
  // `shill_manager_client` must outlive this object.
  GoogleServicesConnectivityRoutine(
      chromeos::network_diagnostics::mojom::RoutineCallSource source,
      ShillManagerClient* shill_manager_client);
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
  void OnGetHostsConnectivityResult(const std::vector<uint8_t>& response);
  void OnGetHostsConnectivityError(const std::string& error_name,
                                   const std::string& error_message);
  void ParseConnectivityResponse(const std::vector<uint8_t>& proto_response);

  const raw_ref<ShillManagerClient> shill_manager_client_;
  std::vector<chromeos::network_diagnostics::mojom::
                  GoogleServicesConnectivityProblemPtr>
      problems_;

  base::WeakPtrFactory<GoogleServicesConnectivityRoutine> weak_factory_{this};
};

}  // namespace ash::network_diagnostics

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_GOOGLE_SERVICES_CONNECTIVITY_ROUTINE_H_
