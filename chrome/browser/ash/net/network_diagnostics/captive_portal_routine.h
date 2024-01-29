// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_CAPTIVE_PORTAL_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_CAPTIVE_PORTAL_ROUTINE_H_

#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace network_diagnostics {

// Tests whether the internet connection is trapped behind a captive portal.
class CaptivePortalRoutine : public NetworkDiagnosticsRoutine {
 public:
  explicit CaptivePortalRoutine(
      chromeos::network_diagnostics::mojom::RoutineCallSource source);
  CaptivePortalRoutine(const CaptivePortalRoutine&) = delete;
  CaptivePortalRoutine& operator=(const CaptivePortalRoutine&) = delete;
  ~CaptivePortalRoutine() override;

  // NetworkDiagnosticsRoutine:
  chromeos::network_diagnostics::mojom::RoutineType Type() override;
  void Run() override;
  void AnalyzeResultsAndExecuteCallback() override;

 private:
  void FetchActiveNetworks();
  void FetchManagedProperties(const std::string& guid);
  void OnNetworkStateListReceived(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);
  void OnManagedPropertiesReceived(
      chromeos::network_config::mojom::ManagedPropertiesPtr managed_properties);

  bool no_active_networks_ = false;
  chromeos::network_config::mojom::PortalState portal_state_ =
      chromeos::network_config::mojom::PortalState::kUnknown;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  std::vector<chromeos::network_diagnostics::mojom::CaptivePortalProblem>
      problems_;
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_CAPTIVE_PORTAL_ROUTINE_H_
