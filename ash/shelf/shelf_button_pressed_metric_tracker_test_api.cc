// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_button_pressed_metric_tracker_test_api.h"

#include "base/time/tick_clock.h"

namespace ash {

ShelfButtonPressedMetricTrackerTestAPI::ShelfButtonPressedMetricTrackerTestAPI(
    ShelfButtonPressedMetricTracker* shelf_button_pressed_metric_tracker)
    : shelf_button_pressed_metric_tracker_(
          shelf_button_pressed_metric_tracker) {}

ShelfButtonPressedMetricTrackerTestAPI::
    ~ShelfButtonPressedMetricTrackerTestAPI() = default;

void ShelfButtonPressedMetricTrackerTestAPI::SetTickClock(
    const base::TickClock* tick_clock) {
  shelf_button_pressed_metric_tracker_->tick_clock_ = tick_clock;
}

}  // namespace ash
