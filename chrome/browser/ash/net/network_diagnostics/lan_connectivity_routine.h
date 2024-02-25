// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_LAN_CONNECTIVITY_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_LAN_CONNECTIVITY_ROUTINE_H_

#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace network_diagnostics {

// Tests whether the device is connected to a LAN. It is possible that the
// device may be trapped in a captive portal yet pass this test successfully.
// Captive portal checks are done separately and are outside of the scope of
// this routine.
class LanConnectivityRoutine : public NetworkDiagnosticsRoutine {
 public:
  explicit LanConnectivityRoutine(
      chromeos::network_diagnostics::mojom::RoutineCallSource source);
  LanConnectivityRoutine(const LanConnectivityRoutine&) = delete;
  LanConnectivityRoutine& operator=(const LanConnectivityRoutine&) = delete;
  ~LanConnectivityRoutine() override;

  // NetworkDiagnosticsRoutine:
  bool CanRun() override;
  chromeos::network_diagnostics::mojom::RoutineType Type() override;
  void Run() override;
  void AnalyzeResultsAndExecuteCallback() override;

 private:
  void FetchActiveNetworks();
  void OnNetworkStateListReceived(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  bool lan_connected_ = false;
  std::vector<chromeos::network_diagnostics::mojom::LanConnectivityProblem>
      problems_;
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_LAN_CONNECTIVITY_ROUTINE_H_
