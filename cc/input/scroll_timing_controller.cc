// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_timing_controller.h"

#include <utility>

namespace cc {

ScrollTimingController::ScrollTimingController() = default;
ScrollTimingController::~ScrollTimingController() = default;

void ScrollTimingController::DidScrollBegin(ui::ScrollInputType input_type,
                                            base::TimeTicks event_timestamp) {
  if (input_type != ui::ScrollInputType::kTouchscreen &&
      input_type != ui::ScrollInputType::kWheel) {
    return;
  }
  // Without a hardware timestamp, startTime/duration would not be comparable
  // with the rest of the dataset, so skip tracking this gesture.
  if (event_timestamp.is_null()) {
    return;
  }
  // `element_id` is captured later in `DidScrollUpdate`; `end_time` stays
  // nullopt until the gesture finishes. A second DidScrollBegin without a
  // matching DidScrollEnd replaces any in-flight gesture.
  active_info_ =
      ScrollTimingInfo{.start_time = event_timestamp, .input_type = input_type};
}

void ScrollTimingController::DidScrollUpdate(ElementId element_id) {
  // Only meaningful while a gesture is being tracked; first effective
  // update of the gesture captures the latched scroller, later updates
  // leave it alone.
  if (!active_info_.has_value() || active_info_->element_id) {
    return;
  }
  active_info_->element_id = element_id;
}

void ScrollTimingController::DidScrollEnd(ui::ScrollInputType input_type) {
  if (!active_info_.has_value()) {
    return;
  }
  // Defensive: a stale/wrong-device GestureScrollEnd may reach here before
  // the InputHandlerProxy validates it against the active gesture. Drop the
  // in-flight record too - the begin/end pair is inconsistent, so the
  // tracked data is suspect and shouldn't be emitted.
  if (input_type != active_info_->input_type) {
    active_info_.reset();
    return;
  }
  // A non-empty ElementId means at least one scroll update moved a scroller;
  // only those gestures produce a PerformanceScrollTiming entry.
  if (active_info_->element_id) {
    active_info_->end_time = base::TimeTicks::Now();
    completed_infos_.push_back(*active_info_);
  }
  active_info_.reset();
}

std::vector<ScrollTimingInfo>
ScrollTimingController::TakeCompletedScrollTimingInfos() {
  return std::exchange(completed_infos_, {});
}

std::optional<base::TimeTicks>
ScrollTimingController::ActiveScrollStartForTesting() const {
  return active_info_.has_value()
             ? std::optional<base::TimeTicks>(active_info_->start_time)
             : std::nullopt;
}

}  // namespace cc
