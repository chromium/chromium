// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/desktop_task_switch_metric_recorder.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/metrics/user_metrics.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

DesktopTaskSwitchMetricRecorder::DesktopTaskSwitchMetricRecorder()
    : last_active_task_window_(nullptr) {
  Shell::Get()->activation_client()->AddObserver(this);
}

DesktopTaskSwitchMetricRecorder::~DesktopTaskSwitchMetricRecorder() {
  Shell::Get()->activation_client()->RemoveObserver(this);
}

void DesktopTaskSwitchMetricRecorder::OnWindowActivated(
    ::wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (gained_active && window_util::IsWindowUserPositionable(gained_active)) {
    if (last_active_task_window_ != gained_active &&
        reason ==
            ::wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT) {
      base::RecordAction(base::UserMetricsAction("Desktop_SwitchTask"));
      Shell::Get()->metrics()->task_switch_metrics_recorder().OnTaskSwitch(
          TaskSwitchSource::DESKTOP);
    }
    last_active_task_window_ = gained_active;
  }
}

}  // namespace ash
