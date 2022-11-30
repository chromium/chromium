// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_TASK_SWITCH_METRICS_RECORDER_H_
#define ASH_METRICS_TASK_SWITCH_METRICS_RECORDER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "ash/ash_export.h"
#include "ash/metrics/task_switch_source.h"

namespace ash {

class TaskSwitchTimeTracker;

// The TaskSwitchMetricsRecorder class records UMA metrics related to task
// switching. The main purpose of the TaskSwitchMetricsRecorder is to track time
// deltas between task switches and record histograms of the deltas.
class ASH_EXPORT TaskSwitchMetricsRecorder {
 public:
  TaskSwitchMetricsRecorder();

  TaskSwitchMetricsRecorder(const TaskSwitchMetricsRecorder&) = delete;
  TaskSwitchMetricsRecorder& operator=(const TaskSwitchMetricsRecorder&) =
      delete;

  virtual ~TaskSwitchMetricsRecorder();

  // Notifies |this| that a "navigate to" task switch has occurred from the
  // specified |task_switch_source|. The metrics associated with
  // TaskSwitchSource::ANY source will be updated as well.
  //
  // NOTE: A |task_switch_source| value of TaskSwitchSource::ANY should not be
  // used and behavior is undefined if it is.
  //
  // A "navigate to" operation is defined by a task switch where the specific
  // task that becomes active is user-predictable (e.g., Alt+Tab accelerator,
  // launching a new window via the shelf, etc). Contrast to a "navigate away"
  // operation which is defined as a user interaction that navigates away from a
  // specified task and the next task that becomes active is likely not
  // user-predictable (e.g., closing or minimizing a window, closing a tab,
  // etc).
  //
  // Will add an entry to |histogram_map_| when called for the first time for
  // each |task_switch_source| value.
  void OnTaskSwitch(TaskSwitchSource task_switch_source);

 private:
  // Internal implementation of OnTaskSwitch(TaskSwitchSource) that will accept
  // the TaskSwitchSource::ANY value.
  void OnTaskSwitchInternal(TaskSwitchSource task_switch_source);

  // Returns the TaskSwitchTimeTracker associated with the specified
  // |task_switch_source|. May return nullptr if mapping does not exist yet.
  TaskSwitchTimeTracker* FindTaskSwitchTimeTracker(
      TaskSwitchSource task_switch_source);

  // Adds a TaskSwitchTimeTracker to |histogram_map_| for the specified
  // |task_switch_source|. Behavior is undefined if a TaskSwitchTimeTracker
  // |histogram_map_| already has an entry for |task_switch_source|.
  void AddTaskSwitchTimeTracker(TaskSwitchSource task_switch_source);

  // Tracks TaskSwitchSource to TaskSwitchTimeTracker mappings. The
  // |histogram_map_| is populated on demand the first time a
  // TaskSwitchTimeTracker is needed for a given source.
  std::unordered_map<int, std::unique_ptr<TaskSwitchTimeTracker>>
      histogram_map_;
};

}  // namespace ash

#endif  // ASH_METRICS_TASK_SWITCH_METRICS_RECORDER_H_
