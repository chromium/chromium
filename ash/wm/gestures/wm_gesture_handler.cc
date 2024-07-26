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

// For the continuous scroll animation, calculate what `OverviewEnterExitType`
// to use based on the scroll event and current overview state. Returns null if
// we should exit overview.
std::optional<OverviewEnterExitType> HandleContinuousScrollIntoOverview(
    float scroll_y,
    bool in_overview,
    bool scroll_in_progress) {
  // Note that when we call this function, we have already clamped the offset
  // so that `0` <= scroll_y <= `kVerticalThresholdDp`.
  if (!in_overview) {
    // Start the continuous scroll. Otherwise, we should enter normally with
    // type `kNormal`.
    return scroll_y < WmGestureHandler::kEnterOverviewModeThresholdDp
               ? OverviewEnterExitType::kContinuousAnimationEnterOnScrollUpdate
               : OverviewEnterExitType::kNormal;
  }

  // If `scroll_in_progress`, the last gesture event was a `EventType::kScroll`,
  // and we need to update the continuous animation.
  if (scroll_in_progress) {
    return OverviewEnterExitType::kContinuousAnimationEnterOnScrollUpdate;
  }

  // A continuous gesture has ended and we should animate into overview mode.
  if (scroll_y >= WmGestureHandler::kEnterOverviewModeThresholdDp) {
    return OverviewEnterExitType::kNormal;
  }

  // A continuous gesture has ended and we should animate out of overview mode.
  return std::nullopt;
}

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

// Similar behavior to `Handle3FingerVerticalScroll` but for continuous
// gestures.
bool Handle3FingerContinuousVerticalScroll(float scroll_y,
                                           bool scroll_in_progress) {
  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();

  // Ignore downward scrolls when not in overview mode.
  if (scroll_y < 0.f && !in_overview) {
    return false;
  }

  // Consume overscroll after continuous scroll has
  // completed progress. This prevents the scroll from propagating to another
  // view and triggering additional behavior.
  // Note that we already clamped `scroll_y` to `kVerticalThresholdDp`. If the
  // threshold has been met but the scroll is in progress, we will need to do
  // the final placement before we mark the scroll as finished.
  if (scroll_y == WmGestureHandler::kVerticalThresholdDp &&
      !overview_controller->is_continuous_scroll_in_progress()) {
    return true;
  }

  // Prevent accidental swipes from triggering the continuous animation.
  if (std::fabs(scroll_y) <
          WmGestureHandler::kContinuousGestureMoveThresholdDp &&
      !in_overview) {
    return false;
  }

  // Handle the different scroll scenarios.
  std::optional<OverviewEnterExitType> entry_type =
      HandleContinuousScrollIntoOverview(scroll_y, in_overview,
                                         scroll_in_progress);

  if (entry_type.has_value()) {
    auto* window_cycle_controller = Shell::Get()->window_cycle_controller();
    if (window_cycle_controller->IsCycling()) {
      window_cycle_controller->CancelCycling();
    }

    base::RecordAction(base::UserMetricsAction("Touchpad_Gesture_Overview"));
    overview_controller->HandleContinuousScroll(scroll_y, entry_type.value());
  } else {
    base::RecordAction(base::UserMetricsAction("Touchpad_Gesture_Overview"));

    // TODO(b/291796028): Animation should change if a new selection has been a
    // made.
    if (overview_controller->AcceptSelection()) {
      return true;
    }
    overview_controller->EndOverview(OverviewEndAction::k3FingerVerticalScroll);
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

  if (features::IsContinuousOverviewScrollAnimationEnabled() &&
      UpdateScrollForContinuousOverviewAnimation()) {
    return true;
  }

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
  const bool vertical_continuous_gesture_started =
      scroll_data_->vertical_continuous_gesture_started;
  const bool horizontal_continuous_gesture_started =
      scroll_data_->horizontal_continuous_gesture_started;
  scroll_data_.reset();

  if (finger_count == 0)
    return false;

  if (finger_count == 3) {
    // The end goal of `kContinuousOverviewScrollAnimation`, b/252521532, is to
    // completely remove the current `Handle3FingerVerticalScroll()`. So, if the
    // feature is enabled, skip this older function and no-op if
    // `vertical_continuous_gesture_started` is false.
    if (features::IsContinuousOverviewScrollAnimationEnabled()) {
      return vertical_continuous_gesture_started
                 ? Handle3FingerContinuousVerticalScroll(
                       std::clamp(scroll_y, 0.f, kVerticalThresholdDp),
                       /*scroll_in_progress=*/false)
                 : false;
    }

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

bool WmGestureHandler::UpdateScrollForContinuousOverviewAnimation() {
  if (!scroll_data_) {
    return false;
  }
  // Ignore horizontally dominant swipes if a continuous swipe has not started
  // yet.
  if (!scroll_data_->vertical_continuous_gesture_started &&
      std::fabs(scroll_data_->scroll_x) > std::fabs(scroll_data_->scroll_y)) {
    return false;
  }
  const int finger_count = scroll_data_->finger_count;

  if (finger_count != 3) {
    return false;
  }

  bool in_overview = Shell::Get()->overview_controller()->InOverviewSession();

  // If this is the first scroll update and we are already in overview mode,
  // reset the offset.
  if (in_overview && !scroll_data_->vertical_continuous_gesture_started) {
    scroll_data_->scroll_y = kVerticalThresholdDp + scroll_data_->scroll_y;
  }
  scroll_data_->vertical_continuous_gesture_started = true;
  bool scroll_started = Handle3FingerContinuousVerticalScroll(
      std::clamp(scroll_data_->scroll_y, 0.f, kVerticalThresholdDp),
      /*scroll_in_progress=*/scroll_data_->vertical_continuous_gesture_started);
  return scroll_started;
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
