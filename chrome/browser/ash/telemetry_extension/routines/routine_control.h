// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_CONTROL_H_
#define CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_CONTROL_H_

#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"

namespace ash {

// Implements the `TelemetryDiagnosticRoutineControl` interface and forwards all
// control calls to cros_healthd. For that reasons the class handles two mojom
// connections, one over crosapi and one over cros_healthd. If either of the
// connections get closed, the other connection is also invalidated and thus
// closed.
class CrosHealthdRoutineControl
    : public crosapi::mojom::TelemetryDiagnosticRoutineControl {
 public:
  using DeleterCallback = base::OnceCallback<void(CrosHealthdRoutineControl*)>;

  CrosHealthdRoutineControl();
  CrosHealthdRoutineControl(const CrosHealthdRoutineControl&) = delete;
  CrosHealthdRoutineControl& operator=(const CrosHealthdRoutineControl&) =
      delete;
  ~CrosHealthdRoutineControl() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_CONTROL_H_
