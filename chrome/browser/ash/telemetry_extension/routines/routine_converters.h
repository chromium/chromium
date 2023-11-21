// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_CONVERTERS_H_
#define CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_CONVERTERS_H_

#include <type_traits>
#include <utility>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"

namespace ash::converters {

// This file contains helper functions used by
// TelemetryDiagnosticsRoutineServiceAsh to convert its types to/from
// cros_healthd routine types.

// Contains conversion functions that skip checking the `mojo::InlinedStructPtr`
// for null.
namespace unchecked {

crosapi::mojom::TelemetryDiagnosticMemtesterResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::MemtesterResultPtr input);

crosapi::mojom::TelemetryDiagnosticMemoryRoutineDetailPtr UncheckedConvertPtr(
    cros_healthd::mojom::MemoryRoutineDetailPtr input);

crosapi::mojom::TelemetryDiagnosticVolumeButtonRoutineDetailPtr
UncheckedConvertPtr(cros_healthd::mojom::VolumeButtonRoutineDetailPtr input);

crosapi::mojom::TelemetryDiagnosticFanRoutineDetailPtr UncheckedConvertPtr(
    cros_healthd::mojom::FanRoutineDetailPtr input);

crosapi::mojom::TelemetryDiagnosticRoutineStateInitializedPtr
UncheckedConvertPtr(cros_healthd::mojom::RoutineStateInitializedPtr input);

crosapi::mojom::TelemetryDiagnosticRoutineStateRunningPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineStateRunningPtr input);

crosapi::mojom::TelemetryDiagnosticRoutineStateWaitingPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineStateWaitingPtr input);

crosapi::mojom::TelemetryDiagnosticRoutineDetailPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineDetailPtr input);

crosapi::mojom::TelemetryDiagnosticRoutineStateFinishedPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineStateFinishedPtr input);

crosapi::mojom::TelemetryDiagnosticRoutineStateUnionPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineStateUnionPtr input);

crosapi::mojom::TelemetryDiagnosticRoutineStatePtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineStatePtr input);

cros_healthd::mojom::RoutineArgumentPtr UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticRoutineArgumentPtr input);

cros_healthd::mojom::MemoryRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticMemoryRoutineArgumentPtr input);

cros_healthd::mojom::VolumeButtonRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticVolumeButtonRoutineArgumentPtr input);

cros_healthd::mojom::FanRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticFanRoutineArgumentPtr input);

}  // namespace unchecked

crosapi::mojom::TelemetryDiagnosticMemtesterTestItemEnum Convert(
    cros_healthd::mojom::MemtesterTestItemEnum input);

crosapi::mojom::TelemetryDiagnosticHardwarePresenceStatus Convert(
    cros_healthd::mojom::HardwarePresenceStatus input);

crosapi::mojom::TelemetryDiagnosticRoutineStateWaiting::Reason Convert(
    cros_healthd::mojom::RoutineStateWaiting::Reason input);

template <class InputT,
          class OutputT = decltype(Convert(std::declval<InputT>())),
          class = std::enable_if_t<std::is_enum_v<InputT>, bool>>
std::vector<OutputT> ConvertVector(std::vector<InputT> input) {
  std::vector<OutputT> result;
  for (auto elem : input) {
    result.push_back(Convert(elem));
  }
  return result;
}

template <class InputT>
auto ConvertRoutinePtr(InputT input) {
  return (!input.is_null()) ? unchecked::UncheckedConvertPtr(std::move(input))
                            : nullptr;
}

}  // namespace ash::converters

#endif  // CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_CONVERTERS_H_
