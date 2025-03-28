// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_METRICS_RECORDER_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_METRICS_RECORDER_H_

#include "base/time/time.h"

namespace task_manager {

// %s represents the ith process being ended.
// i.e. FirstProcessEnded, SecondProcessEnded, ... up to (and including) the
// Fifth process. Any process after the fifth one is discarded and not recorded.
inline constexpr char kTimeToEndProcessHistogram[] =
    "TaskManager.%sProcessEnded.ElapsedTime";
inline constexpr char kClosedElapsedTimeHistogram[] =
    "TaskManager.Closed.ElapsedTime";
inline constexpr char kClosedTabElapsedTimeHistogram[] =
    "TaskManager.Closed.%s.ElapsedTime";
inline constexpr char kStartActionHistogram[] = "TaskManager.Opened";

// Represents how the Task Manager was started.
// Used for histograms. Current values should not be renumbered or removed.
// Please keep in sync with "StartAction" (see lint).
// LINT.IfChange(StartAction)
enum class StartAction {
  kOther = 0,
  kContextMenu = 1,
  kMoreTools = 2,
  kShortcut = 3,
  kMainMenu = 4,
  kMaxValue = kMainMenu,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/task_manager/enums.xml:StartAction)

// Not an enum used in UMA, rather it's a loose mapping to DisplayCategory.
enum class CategoryRecord {
  kOther = 0,
  kTabsAndExtensions = 1,
  kBrowser = 2,
  kSystem = 3,
  kAll = 4,
  kMaxValue = kAll,
};

// Records metrics and events that happen on Task Manager.
void RecordNewOpenEvent(StartAction action);
void RecordCloseEvent(const base::TimeTicks& start_time,
                      const base::TimeTicks& end_time);
void RecordEndProcessEvent(const base::TimeTicks& start_time,
                           const base::TimeTicks& end_time,
                           size_t end_process_count);
void RecordTabSwitchEvent(CategoryRecord old_category,
                          const base::TimeDelta& time_spent);

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_METRICS_RECORDER_H_
