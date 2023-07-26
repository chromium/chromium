// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/routines/telemetry_diagnostic_routine_service_ash.h"

#include <utility>

#include "base/notreached.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

TelemetryDiagnosticsRoutineServiceAsh::TelemetryDiagnosticsRoutineServiceAsh() =
    default;

TelemetryDiagnosticsRoutineServiceAsh::
    ~TelemetryDiagnosticsRoutineServiceAsh() = default;

void TelemetryDiagnosticsRoutineServiceAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::TelemetryDiagnosticRoutinesService>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void TelemetryDiagnosticsRoutineServiceAsh::CreateRoutine(
    crosapi::mojom::TelemetryDiagnosticRoutineArgumentPtr routine_argument,
    mojo::PendingReceiver<crosapi::mojom::TelemetryDiagnosticRoutineControl>
        routine_receiver,
    mojo::PendingRemote<crosapi::mojom::TelemetryDiagnosticRoutineObserver>
        observer) {
  NOTIMPLEMENTED();
}

}  // namespace ash
