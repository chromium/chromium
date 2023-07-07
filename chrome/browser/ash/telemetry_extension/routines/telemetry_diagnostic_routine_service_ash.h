// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_ROUTINES_TELEMETRY_DIAGNOSTIC_ROUTINE_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_ROUTINES_TELEMETRY_DIAGNOSTIC_ROUTINE_SERVICE_ASH_H_

#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {

// Implementation of the `TelemetryDiagnosticRoutinesService`, allows for
// creating new routines on the platform as well as interaction with existing
// routines and requesting information about the `SupportStatus` of routines.
class TelemetryDiagnosticsRoutineServiceAsh
    : public crosapi::mojom::TelemetryDiagnosticRoutinesService {
 public:
  TelemetryDiagnosticsRoutineServiceAsh();
  TelemetryDiagnosticsRoutineServiceAsh(
      const TelemetryDiagnosticsRoutineServiceAsh&) = delete;
  TelemetryDiagnosticsRoutineServiceAsh& operator=(
      const TelemetryDiagnosticsRoutineServiceAsh&) = delete;
  ~TelemetryDiagnosticsRoutineServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::TelemetryDiagnosticRoutinesService>
          receiver);

 private:
  // Support any number of connections.
  mojo::ReceiverSet<crosapi::mojom::TelemetryDiagnosticRoutinesService>
      receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_ROUTINES_TELEMETRY_DIAGNOSTIC_ROUTINE_SERVICE_ASH_H_
