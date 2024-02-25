// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_SIGNAL_STRENGTH_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_SIGNAL_STRENGTH_ROUTINE_H_

#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace network_diagnostics {

// Tests the Network Interface Controller (NIC) signal strength.
class SignalStrengthRoutine : public NetworkDiagnosticsRoutine {
 public:
  explicit SignalStrengthRoutine(
      chromeos::network_diagnostics::mojom::RoutineCallSource source);
  SignalStrengthRoutine(const SignalStrengthRoutine&) = delete;
  SignalStrengthRoutine& operator=(const SignalStrengthRoutine&) = delete;
  ~SignalStrengthRoutine() override;

  // NetworkDiagnosticRoutine:
  bool CanRun() override;
  chromeos::network_diagnostics::mojom::RoutineType Type() override;
  void Run() override;
  void AnalyzeResultsAndExecuteCallback() override;

 private:
  void FetchActiveWirelessNetworks();
  void OnNetworkStateListReceived(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  // Represents the strength of an unknown signal.
  static constexpr int kUnknownSignalStrength = 0;
  int signal_strength_ = kUnknownSignalStrength;
  std::vector<chromeos::network_diagnostics::mojom::SignalStrengthProblem>
      problems_;
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_SIGNAL_STRENGTH_ROUTINE_H_
