// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_COMMON_HISTOGRAM_UTIL_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_COMMON_HISTOGRAM_UTIL_H_

#include <stddef.h>

#include <cstdint>
#include <string_view>

#include "ash/webui/diagnostics_ui/mojom/system_routine_controller.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-shared.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ash {
namespace diagnostics {
namespace metrics {

// The enums below are used in histograms, do not remove/renumber entries. If
// you're adding to any of these enums, update the corresponding enum listing in
// tools/metrics/histograms/enums.xml: CrosDiagnosticsDataError.
enum class DataError {
  // Null or nullptr value.
  kNoData = 0,
  // For numeric values that are NaN.
  kNotANumber = 1,
  // Expectation about data not met. Ex. routing prefix is between zero and
  // thirty-two.
  kExpectationNotMet = 2,
  kMaxValue = kExpectationNotMet,
};

void EmitAppOpenDuration(const base::TimeDelta& time_elapsed);

void EmitMemoryRoutineDuration(const base::TimeDelta& memory_routine_duration);

void EmitRoutineRunCount(uint16_t routine_count);

void EmitRoutineResult(mojom::RoutineType routine_type,
                       mojom::StandardRoutineResult result);

void EmitSystemDataError(DataError error);

void EmitBatteryDataError(DataError error);

void EmitNetworkDataError(DataError error);

// Tracks type and source struct of errors from calls to cros_healthd probe
// service. `source_type` matches the `type_name` lookup in
// cros_healthd_helpers.
void EmitCrosHealthdProbeError(std::string_view source_type,
                               cros_healthd::mojom::ErrorType error_type);

void EmitKeyboardTesterRoutineDuration(
    const base::TimeDelta& keyboard_tester_routine_duration);

}  // namespace metrics
}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_COMMON_HISTOGRAM_UTIL_H_
