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
  }
}

}  // namespace unchecked

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
