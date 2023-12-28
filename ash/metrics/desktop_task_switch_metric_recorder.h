// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_DESKTOP_TASK_SWITCH_METRIC_RECORDER_H_
#define ASH_METRICS_DESKTOP_TASK_SWITCH_METRIC_RECORDER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

// Tracks metrics for task switches caused by the user activating a task window
// by clicking or tapping on it.
class ASH_EXPORT DesktopTaskSwitchMetricRecorder
    : public ::wm::ActivationChangeObserver {
 public:
  DesktopTaskSwitchMetricRecorder();

  DesktopTaskSwitchMetricRecorder(const DesktopTaskSwitchMetricRecorder&) =
      delete;
  DesktopTaskSwitchMetricRecorder& operator=(
      const DesktopTaskSwitchMetricRecorder&) = delete;

  ~DesktopTaskSwitchMetricRecorder() override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(
      ::wm::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

 private:
  // Tracks the last active task window.
  raw_ptr<aura::Window, DanglingUntriaged> last_active_task_window_;
};

}  // namespace ash

#endif  // ASH_METRICS_DESKTOP_TASK_SWITCH_METRIC_RECORDER_H_
