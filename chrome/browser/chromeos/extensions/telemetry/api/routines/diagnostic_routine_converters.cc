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

cx_diag::MemtesterResult UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticMemtesterResultPtr input) {
  cx_diag::MemtesterResult result;
  result.passed_items = ConvertVector(input->passed_items);
  result.failed_items = ConvertVector(input->failed_items);

  return result;
}

cx_diag::MemoryRoutineFinishedInfo UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticMemoryRoutineDetailPtr input,
    base::Uuid uuid,
    bool has_passed) {
  cx_diag::MemoryRoutineFinishedInfo result;

  result.uuid = uuid.AsLowercaseString();
  result.has_passed = has_passed;
  result.bytes_tested = input->bytes_tested;
  result.result = ConvertPtr(std::move(input->result));

  return result;
}

}  // namespace unchecked

cx_diag::ExceptionReason Convert(
    crosapi::TelemetryExtensionException::Reason input) {
  switch (input) {
    case crosapi::TelemetryExtensionException::Reason::kUnmappedEnumField:
      return cx_diag::ExceptionReason::kUnknown;
    case crosapi::TelemetryExtensionException::Reason::
        kMojoDisconnectWithoutReason:
      return cx_diag::ExceptionReason::kUnknown;
    case crosapi::TelemetryExtensionException::Reason::kUnexpected:
      return cx_diag::ExceptionReason::kUnexpected;
    case crosapi::TelemetryExtensionException::Reason::kUnsupported:
      return cx_diag::ExceptionReason::kUnsupported;
  }
  NOTREACHED();
}

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

cx_diag::MemtesterTestItemEnum Convert(
    crosapi::TelemetryDiagnosticMemtesterTestItemEnum input) {
  switch (input) {
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kUnmappedEnumField:
      return cx_diag::MemtesterTestItemEnum::kUnknown;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kUnknown:
      return cx_diag::MemtesterTestItemEnum::kUnknown;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kStuckAddress:
      return cx_diag::MemtesterTestItemEnum::kStuckAddress;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareAND:
      return cx_diag::MemtesterTestItemEnum::kCompareAnd;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareDIV:
      return cx_diag::MemtesterTestItemEnum::kCompareDiv;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareMUL:
      return cx_diag::MemtesterTestItemEnum::kCompareMul;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareOR:
      return cx_diag::MemtesterTestItemEnum::kCompareOr;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareSUB:
      return cx_diag::MemtesterTestItemEnum::kCompareSub;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareXOR:
      return cx_diag::MemtesterTestItemEnum::kCompareXor;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::
        kSequentialIncrement:
      return cx_diag::MemtesterTestItemEnum::kSequentialIncrement;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBitFlip:
      return cx_diag::MemtesterTestItemEnum::kBitFlip;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBitSpread:
      return cx_diag::MemtesterTestItemEnum::kBitSpread;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBlockSequential:
      return cx_diag::MemtesterTestItemEnum::kBlockSequential;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCheckerboard:
      return cx_diag::MemtesterTestItemEnum::kCheckerboard;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kRandomValue:
      return cx_diag::MemtesterTestItemEnum::kRandomValue;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kSolidBits:
      return cx_diag::MemtesterTestItemEnum::kSolidBits;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kWalkingOnes:
      return cx_diag::MemtesterTestItemEnum::kWalkingOnes;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kWalkingZeroes:
      return cx_diag::MemtesterTestItemEnum::kWalkingZeroes;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kEightBitWrites:
      return cx_diag::MemtesterTestItemEnum::kEightBitWrites;
    case crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kSixteenBitWrites:
      return cx_diag::MemtesterTestItemEnum::kSixteenBitWrites;
  }
  NOTREACHED_NORETURN();
}

}  // namespace chromeos::converters::routines
