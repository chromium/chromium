// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_HISTOGRAM_UTIL_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_HISTOGRAM_UTIL_H_

#include <stddef.h>
#include <cstdint>

#include "ash/webui/diagnostics_ui/mojom/system_routine_controller.mojom.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ash {
namespace diagnostics {
namespace metrics {

void EmitAppOpenDuration(const base::TimeDelta& time_elapsed);

void EmitMemoryRoutineDuration(const base::TimeDelta& memory_routine_duration);

void EmitRoutineRunCount(uint16_t routine_count);

void EmitRoutineResult(mojom::RoutineType routine_type,
                       mojom::StandardRoutineResult result);

}  // namespace metrics
}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_HISTOGRAM_UTIL_H_
