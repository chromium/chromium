// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine.h"

#include <utility>

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_observation.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

DiagnosticRoutine::DiagnosticRoutine(
    mojo::PendingRemote<crosapi::mojom::TelemetryDiagnosticRoutineControl>
        control_remote,
    mojo::PendingReceiver<crosapi::mojom::TelemetryDiagnosticRoutineObserver>
        observer_receiver,
    RoutineInfo info)
    : routine_control_(std::move(control_remote)),
      observation_(info.extension_id,
                   info.uuid,
                   info.browser_context,
                   std::move(observer_receiver)) {}

DiagnosticRoutine::~DiagnosticRoutine() = default;

mojo::Remote<crosapi::mojom::TelemetryDiagnosticRoutineControl>&
DiagnosticRoutine::GetRemote() {
  return routine_control_;
}

}  // namespace chromeos
