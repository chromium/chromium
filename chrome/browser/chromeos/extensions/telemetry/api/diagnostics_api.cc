// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"

namespace chromeos {

// runBatteryCapacityRoutine ---------------------------------------------------

namespace {

api::os_diagnostics::RoutineStatus ConvertRoutineStatus(
    health::mojom::DiagnosticRoutineStatusEnum status) {
  switch (status) {
    case health::mojom::DiagnosticRoutineStatusEnum::kReady:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_READY;
    case health::mojom::DiagnosticRoutineStatusEnum::kRunning:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_RUNNING;
    case health::mojom::DiagnosticRoutineStatusEnum::kWaiting:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_WAITING;
    case health::mojom::DiagnosticRoutineStatusEnum::kPassed:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_PASSED;
    case health::mojom::DiagnosticRoutineStatusEnum::kFailed:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_FAILED;
    case health::mojom::DiagnosticRoutineStatusEnum::kError:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_ERROR;
    case health::mojom::DiagnosticRoutineStatusEnum::kCancelled:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_CANCELLED;
    case health::mojom::DiagnosticRoutineStatusEnum::kFailedToStart:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_FAILED_TO_START;
    case health::mojom::DiagnosticRoutineStatusEnum::kRemoved:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_REMOVED;
    case health::mojom::DiagnosticRoutineStatusEnum::kCancelling:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_CANCELLING;
    case health::mojom::DiagnosticRoutineStatusEnum::kUnsupported:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_UNSUPPORTED;
    case health::mojom::DiagnosticRoutineStatusEnum::kNotRun:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_NOT_RUN;
  }
}

}  // namespace

OsDiagnosticsRunBatteryCapacityRoutineFunction::
    OsDiagnosticsRunBatteryCapacityRoutineFunction()
    : diagnostics_service_(
          remote_diagnostics_service_.BindNewPipeAndPassReceiver()) {}
OsDiagnosticsRunBatteryCapacityRoutineFunction::
    ~OsDiagnosticsRunBatteryCapacityRoutineFunction() = default;

ExtensionFunction::ResponseAction
OsDiagnosticsRunBatteryCapacityRoutineFunction::Run() {
  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb = base::BindOnce(
      &OsDiagnosticsRunBatteryCapacityRoutineFunction::OnResult, this);

  remote_diagnostics_service_->RunBatteryCapacityRoutine(std::move(cb));

  return RespondLater();
}

void OsDiagnosticsRunBatteryCapacityRoutineFunction::OnResult(
    health::mojom::RunRoutineResponsePtr ptr) {
  if (!ptr) {
    // |ptr| should never be null, otherwise Mojo validation will fail.
    // However it's safer to handle it in case of API changes.
    Respond(Error("API internal error"));
    return;
  }

  api::os_diagnostics::RunRoutineResponse result;
  result.id = ptr->id;
  result.status = ConvertRoutineStatus(ptr->status);
  Respond(ArgumentList(
      api::os_diagnostics::RunBatteryCapacityRoutine::Results::Create(result)));
}

}  // namespace chromeos
