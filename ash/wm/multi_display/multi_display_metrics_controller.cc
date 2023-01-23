// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/multi_display/multi_display_metrics_controller.h"

#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/metrics/histogram_functions.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

constexpr base::TimeDelta kTimerDuration = base::Minutes(1);

}  // namespace

MultiDisplayMetricsController::MultiDisplayMetricsController() = default;

MultiDisplayMetricsController::~MultiDisplayMetricsController() = default;

void MultiDisplayMetricsController::OnWindowMovedOrResized(
    aura::Window* window) {
  if (!timer_.IsRunning()) {
    return;
  }

  if (!windows_.Contains(window)) {
    return;
  }

  timer_.Stop();
  RecordHistogram(/*user_moved_window=*/true);
}

void MultiDisplayMetricsController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  const bool rotation_changed = changed_metrics & DISPLAY_METRIC_ROTATION;
  const bool work_area_changed = changed_metrics & DISPLAY_METRIC_WORK_AREA;
  if (!rotation_changed && !work_area_changed) {
    return;
  }

  timer_.Stop();
  windows_.RemoveAll();

  auto windows =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kAllDesks);
  if (windows.empty()) {
    return;
  }

  // Store the MRU windows that existed at the time of this display change. This
  // is so if a user creates a new window after the display change, and then
  // moves or resizes it, it would not count.
  for (aura::Window* window : windows) {
    windows_.Add(window);
  }
  last_display_change_type_ = rotation_changed ? DisplayChangeType::kRotated
                                               : DisplayChangeType::kWorkArea;
  timer_.Start(FROM_HERE, kTimerDuration, this,
               &MultiDisplayMetricsController::OnTimerFinished);
}

void MultiDisplayMetricsController::OnTimerFinished() {
  RecordHistogram(/*user_moved_window=*/false);
}

void MultiDisplayMetricsController::RecordHistogram(bool user_moved_window) {
  windows_.RemoveAll();
  std::string histogram_name;
  switch (last_display_change_type_) {
    case DisplayChangeType::kRotated:
      histogram_name = kRotatedHistogram;
      break;
    case DisplayChangeType::kWorkArea:
      histogram_name = kWorkAreaChangedHistogram;
      break;
  }
  base::UmaHistogramBoolean(histogram_name, user_moved_window);
}

}  // namespace ash
