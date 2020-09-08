// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/wm_gesture_handler.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_cycle_controller.h"
#include "base/metrics/user_metrics.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"

namespace ash {

namespace {

// The amount the fingers must move in a direction before a continuous gesture
// animation is started. This is to minimize accidental scrolls.
constexpr int kContinuousGestureMoveThresholdDp = 10;

// Handles vertical 3-finger scroll gesture by entering overview on scrolling
// up, and exiting it on scrolling down. If entering overview and window cycle
// list is open, close the window cycle list.
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

    auto* window_cycle_controller = Shell::Get()->window_cycle_controller();
    if (window_cycle_controller->IsCycling())
      window_cycle_controller->CancelCycling();

    base::RecordAction(base::UserMetricsAction("Touchpad_Gesture_Overview"));
    overview_controller->StartOverview();
  }

  return true;
}

// Handles horizontal 4-finger scroll by switching desks if possible.
// Returns true if the gesture was handled.
bool HandleDesksSwitchHorizontalScroll(float scroll_x) {
  if (std::fabs(scroll_x) < WmGestureHandler::kHorizontalThresholdDp)
    return false;

  // This does not invert if the user changes their touchpad settings
  // currently. The scroll works Australian way (scroll left to go to the
  // desk on the right and vice versa).
  return DesksController::Get()->ActivateAdjacentDesk(
      /*going_left=*/scroll_x > 0, DesksSwitchSource::kDeskSwitchTouchpad);
}

}  // namespace

WmGestureHandler::WmGestureHandler()
    : is_enhanced_desk_animations_(features::IsEnhancedDeskAnimations()) {}

WmGestureHandler::~WmGestureHandler() = default;

bool WmGestureHandler::ProcessScrollEvent(const ui::ScrollEvent& event) {
  // ET_SCROLL_FLING_CANCEL means a touchpad swipe has started.
  if (event.type() == ui::ET_SCROLL_FLING_CANCEL) {
    scroll_data_ = ScrollData();
    return false;
  }

  // ET_SCROLL_FLING_START means a touchpad swipe has ended.
  if (event.type() == ui::ET_SCROLL_FLING_START) {
    bool success = EndScroll();
    DCHECK(!scroll_data_);
    return success;
  }

  DCHECK_EQ(ui::ET_SCROLL, event.type());

  if (!scroll_data_)
    return false;

  // Only three or four finger scrolls are supported.
  const int finger_count = event.finger_count();
  if (finger_count != 3 && finger_count != 4) {
    scroll_data_.reset();
    return false;
  }

  // There is a finger switch, end the current gesture.
  if (scroll_data_->finger_count != 0 &&
      scroll_data_->finger_count != finger_count) {
    scroll_data_.reset();
    return false;
  }

  scroll_data_->scroll_x += event.x_offset();
  scroll_data_->scroll_y += event.y_offset();

  // If the requirements to move the overview selector or the window cycle list
  // selector are met, reset |scroll_data_|. If both are open, move the cycle
  // list's selector.
  const bool moved =
      MoveWindowCycleListSelection(finger_count, scroll_data_->scroll_x,
                                   scroll_data_->scroll_y) ||
      MoveOverviewSelection(finger_count, scroll_data_->scroll_x,
                            scroll_data_->scroll_y);

  if (is_enhanced_desk_animations_ && finger_count == 4) {
    DCHECK(!moved);
    // Update the continuous desk animation if it has already been started,
    // otherwise start it if it passes the threshold.
    if (scroll_data_->continuous_gesture_started) {
      DesksController::Get()->UpdateAnimationForGesture(event.x_offset());
    } else if (std::abs(scroll_data_->scroll_x) >
               kContinuousGestureMoveThresholdDp) {
      if (!DesksController::Get()->StartAnimationForGesture(
              /*move_left=*/event.x_offset() > 0)) {
        // Starting an animation failed. This can happen if we are on the
        // lockscreen or an ongoing animation from a different source is
        // happening. In this case reset |scroll_data_| and wait for the next 4
        // finger swipe.
        scroll_data_.reset();
        return false;
      }
      scroll_data_->continuous_gesture_started = true;
    }
  }

  if (moved)
    scroll_data_ = ScrollData();
  scroll_data_->finger_count = finger_count;
  return moved;
}

bool WmGestureHandler::EndScroll() {
  if (!scroll_data_)
    return false;

  const int finger_count = scroll_data_->finger_count;
  const float scroll_x = scroll_data_->scroll_x;
  const float scroll_y = scroll_data_->scroll_y;
  const bool continuous_gesture_started =
      scroll_data_->continuous_gesture_started;
  scroll_data_.reset();

  if (finger_count == 0)
    return false;

  if (finger_count == 3) {
    if (std::fabs(scroll_x) < std::fabs(scroll_y))
      return Handle3FingerVerticalScroll(scroll_y);

    return MoveOverviewSelection(finger_count, scroll_x, scroll_y);
  }

  if (finger_count != 4)
    return false;

  if (!is_enhanced_desk_animations_)
    return HandleDesksSwitchHorizontalScroll(scroll_x);

  if (continuous_gesture_started)
    DesksController::Get()->EndAnimationForGesture();

  return continuous_gesture_started;
}

bool WmGestureHandler::MoveOverviewSelection(int finger_count,
                                             float scroll_x,
                                             float scroll_y) {
  if (finger_count != 3)
    return false;

  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  if (!ShouldHorizontallyScrollSelector(in_overview, scroll_x, scroll_y))
    return false;

  overview_controller->IncrementSelection(/*forward=*/scroll_x > 0);
  return true;
}

bool WmGestureHandler::MoveWindowCycleListSelection(int finger_count,
                                                    float scroll_x,
                                                    float scroll_y) {
  if (!features::IsInteractiveWindowCycleListEnabled() || finger_count != 3)
    return false;

  auto* window_cycle_controller = Shell::Get()->window_cycle_controller();
  const bool is_cycling = window_cycle_controller->IsCycling();
  if (!ShouldHorizontallyScrollSelector(is_cycling, scroll_x, scroll_y))
    return false;

  window_cycle_controller->HandleCycleWindow(
      scroll_x > 0 ? WindowCycleController::FORWARD
                   : WindowCycleController::BACKWARD);
  return true;
}

bool WmGestureHandler::ShouldHorizontallyScrollSelector(bool in_session,
                                                        float scroll_x,
                                                        float scroll_y) {
  // Dominantly vertical scrolls and small horizontal scrolls do not move the
  // selector.
  if (!in_session || std::fabs(scroll_x) < std::fabs(scroll_y))
    return false;

  if (std::fabs(scroll_x) < kHorizontalThresholdDp)
    return false;

  return true;
}

}  // namespace ash
