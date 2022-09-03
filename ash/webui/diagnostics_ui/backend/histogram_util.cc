// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/histogram_util.h"

#include "ash/webui/diagnostics_ui/backend/routine_properties.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace ash {
namespace diagnostics {
namespace metrics {

namespace {

constexpr char kProbeErrorMetricBatteryInfoSource[] =
    "ChromeOS.DiagnosticsUi.Error.CrosHealthdProbeError.BatteryInfo";

// Source type matches |type_name| from cros_healthd_helpers.
const std::string GetMetricNameForSourceType(
    const base::StringPiece source_type) {
  if (source_type == "battery info")
    return kProbeErrorMetricBatteryInfoSource;

  return "";
}

}  // namespace

void EmitAppOpenDuration(const base::TimeDelta& time_elapsed) {
  base::UmaHistogramLongTimes100("ChromeOS.DiagnosticsUi.OpenDuration",
                                 time_elapsed);
}

void EmitMemoryRoutineDuration(const base::TimeDelta& memory_routine_duration) {
  base::UmaHistogramLongTimes100("ChromeOS.DiagnosticsUi.MemoryRoutineDuration",
                                 memory_routine_duration);
}

void EmitRoutineRunCount(uint16_t routine_count) {
  base::UmaHistogramCounts100("ChromeOS.DiagnosticsUi.RoutineCount",
                              routine_count);
}

void EmitRoutineResult(mojom::RoutineType routine_type,
                       mojom::StandardRoutineResult result) {
  base::UmaHistogramEnumeration(std::string("ChromeOS.DiagnosticsUi.") +
                                    GetRoutineMetricName(routine_type),
                                result);
  return;
}

void EmitSystemDataError(DataError error) {
  base::UmaHistogramEnumeration("ChromeOS.DiagnosticsUi.Error.System", error);
}

void EmitCrosHealthdProbeError(const base::StringPiece source_type,
                               cros_healthd::mojom::ErrorType error_type) {
  const std::string& metric_name = GetMetricNameForSourceType(source_type);

  // |metric_name| may be empty in which case we do not want a metric send
  // attempted.
  if (metric_name.empty()) {
    LOG(WARNING)
        << "Ignoring request to record metric for ProbeError of error_type: "
        << error_type << " for unknown source_stuct: " << source_type;
    return;
  }

  base::UmaHistogramEnumeration(metric_name, error_type);
}

}  // namespace metrics
}  // namespace diagnostics
}  // namespace ash
