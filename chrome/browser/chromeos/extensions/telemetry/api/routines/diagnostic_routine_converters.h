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
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_exception.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"

// This file contains helper functions used by the routine API to convert its
// types to/from telemetry service types.
namespace chromeos::converters::routines {

// Functions in unchecked namespace do not verify whether input pointer is
// nullptr, they should be called only via ConvertPtr wrapper that checks
// whether input pointer is nullptr.
namespace unchecked {

api::os_diagnostics::RoutineInitializedInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::RoutineStateInitializedPtr input,
    base::Uuid uuid);

api::os_diagnostics::RoutineRunningInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::RoutineStateRunningPtr input,
    base::Uuid uuid,
    uint32_t percentage);

api::os_diagnostics::NetworkBandwidthRoutineRunningInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::NetworkBandwidthRoutineRunningInfoPtr input);

api::os_diagnostics::RoutineInquiryUnion UncheckedConvertPtr(
    ash::cros_healthd::mojom::RoutineInquiryPtr input);

api::os_diagnostics::RoutineInteractionUnion UncheckedConvertPtr(
    ash::cros_healthd::mojom::RoutineInteractionPtr input);

api::os_diagnostics::RoutineWaitingInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::RoutineStateWaitingPtr input,
    base::Uuid uuid,
    uint32_t percentage);

api::os_diagnostics::MemtesterResult UncheckedConvertPtr(
    ash::cros_healthd::mojom::MemtesterResultPtr input);

// For legacy finished events.
// TODO(b/331540565): Remove this function after the legacy event is removed.
api::os_diagnostics::LegacyMemoryRoutineFinishedInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::MemoryRoutineDetailPtr input,
    base::Uuid uuid,
    bool has_passed);

// For legacy finished events.
// TODO(b/331540565): Remove this function after the legacy event is removed.
api::os_diagnostics::LegacyFanRoutineFinishedInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::FanRoutineDetailPtr input,
    base::Uuid uuid,
    bool has_passed);

api::os_diagnostics::MemoryRoutineFinishedDetail UncheckedConvertPtr(
    ash::cros_healthd::mojom::MemoryRoutineDetailPtr input);

api::os_diagnostics::FanRoutineFinishedDetail UncheckedConvertPtr(
    ash::cros_healthd::mojom::FanRoutineDetailPtr input);

api::os_diagnostics::NetworkBandwidthRoutineFinishedDetail UncheckedConvertPtr(
    ash::cros_healthd::mojom::NetworkBandwidthRoutineDetailPtr input);

api::os_diagnostics::CameraFrameAnalysisRoutineFinishedDetail
UncheckedConvertPtr(
    ash::cros_healthd::mojom::CameraFrameAnalysisRoutineDetailPtr input);

api::os_diagnostics::RoutineFinishedInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::RoutineStateFinishedPtr input,
    base::Uuid uuid,
    bool has_passed);

}  // namespace unchecked

api::os_diagnostics::ExceptionReason Convert(
    ash::cros_healthd::mojom::Exception::Reason input);

api::os_diagnostics::RoutineWaitingReason Convert(
    ash::cros_healthd::mojom::RoutineStateWaiting::Reason input);

api::os_diagnostics::MemtesterTestItemEnum Convert(
    ash::cros_healthd::mojom::MemtesterTestItemEnum input);

api::os_diagnostics::HardwarePresenceStatus Convert(
    ash::cros_healthd::mojom::HardwarePresenceStatus input);

api::os_diagnostics::NetworkBandwidthRoutineRunningType Convert(
    ash::cros_healthd::mojom::NetworkBandwidthRoutineRunningInfo::Type input);

api::os_diagnostics::CameraFrameAnalysisIssue Convert(
    ash::cros_healthd::mojom::CameraFrameAnalysisRoutineDetail::Issue input);

api::os_diagnostics::CameraSubtestResult Convert(
    ash::cros_healthd::mojom::CameraSubtestResult input);

template <class InputT,
          class OutputT = decltype(Convert(std::declval<InputT>()))>
  requires(std::is_enum_v<InputT> || std::is_integral_v<InputT>)
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
              std::declval<Types>()...))>
  requires(std::is_default_constructible_v<OutputT>)
OutputT ConvertPtr(InputT input, Types... args) {
  return (input) ? unchecked::UncheckedConvertPtr(std::move(input), args...)
                 : OutputT();
}

}  // namespace chromeos::converters::routines

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_CONVERTERS_H_
