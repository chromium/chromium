// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/wm_gesture_handler.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_util.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "ui/aura/client/window_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Handles vertical 3-finger scroll gesture by entering overview on scrolling
// up, and exiting it on scrolling down. If entering overview and window cycle
// list is open, close the window cycle list.
// Returns true if the gesture was handled.
bool Handle3FingerVerticalScroll(float scroll_y) {
  if (std::fabs(scroll_y) < WmGestureHandler::kVerticalThresholdDp)
    return false;

  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();

  // Ignore the wrong vertical gestures (i.e., swiping down/up with three
  // fingers to enter/exit overview).
  if (in_overview ? (scroll_y > 0) : (scroll_y < 0)) {
    return false;
  }

  if (in_overview) {
    base::RecordAction(base::UserMetricsAction("Touchpad_Gesture_Overview"));
    if (overview_controller->AcceptSelection())
      return true;
    overview_controller->EndOverview(OverviewEndAction::k3FingerVerticalScroll);
  } else {
    auto* window_cycle_controller = Shell::Get()->window_cycle_controller();
    if (window_cycle_controller->IsCycling())
      window_cycle_controller->CancelCycling();

    base::RecordAction(base::UserMetricsAction("Touchpad_Gesture_Overview"));
    overview_controller->StartOverview(
        OverviewStartAction::k3FingerVerticalScroll);
  }

  return true;
}

}  // namespace

WmGestureHandler::WmGestureHandler() = default;

WmGestureHandler::~WmGestureHandler() = default;

bool WmGestureHandler::ProcessScrollEvent(const ui::ScrollEvent& event) {
  // Disable touchpad swipe when screen is pinned.
  // Also skip touchpad swipe in kiosk mode.
  if (Shell::Get()->screen_pinning_controller()->IsPinned() ||
      Shell::Get()->session_controller()->IsRunningInAppMode()) {
    return false;
  }

  // EventType::kScrollFlingCancel means a touchpad swipe has started.
  if (event.type() == ui::EventType::kScrollFlingCancel) {
    scroll_data_ = ScrollData();
    return false;
  }

  // EventType::kScrollFlingStart means a touchpad swipe has ended.
  if (event.type() == ui::EventType::kScrollFlingStart) {
    bool success = EndScroll();
    DCHECK(!scroll_data_);
    return success;
  }
  DCHECK_EQ(ui::EventType::kScroll, event.type());

  const int direction = window_util::IsNaturalScrollOn(event) ? -1 : 1;

  return ProcessEventImpl(event.finger_count(), event.x_offset() * direction,
                          event.y_offset() * direction);
}

bool WmGestureHandler::ProcessEventImpl(int finger_count,
                                        float delta_x,
                                        float delta_y) {
  if (!scroll_data_) {
    return false;
  }

  // Only three or four finger scrolls are supported.
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

  scroll_data_->scroll_x += delta_x;
  scroll_data_->scroll_y += delta_y;

  // If the requirements to move the overview selector are met, reset
  // |scroll_data_|.
  const bool moved = MoveOverviewSelection(finger_count, scroll_data_->scroll_x,
                                           scroll_data_->scroll_y);

  if (finger_count == 4) {
    DCHECK(!moved);
    // Horizontal gesture may be flipped.
    const float offset_x = -delta_x;
    const float scroll_x = scroll_data_->scroll_x;
    auto* desks_controller = DesksController::Get();
    // Update the continuous desk animation if it has already been started,
    // otherwise start it if it passes the threshold.
    if (scroll_data_->horizontal_continuous_gesture_started) {
      desks_controller->UpdateSwipeAnimation(offset_x);
    } else if (std::abs(scroll_x) > kContinuousGestureMoveThresholdDp) {
      if (!desks_controller->StartSwipeAnimation(/*move_left=*/offset_x > 0)) {
        // Starting an animation failed. This can happen if we are on the
        // lockscreen or an ongoing animation from a different source is
        // happening. In this case reset |scroll_data_| and wait for the next 4
        // finger swipe.
        scroll_data_.reset();
        return false;
      }
      scroll_data_->horizontal_continuous_gesture_started = true;
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
  const bool horizontal_continuous_gesture_started =
      scroll_data_->horizontal_continuous_gesture_started;
  scroll_data_.reset();

  if (finger_count == 0)
    return false;

  if (finger_count == 3) {
    // If the event should be captured by other normal window, do not handle
    // this event as overview handling gesture. If it is captured by non-normal
    // window (e.g. menu/popup), we can force enter overview mode.
    aura::Window* capture_window =
        ::wm::CaptureController::Get()->GetCaptureWindow();
    if (capture_window &&
        capture_window->GetType() == aura::client::WINDOW_TYPE_NORMAL) {
      return false;
    }

    if (std::fabs(scroll_x) < std::fabs(scroll_y)) {
      return Handle3FingerVerticalScroll(scroll_y);
    }

    return MoveOverviewSelection(finger_count, scroll_x, scroll_y);
  }

  if (finger_count != 4)
    return false;

  if (horizontal_continuous_gesture_started) {
    DesksController::Get()->EndSwipeAnimation();
  }

  return horizontal_continuous_gesture_started;
}

bool WmGestureHandler::MoveOverviewSelection(int finger_count,
                                             float scroll_x,
                                             float scroll_y) {
  if (finger_count != 3)
    return false;

  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  if (!ShouldHorizontallyScroll(in_overview, scroll_x, scroll_y))
    return false;

  overview_controller->IncrementSelection(/*forward=*/scroll_x > 0);
  return true;
}

bool WmGestureHandler::ShouldHorizontallyScroll(bool in_session,
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
