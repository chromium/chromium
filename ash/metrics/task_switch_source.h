// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_TASK_SWITCH_SOURCE_H_
#define ASH_METRICS_TASK_SWITCH_SOURCE_H_

namespace ash {

// Enumeration of the different user interfaces that could be the source of
// a task switch. Note this is not necessarily comprehensive of all sources.
enum class TaskSwitchSource {
  // Task switches caused by any two sources in this enum. NOTE: This value
  // should NOT be used outside of TaskSwitchMetricsRecorder.
  ANY,
  // Task switches caused by the user activating a task window by clicking or
  // tapping on it, or moving the mouse over the window while focus follows
  // cursor is enabled.
  DESKTOP,
  // Task switches caused by selecting a window from overview mode which is
  // different from the previously-active window.
  OVERVIEW_MODE,
  // All task switches caused by shelf buttons, not including sub-menus.
  SHELF,
  // Task switches caused by the WindowCycleController (ie Alt+Tab).
  WINDOW_CYCLE_CONTROLLER
};

}  // namespace ash

#endif  // ASH_METRICS_TASK_SWITCH_SOURCE_H_
