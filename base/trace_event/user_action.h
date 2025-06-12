// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_USER_ACTION_H_
#define BASE_TRACE_EVENT_USER_ACTION_H_

#include <stdint.h>

#include <string>

#include "base/base_export.h"
#include "base/time/time.h"

namespace base::trace_event {

// Emits an trace event for an action that the user performed.
// See base/metrics/user_metrics.h for more details.
void BASE_EXPORT EmitUserActionEvent(const std::string& action,
                                     TimeTicks action_time);

}  // namespace base::trace_event

#endif  // BASE_TRACE_EVENT_USER_ACTION_H_
