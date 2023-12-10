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

crosapi::TelemetryDiagnosticVolumeButtonRoutineDetailPtr UncheckedConvertPtr(
    healthd::VolumeButtonRoutineDetailPtr input) {
  return crosapi::TelemetryDiagnosticVolumeButtonRoutineDetail::New();
}

crosapi::TelemetryDiagnosticFanRoutineDetailPtr UncheckedConvertPtr(
    healthd::FanRoutineDetailPtr input) {
  return crosapi::TelemetryDiagnosticFanRoutineDetail::New(
      input->passed_fan_ids, input->failed_fan_ids,
      Convert(input->fan_count_status));
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
    case healthd::RoutineDetail::Tag::kUnrecognizedArgument:
      return crosapi::TelemetryDiagnosticRoutineDetail::NewUnrecognizedArgument(
          input->get_unrecognizedArgument());
    case healthd::RoutineDetail::Tag::kMemory:
      return crosapi::TelemetryDiagnosticRoutineDetail::NewMemory(
          ConvertRoutinePtr(std::move(input->get_memory())));
    case healthd::RoutineDetail::Tag::kVolumeButton:
      return crosapi::TelemetryDiagnosticRoutineDetail::NewVolumeButton(
          ConvertRoutinePtr(std::move(input->get_volume_button())));
    case healthd::RoutineDetail::Tag::kFan:
      return crosapi::TelemetryDiagnosticRoutineDetail::NewFan(
          ConvertRoutinePtr(std::move(input->get_fan())));
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
    case healthd::RoutineStateUnion::Tag::kUnrecognizedArgument:
      return crosapi::TelemetryDiagnosticRoutineStateUnion::
          NewUnrecognizedArgument(input->get_unrecognizedArgument());
    case healthd::RoutineStateUnion::Tag::kInitialized:
      return crosapi::TelemetryDiagnosticRoutineStateUnion::NewInitialized(
          ConvertRoutinePtr(std::move(input->get_initialized())));
    case healthd::RoutineStateUnion::Tag::kRunning:
      return crosapi::TelemetryDiagnosticRoutineStateUnion::NewRunning(
          ConvertRoutinePtr(std::move(input->get_running())));
    case healthd::RoutineStateUnion::Tag::kWaiting:
      return crosapi::TelemetryDiagnosticRoutineStateUnion::NewWaiting(
          ConvertRoutinePtr(std::move(input->get_waiting())));
    case healthd::RoutineStateUnion::Tag::kFinished:
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
    case crosapi::TelemetryDiagnosticRoutineArgument::Tag::
        kUnrecognizedArgument:
      return healthd::RoutineArgument::NewUnrecognizedArgument(
          std::move(input->get_unrecognizedArgument()));
    case crosapi::TelemetryDiagnosticRoutineArgument::Tag::kMemory:
      return healthd::RoutineArgument::NewMemory(
          ConvertRoutinePtr(std::move(input->get_memory())));
    case crosapi::TelemetryDiagnosticRoutineArgument::Tag::kVolumeButton:
      return healthd::RoutineArgument::NewVolumeButton(
          ConvertRoutinePtr(std::move(input->get_volume_button())));
    case crosapi::TelemetryDiagnosticRoutineArgument::Tag::kFan:
      return healthd::RoutineArgument::NewFan(
          ConvertRoutinePtr(std::move(input->get_fan())));
  }
}

healthd::MemoryRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticMemoryRoutineArgumentPtr input) {
  return healthd::MemoryRoutineArgument::New(
      std::move(input->max_testing_mem_kib));
}

healthd::VolumeButtonRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticVolumeButtonRoutineArgumentPtr input) {
  auto arg = healthd::VolumeButtonRoutineArgument::New();
  switch (input->type) {
    case crosapi::TelemetryDiagnosticVolumeButtonRoutineArgument::ButtonType::
        kUnmappedEnumField:
      arg->type =
          healthd::VolumeButtonRoutineArgument::ButtonType::kUnmappedEnumField;
      break;
    case crosapi::TelemetryDiagnosticVolumeButtonRoutineArgument::ButtonType::
        kVolumeUp:
      arg->type = healthd::VolumeButtonRoutineArgument::ButtonType::kVolumeUp;
      break;
    case crosapi::TelemetryDiagnosticVolumeButtonRoutineArgument::ButtonType::
        kVolumeDown:
      arg->type = healthd::VolumeButtonRoutineArgument::ButtonType::kVolumeDown;
      break;
  }
  arg->timeout = input->timeout;
  return arg;
}

healthd::FanRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::TelemetryDiagnosticFanRoutineArgumentPtr input) {
  return healthd::FanRoutineArgument::New();
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
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kEightBitWrites;
    case healthd::MemtesterTestItemEnum::k16BitWrites:
      return crosapi::TelemetryDiagnosticMemtesterTestItemEnum::
          kSixteenBitWrites;
  }
  NOTREACHED_NORETURN();
}

crosapi::TelemetryDiagnosticHardwarePresenceStatus Convert(
    healthd::HardwarePresenceStatus input) {
  switch (input) {
    case healthd::HardwarePresenceStatus::kUnmappedEnumField:
      return crosapi::TelemetryDiagnosticHardwarePresenceStatus::
          kUnmappedEnumField;
    case healthd::HardwarePresenceStatus::kMatched:
      return crosapi::TelemetryDiagnosticHardwarePresenceStatus::kMatched;
    case healthd::HardwarePresenceStatus::kNotMatched:
      return crosapi::TelemetryDiagnosticHardwarePresenceStatus::kNotMatched;
    case healthd::HardwarePresenceStatus::kNotConfigured:
      return crosapi::TelemetryDiagnosticHardwarePresenceStatus::kNotConfigured;
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
