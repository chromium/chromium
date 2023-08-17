// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_converters.h"

#include <cstdint>

#include "base/notreached.h"
#include "base/uuid.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"

namespace chromeos::converters::routines {

namespace {
namespace cx_diag = api::os_diagnostics;
namespace crosapi = ::crosapi::mojom;
}  // namespace

namespace unchecked {

cx_diag::RoutineInitializedInfo UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticRoutineStateInitializedPtr input,
    base::Uuid uuid) {
  cx_diag::RoutineInitializedInfo result;
  result.uuid = uuid.AsLowercaseString();
  return result;
}

cx_diag::RoutineRunningInfo UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticRoutineStateRunningPtr input,
    base::Uuid uuid,
    uint32_t percentage) {
  cx_diag::RoutineRunningInfo result;
  result.uuid = uuid.AsLowercaseString();
  result.percentage = percentage;
  return result;
}

cx_diag::RoutineWaitingInfo UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticRoutineStateWaitingPtr input,
    base::Uuid uuid,
    uint32_t percentage) {
  cx_diag::RoutineWaitingInfo result;
  result.uuid = uuid.AsLowercaseString();
  result.reason = Convert(input->reason);
  result.message = input->message;
  result.percentage = percentage;
  return result;
}

}  // namespace unchecked

cx_diag::RoutineWaitingReason Convert(
    crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason input) {
  switch (input) {
    case crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason::
        kUnmappedEnumField:
      return cx_diag::RoutineWaitingReason::kNone;
    case crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason::
        kWaitingToBeScheduled:
      return cx_diag::RoutineWaitingReason::kWaitingToBeScheduled;
    case crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason::
        kWaitingUserInput:
      return cx_diag::RoutineWaitingReason::kWaitingUserInput;
  }
  NOTREACHED_NORETURN();
}

}  // namespace chromeos::converters::routines
