// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/task_switch_metrics_recorder.h"

#include <memory>

#include "ash/metrics/task_switch_time_tracker.h"
#include "base/check_op.h"
#include "base/notreached.h"

namespace ash {

namespace {

const char kAshTaskSwitchHistogramName[] = "Ash.TimeBetweenTaskSwitches";

const char kDesktopHistogramName[] =
    "Ash.Desktop.TimeBetweenNavigateToTaskSwitches";

const char kShelfHistogramName[] =
    "Ash.Shelf.TimeBetweenNavigateToTaskSwitches";

const char kAcceleratorWindowCycleHistogramName[] =
    "Ash.WindowCycleController.TimeBetweenTaskSwitches";

const char kOverviewModeHistogramName[] =
    "Ash.Overview.TimeBetweenActiveWindowChanges";

// Returns the histogram name for the given |task_switch_source|.
const char* GetHistogramName(TaskSwitchSource task_switch_source) {
  switch (task_switch_source) {
    case TaskSwitchSource::ANY:
      return kAshTaskSwitchHistogramName;
    case TaskSwitchSource::DESKTOP:
      return kDesktopHistogramName;
    case TaskSwitchSource::OVERVIEW_MODE:
      return kOverviewModeHistogramName;
    case TaskSwitchSource::SHELF:
      return kShelfHistogramName;
    case TaskSwitchSource::WINDOW_CYCLE_CONTROLLER:
      return kAcceleratorWindowCycleHistogramName;
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

TaskSwitchMetricsRecorder::TaskSwitchMetricsRecorder() = default;

TaskSwitchMetricsRecorder::~TaskSwitchMetricsRecorder() = default;

void TaskSwitchMetricsRecorder::OnTaskSwitch(
    TaskSwitchSource task_switch_source) {
  DCHECK_NE(task_switch_source, TaskSwitchSource::ANY);
  if (task_switch_source != TaskSwitchSource::ANY) {
    OnTaskSwitchInternal(task_switch_source);
    OnTaskSwitchInternal(TaskSwitchSource::ANY);
  }
}

void TaskSwitchMetricsRecorder::OnTaskSwitchInternal(
    TaskSwitchSource task_switch_source) {
  TaskSwitchTimeTracker* task_switch_time_tracker =
      FindTaskSwitchTimeTracker(task_switch_source);
  if (!task_switch_time_tracker)
    AddTaskSwitchTimeTracker(task_switch_source);

  task_switch_time_tracker = FindTaskSwitchTimeTracker(task_switch_source);
  CHECK(task_switch_time_tracker);

  task_switch_time_tracker->OnTaskSwitch();
}

TaskSwitchTimeTracker* TaskSwitchMetricsRecorder::FindTaskSwitchTimeTracker(
    TaskSwitchSource task_switch_source) {
  auto it = histogram_map_.find(static_cast<int>(task_switch_source));
  if (it == histogram_map_.end())
    return nullptr;

  return it->second.get();
}

void TaskSwitchMetricsRecorder::AddTaskSwitchTimeTracker(
    TaskSwitchSource task_switch_source) {
  CHECK(histogram_map_.find(static_cast<int>(task_switch_source)) ==
        histogram_map_.end());

  const char* histogram_name = GetHistogramName(task_switch_source);
  DCHECK(histogram_name);

  histogram_map_[static_cast<int>(task_switch_source)] =
      std::make_unique<TaskSwitchTimeTracker>(histogram_name);
}

}  // namespace ash
