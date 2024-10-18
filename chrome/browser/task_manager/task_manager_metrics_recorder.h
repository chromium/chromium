// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_METRICS_RECORDER_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_METRICS_RECORDER_H_

#include "base/time/time.h"

namespace task_manager {

inline constexpr char kClosedElapsedTimeHistogram[] =
    "TaskManager.Closed.ElapsedTime";
inline constexpr char kStartActionHistogram[] = "TaskManager.Opened";

// Represents how the Task Manager was started.
// Used for histograms. Current values should not be renumbered or removed.
// Please keep in sync with "StartAction" (see lint).
// LINT.IfChange(StartAction)
enum class StartAction {
  kAnyDebug = 0,
  kContextMenu = 1,
  kMoreTools = 2,
  kShortcut = 3,
  kMaxValue = kShortcut,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/task_manager/enums.xml:StartAction)

// Records metrics and events that happen on Task Manager.
void RecordNewOpenEvent(StartAction action);
void RecordCloseEvent(const base::TimeTicks& start_time,
                      const base::TimeTicks& end_time);

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_METRICS_RECORDER_H_
