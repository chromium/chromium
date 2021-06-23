// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_SIGNAL_STRENGTH_ROUTINE_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_SIGNAL_STRENGTH_ROUTINE_H_

#include <vector>

#include "base/callback.h"
#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics_routine.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace network_diagnostics {

// Tests the Network Interface Controller (NIC) signal strength.
class SignalStrengthRoutine : public NetworkDiagnosticsRoutine {
 public:
  using SignalStrengthRoutineCallback =
      mojom::NetworkDiagnosticsRoutines::SignalStrengthCallback;

  SignalStrengthRoutine();
  SignalStrengthRoutine(const SignalStrengthRoutine&) = delete;
  SignalStrengthRoutine& operator=(const SignalStrengthRoutine&) = delete;
  ~SignalStrengthRoutine() override;

  // NetworkDiagnosticRoutine:
  bool CanRun() override;
  void AnalyzeResultsAndExecuteCallback() override;

  // Run the core logic of this routine. Set |callback| to
  // |routine_completed_callback_|, which is to be executed in
  // AnalyzeResultsAndExecuteCallback().
  void RunRoutine(SignalStrengthRoutineCallback callback);

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
  std::vector<mojom::SignalStrengthProblem> problems_;
  SignalStrengthRoutineCallback routine_completed_callback_;
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_SIGNAL_STRENGTH_ROUTINE_H_
