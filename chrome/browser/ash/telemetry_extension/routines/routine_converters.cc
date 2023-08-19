// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/routines/routine_converters.h"

#include <utility>

#include "base/notreached.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"

namespace ash::converters {

namespace {

namespace crosapi = ::crosapi::mojom;
namespace healthd = cros_healthd::mojom;

}  // namespace

namespace unchecked {

crosapi::TelemetryDiagnosticMemtesterResultPtr UncheckedConvertPtr(
    healthd::MemtesterResultPtr input) {
  return crosapi::TelemetryDiagnosticMemtesterResult::New(
      ConvertVector(input->passed_items), ConvertVector(input->failed_items));
}

crosapi::TelemetryDiagnosticMemoryRoutineDetailPtr UncheckedConvertPtr(
    healthd::MemoryRoutineDetailPtr input) {
  return crosapi::TelemetryDiagnosticMemoryRoutineDetail::New(
      input->bytes_tested, ConvertRoutinePtr(std::move(input->result)));
}

crosapi::TelemetryDiagnosticRoutineStateInitializedPtr UncheckedConvertPtr(
    healthd::RoutineStateInitializedPtr input) {
  return crosapi::TelemetryDiagnosticRoutineStateInitialized::New();
}

crosapi::TelemetryDiagnosticRoutineStateRunningPtr UncheckedConvertPtr(
    healthd::RoutineStateRunningPtr input) {
  return crosapi::TelemetryDiagnosticRoutineStateRunning::New();
}

crosapi::TelemetryDiagnosticRoutineStateWaitingPtr UncheckedConvertPtr(
    healthd::RoutineStateWaitingPtr input) {
  return crosapi::TelemetryDiagnosticRoutineStateWaiting::New(
      Convert(input->reason), input->message);
}

crosapi::TelemetryDiagnosticRoutineDetailPtr UncheckedConvertPtr(
    healthd::RoutineDetailPtr input) {
  switch (input->which()) {
    case healthd::internal::RoutineDetail_Data::RoutineDetail_Tag::
        kUnrecognizedArgument:
      return crosapi::TelemetryDiagnosticRoutineDetail::NewUnrecognizedArgument(
          input->get_unrecognizedArgument());
    case healthd::internal::RoutineDetail_Data::RoutineDetail_Tag::kMemory:
      return crosapi::TelemetryDiagnosticRoutineDetail::NewMemory(
          ConvertRoutinePtr(std::move(input->get_memory())));
  }
  NOTREACHED_NORETURN();
}

crosapi::TelemetryDiagnosticRoutineStateFinishedPtr UncheckedConvertPtr(
    healthd::RoutineStateFinishedPtr input) {
  return crosapi::TelemetryDiagnosticRoutineStateFinished::New(
      input->has_passed, ConvertRoutinePtr(std::move(input->detail)));
}

crosapi::TelemetryDiagnosticRoutineStateUnionPtr UncheckedConvertPtr(
    healthd::RoutineStateUnionPtr input) {
  switch (input->which()) {
    case healthd::internal::RoutineStateUnion_Data::RoutineStateUnion_Tag::
        kUnrecognizedArgument:
      return crosapi::TelemetryDiagnosticRoutineStateUnion::
          NewUnrecognizedArgument(input->get_unrecognizedArgument());
    case healthd::internal::RoutineStateUnion_Data::RoutineStateUnion_Tag::
        kInitialized:
      return crosapi::TelemetryDiagnosticRoutineStateUnion::NewInitialized(
          ConvertRoutinePtr(std::move(input->get_initialized())));
    case healthd::internal::RoutineStateUnion_Data::RoutineStateUnion_Tag::
        kRunning:
      return crosapi::TelemetryDiagnosticRoutineStateUnion::NewRunning(
          ConvertRoutinePtr(std::move(input->get_running())));
    case healthd::internal::RoutineStateUnion_Data::RoutineStateUnion_Tag::
        kWaiting:
      return crosapi::TelemetryDiagnosticRoutineStateUnion::NewWaiting(
          ConvertRoutinePtr(std::move(input->get_waiting())));
    case healthd::internal::RoutineStateUnion_Data::RoutineStateUnion_Tag::
        kFinished:
      return crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
          ConvertRoutinePtr(std::move(input->get_finished())));
  }
  NOTREACHED_NORETURN();
}

crosapi::TelemetryDiagnosticRoutineStatePtr UncheckedConvertPtr(
    healthd::RoutineStatePtr input) {
  return crosapi::TelemetryDiagnosticRoutineState::New(
      input->percentage, ConvertRoutinePtr(std::move(input->state_union)));
}

healthd::RoutineArgumentPtr UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticRoutineArgumentPtr input) {
  switch (input->which()) {
    case crosapi::internal::TelemetryDiagnosticRoutineArgument_Data::
        TelemetryDiagnosticRoutineArgument_Tag::kUnrecognizedArgument:
      return healthd::RoutineArgument::NewUnrecognizedArgument(
          std::move(input->get_unrecognizedArgument()));
    case crosapi::internal::TelemetryDiagnosticRoutineArgument_Data::
        TelemetryDiagnosticRoutineArgument_Tag::kMemory:
      return healthd::RoutineArgument::NewMemory(
          ConvertRoutinePtr(std::move(input->get_memory())));
  }
}

