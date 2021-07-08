// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/histogram_util.h"

#include "ash/webui/diagnostics_ui/backend/routine_properties.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace ash {
namespace diagnostics {
namespace metrics {

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

}  // namespace metrics
}  // namespace diagnostics
}  // namespace ash
