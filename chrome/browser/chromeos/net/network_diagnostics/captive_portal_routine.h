// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_CAPTIVE_PORTAL_ROUTINE_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_CAPTIVE_PORTAL_ROUTINE_H_

#include <vector>

#include "base/callback.h"
#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics_routine.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace network_diagnostics {

// Tests whether the internet connection is trapped behind a captive portal.
class CaptivePortalRoutine : public NetworkDiagnosticsRoutine {
 public:
  using CaptivePortalRoutineCallback =
      mojom::NetworkDiagnosticsRoutines::CaptivePortalCallback;

  CaptivePortalRoutine();
  CaptivePortalRoutine(const CaptivePortalRoutine&) = delete;
  CaptivePortalRoutine& operator=(const CaptivePortalRoutine&) = delete;
  ~CaptivePortalRoutine() override;

  // NetworkDiagnosticsRoutine:
  void AnalyzeResultsAndExecuteCallback() override;

  // Run the core logic of this routine. Set |callback| to
  // |routine_completed_callback_|, which is to be executed in
  // AnalyzeResultsAndExecuteCallback().
  void RunRoutine(CaptivePortalRoutineCallback callback);

 private:
  void FetchActiveNetworks();
  void FetchManagedProperties(const std::string& guid);
  void OnNetworkStateListReceived(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);
  void OnManagedPropertiesReceived(
      network_config::mojom::ManagedPropertiesPtr managed_properties);

  bool no_active_networks_ = false;
  chromeos::network_config::mojom::PortalState portal_state_ =
      chromeos::network_config::mojom::PortalState::kUnknown;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  std::vector<mojom::CaptivePortalProblem> problems_;
  CaptivePortalRoutineCallback routine_completed_callback_;
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_CAPTIVE_PORTAL_ROUTINE_H_