healthd::MemoryRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticMemoryRoutineArgumentPtr input) {
  return healthd::MemoryRoutineArgument::New(
      std::move(input->max_testing_mem_kib));
}

}  // namespace unchecked

crosapi::TelemetryDiagnosticMemtesterTestItemEnum Convert(
    healthd::MemtesterTestItemEnum input) {
  switch (input) {
    case healthd::MemtesterTestItemEnum::kUnmappedEnumField:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::
          kUnmappedEnumField;
    case healthd::MemtesterTestItemEnum::kUnknown:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kUnknown;
    case healthd::MemtesterTestItemEnum::kStuckAddress:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kStuckAddress;
    case healthd::MemtesterTestItemEnum::kCompareAND:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareAND;
    case healthd::MemtesterTestItemEnum::kCompareDIV:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareDIV;
    case healthd::MemtesterTestItemEnum::kCompareMUL:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareMUL;
    case healthd::MemtesterTestItemEnum::kCompareOR:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareOR;
    case healthd::MemtesterTestItemEnum::kCompareSUB:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareSUB;
    case healthd::MemtesterTestItemEnum::kCompareXOR:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareXOR;
    case healthd::MemtesterTestItemEnum::kSequentialIncrement:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::
          kSequentialIncrement;
    case healthd::MemtesterTestItemEnum::kBitFlip:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBitFlip;
    case healthd::MemtesterTestItemEnum::kBitSpread:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kBitSpread;
    case healthd::MemtesterTestItemEnum::kBlockSequential:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::
          kBlockSequential;
    case healthd::MemtesterTestItemEnum::kCheckerboard:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCheckerboard;
    case healthd::MemtesterTestItemEnum::kRandomValue:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kRandomValue;
    case healthd::MemtesterTestItemEnum::kSolidBits:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kSolidBits;
    case healthd::MemtesterTestItemEnum::kWalkingOnes:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kWalkingOnes;
    case healthd::MemtesterTestItemEnum::kWalkingZeroes:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kWalkingZeroes;
    case healthd::MemtesterTestItemEnum::k8BitWrites:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::k8BitWrites;
    case healthd::MemtesterTestItemEnum::k16BitWrites:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::k16BitWrites;
  }
  NOTREACHED_NORETURN();
}

crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason Convert(
    healthd::RoutineStateWaiting::Reason input) {
  switch (input) {
    case healthd::RoutineStateWaiting_Reason::kUnmappedEnumField:
      return crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason::
          kUnmappedEnumField;
    case healthd::RoutineStateWaiting_Reason::kWaitingToBeScheduled:
      return crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason::
          kWaitingToBeScheduled;
    case healthd::RoutineStateWaiting_Reason::kWaitingUserInput:
      return crosapi::TelemetryDiagnosticRoutineStateWaiting_Reason::
          kWaitingUserInput;
  }
  NOTREACHED_NORETURN();
}

}  // namespace ash::converters
