// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/wm_gesture_handler.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/metrics/user_metrics.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace ash {

namespace {

// Handles vertical 3-finger scroll gesture by entering overview on scrolling
// up, and exiting it on scrolling down.
// Returns true if the gesture was handled.
bool Handle3FingerVerticalScroll(float scroll_y) {
  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  if (in_overview) {
    if (scroll_y < WmGestureHandler::kVerticalThresholdDp)
      return false;

    base::RecordAction(base::UserMetricsAction("Touchpad_Gesture_Overview"));
    if (overview_controller->AcceptSelection())
      return true;
    overview_controller->EndOverview();
  } else {
    if (scroll_y > -WmGestureHandler::kVerticalThresholdDp)
      return false;

    base::RecordAction(base::UserMetricsAction("Touchpad_Gesture_Overview"));
    overview_controller->StartOverview();
  }

  return true;
}

// Handles horizontal 4-finger scroll by switching desks if possible.
// Returns true if the gesture was handled.
bool HandleDesksSwitchHorizontalScroll(float scroll_x) {
  DCHECK(features::IsVirtualDesksEnabled());

  if (std::fabs(scroll_x) < WmGestureHandler::kHorizontalThresholdDp)
    return false;

  // This does not invert if the user changes their touchpad settings
  // currently. The scroll works Australian way (scroll left to go to the
  // desk on the right and vice versa).
  DesksController::Get()->ActivateAdjacentDesk(
      /*going_left=*/scroll_x > 0, DesksSwitchSource::kDeskSwitchTouchpad);
  return true;
}

}  // namespace

WmGestureHandler::WmGestureHandler()
    : can_handle_desks_gestures_(features::IsVirtualDesksEnabled()) {}

WmGestureHandler::~WmGestureHandler() = default;

bool WmGestureHandler::ProcessScrollEvent(const ui::ScrollEvent& event) {
  if (event.type() == ui::ET_SCROLL_FLING_CANCEL) {
    scroll_data_ = base::make_optional(ScrollData());
    return false;
  }

  if (event.type() == ui::ET_SCROLL_FLING_START) {
    bool success = EndScroll();
    DCHECK(!scroll_data_);
    return success;
  }

  if (!scroll_data_)
    return false;

  // Only three or four finger scrolls are supported.
  const int finger_count = event.finger_count();
  if (finger_count != 3 && finger_count != 4) {
    scroll_data_.reset();
    return false;
  }

  if (scroll_data_->finger_count != 0 &&
      scroll_data_->finger_count != finger_count) {
    scroll_data_.reset();
    return false;
  }

  scroll_data_->scroll_x += event.x_offset();
  scroll_data_->scroll_y += event.y_offset();
  // If the requirements to move the overview selector are met, reset
  // |scroll_data_|.
  const bool moved = MoveOverviewSelection(finger_count, scroll_data_->scroll_x,
                                           scroll_data_->scroll_y);
  if (moved)
    scroll_data_ = base::make_optional(ScrollData());
  scroll_data_->finger_count = finger_count;
  return moved;
}

bool WmGestureHandler::EndScroll() {
  if (!scroll_data_)
    return false;

  const float scroll_x = scroll_data_->scroll_x;
  const float scroll_y = scroll_data_->scroll_y;
  const int finger_count = scroll_data_->finger_count;
  scroll_data_.reset();

  if (finger_count == 0)
    return false;

  if (finger_count == 3) {
    if (std::fabs(scroll_x) < std::fabs(scroll_y))
      return Handle3FingerVerticalScroll(scroll_y);

    return MoveOverviewSelection(finger_count, scroll_x, scroll_y);
  }

  return finger_count == 4 && can_handle_desks_gestures_ &&
         HandleDesksSwitchHorizontalScroll(scroll_x);
}

bool WmGestureHandler::MoveOverviewSelection(int finger_count,
                                             float scroll_x,
                                             float scroll_y) {
  if (finger_count != 3)
    return false;

  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  // Dominantly vertical scrolls and small horizontal scrolls do not move the
  // overview selector.
  if (!in_overview || std::fabs(scroll_x) < std::fabs(scroll_y))
    return false;

  if (std::fabs(scroll_x) < kHorizontalThresholdDp)
    return false;

  overview_controller->IncrementSelection(/*forward=*/scroll_x > 0);
  return true;
}

}  // namespace ash
