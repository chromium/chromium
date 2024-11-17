// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/common/histogram_util.h"

#include <string_view>

#include "ash/webui/diagnostics_ui/backend/common/routine_properties.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace ash {
namespace diagnostics {
namespace metrics {

namespace {

constexpr char kProbeErrorMetricBatteryInfoSource[] =
    "ChromeOS.DiagnosticsUi.Error.CrosHealthdProbeError.BatteryInfo";
constexpr char kProbeErrorMetricCpuInfoSource[] =
    "ChromeOS.DiagnosticsUi.Error.CrosHealthdProbeError.CpuInfo";
constexpr char kProbeErrorMetricMemoryInfoSource[] =
    "ChromeOS.DiagnosticsUi.Error.CrosHealthdProbeError.MemoryInfo";
constexpr char kProbeErrorMetricSystemInfoSource[] =
    "ChromeOS.DiagnosticsUi.Error.CrosHealthdProbeError.SystemInfo";

// Source type matches |type_name| from cros_healthd_helpers.
const std::string GetMetricNameForSourceType(std::string_view source_type) {
  if (source_type == "battery info")
    return kProbeErrorMetricBatteryInfoSource;
  if (source_type == "cpu info")
    return kProbeErrorMetricCpuInfoSource;
  if (source_type == "memory info")
    return kProbeErrorMetricMemoryInfoSource;
  if (source_type == "system info")
    return kProbeErrorMetricSystemInfoSource;

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

void EmitBatteryDataError(DataError error) {
  base::UmaHistogramEnumeration("ChromeOS.DiagnosticsUi.Error.Battery", error);
}

void EmitNetworkDataError(DataError error) {
  base::UmaHistogramEnumeration("ChromeOS.DiagnosticsUi.Error.Network", error);
}

void EmitCrosHealthdProbeError(std::string_view source_type,
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

void EmitKeyboardTesterRoutineDuration(
    const base::TimeDelta& keyboard_tester_routine_duration) {
  base::UmaHistogramLongTimes100(
      "ChromeOS.DiagnosticsUi.KeyboardTesterRoutineDuration",
      keyboard_tester_routine_duration);
}

}  // namespace metrics
}  // namespace diagnostics
}  // namespace ash
