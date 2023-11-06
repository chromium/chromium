// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_CONVERTERS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_CONVERTERS_H_

#include <cstdint>
#include <type_traits>
#include <utility>

#include "base/uuid.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"

// This file contains helper functions used by the routine API to convert its
// types to/from telemetry service types.
namespace chromeos::converters::routines {

// Functions in unchecked namespace do not verify whether input pointer is
// nullptr, they should be called only via ConvertPtr wrapper that checks
// whether input pointer is nullptr.
namespace unchecked {

api::os_diagnostics::RoutineInitializedInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticRoutineStateInitializedPtr input,
    base::Uuid uuid);

api::os_diagnostics::RoutineRunningInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticRoutineStateRunningPtr input,
    base::Uuid uuid,
    uint32_t percentage);

api::os_diagnostics::RoutineWaitingInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticRoutineStateWaitingPtr input,
    base::Uuid uuid,
    uint32_t percentage);

api::os_diagnostics::MemtesterResult UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticMemtesterResultPtr input);

api::os_diagnostics::MemoryRoutineFinishedInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticMemoryRoutineDetailPtr input,
    base::Uuid uuid,
    bool has_passed);

api::os_diagnostics::VolumeButtonRoutineFinishedInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticVolumeButtonRoutineDetailPtr input,
    base::Uuid uuid,
    bool has_passed);

}  // namespace unchecked

api::os_diagnostics::ExceptionReason Convert(
    crosapi::mojom::TelemetryExtensionException::Reason input);

api::os_diagnostics::RoutineWaitingReason Convert(
    crosapi::mojom::TelemetryDiagnosticRoutineStateWaiting::Reason input);

api::os_diagnostics::MemtesterTestItemEnum Convert(
    crosapi::mojom::TelemetryDiagnosticMemtesterTestItemEnum input);

template <class InputT,
          class OutputT = decltype(Convert(std::declval<InputT>())),
          class = std::enable_if_t<std::is_enum_v<InputT> ||
                                   std::is_integral_v<InputT>>>
std::vector<OutputT> ConvertVector(std::vector<InputT> input) {
  std::vector<OutputT> output;
  for (auto elem : input) {
    output.push_back(Convert(std::move(elem)));
  }
  return output;
}

template <class InputT,
          class... Types,
          class OutputT = decltype(unchecked::UncheckedConvertPtr(
              std::declval<InputT>(),
              std::declval<Types>()...)),
          class = std::enable_if_t<std::is_default_constructible_v<OutputT>>>
OutputT ConvertPtr(InputT input, Types... args) {
  return (input) ? unchecked::UncheckedConvertPtr(std::move(input), args...)
                 : OutputT();
}

}  // namespace chromeos::converters::routines

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_CONVERTERS_H_
