// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_button_pressed_metric_tracker.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/views/controls/button/button.h"

namespace ash {

ShelfButtonPressedMetricTracker::ShelfButtonPressedMetricTracker() = default;

ShelfButtonPressedMetricTracker::~ShelfButtonPressedMetricTracker() = default;

void ShelfButtonPressedMetricTracker::ButtonPressed(
    const ui::Event& event,
    const views::Button* sender,
    ShelfAction performed_action) {
  RecordButtonPressedSource(event);
  RecordButtonPressedAction(performed_action);
}

void ShelfButtonPressedMetricTracker::RecordButtonPressedSource(
    const ui::Event& event) {
  // NOTE: These metrics are called "launcher" instead of "shelf" because the
  // original name of the shelf UI was "launcher".
  if (event.IsMouseEvent()) {
    base::RecordAction(base::UserMetricsAction("Launcher_ButtonPressed_Mouse"));
  } else if (event.IsGestureEvent()) {
    base::RecordAction(base::UserMetricsAction("Launcher_ButtonPressed_Touch"));
  }
}

void ShelfButtonPressedMetricTracker::RecordButtonPressedAction(
    ShelfAction performed_action) {
  switch (performed_action) {
    case SHELF_ACTION_NONE:
    case SHELF_ACTION_APP_LIST_SHOWN:
    case SHELF_ACTION_APP_LIST_DISMISSED:
    case SHELF_ACTION_APP_LIST_BACK:
      break;
    case SHELF_ACTION_NEW_WINDOW_CREATED:
      base::RecordAction(base::UserMetricsAction("Launcher_LaunchTask"));
      Shell::Get()->metrics()->task_switch_metrics_recorder().OnTaskSwitch(
          TaskSwitchSource::SHELF);
      break;
    case SHELF_ACTION_WINDOW_ACTIVATED:
      base::RecordAction(base::UserMetricsAction("Launcher_SwitchTask"));
      Shell::Get()->metrics()->task_switch_metrics_recorder().OnTaskSwitch(
          TaskSwitchSource::SHELF);
      break;
    case SHELF_ACTION_WINDOW_MINIMIZED:
      base::RecordAction(base::UserMetricsAction("Launcher_MinimizeTask"));
      break;
  }
}

}  // namespace ash
