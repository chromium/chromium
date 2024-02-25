// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"

#include "base/time/time.h"

namespace ash {
namespace network_diagnostics {

NetworkDiagnosticsRoutine::NetworkDiagnosticsRoutine(
    chromeos::network_diagnostics::mojom::RoutineCallSource source) {
  result_.verdict =
      chromeos::network_diagnostics::mojom::RoutineVerdict::kNotRun;
  result_.source = source;
}

NetworkDiagnosticsRoutine::~NetworkDiagnosticsRoutine() = default;

bool NetworkDiagnosticsRoutine::CanRun() {
  return true;
}

void NetworkDiagnosticsRoutine::RunRoutine(RoutineResultCallback callback) {
  callback_ = std::move(callback);

  if (!CanRun()) {
    ExecuteCallback();
    return;
  }

  Run();
}

void NetworkDiagnosticsRoutine::ExecuteCallback() {
  result_.timestamp = base::Time::Now();
  std::move(callback_).Run(result_.Clone());
}

}  // namespace network_diagnostics
}  // namespace ash
