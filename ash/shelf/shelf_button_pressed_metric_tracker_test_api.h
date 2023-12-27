// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_BUTTON_PRESSED_METRIC_TRACKER_TEST_API_H_
#define ASH_SHELF_SHELF_BUTTON_PRESSED_METRIC_TRACKER_TEST_API_H_

#include "ash/shelf/shelf_button_pressed_metric_tracker.h"
#include "base/memory/raw_ptr.h"

#include "ui/events/event.h"

namespace base {
class TickClock;
}  // namespace base

namespace ash {

class ShelfButtonPressedMetricTrackerTestAPI {
 public:
  explicit ShelfButtonPressedMetricTrackerTestAPI(
      ShelfButtonPressedMetricTracker* shelf_button_pressed_metric_tracker);

  ShelfButtonPressedMetricTrackerTestAPI(
      const ShelfButtonPressedMetricTrackerTestAPI&) = delete;
  ShelfButtonPressedMetricTrackerTestAPI& operator=(
      const ShelfButtonPressedMetricTrackerTestAPI&) = delete;

  ~ShelfButtonPressedMetricTrackerTestAPI();

  // Set's the |tick_clock_| on the internal ShelfButtonPressedMetricTracker.
  // This doesn't take the ownership of the clock. |tick_clock| must outlive the
  // ShelfButtonPressedMetricTracker instance.
  void SetTickClock(const base::TickClock* tick_clock);

 private:
  const raw_ptr<ShelfButtonPressedMetricTracker>
      shelf_button_pressed_metric_tracker_;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_BUTTON_PRESSED_METRIC_TRACKER_TEST_API_H_
