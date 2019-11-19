// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/touch_accessibility_enabler.h"

#include <math.h>

#include <utility>

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/time/default_tick_clock.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"

namespace ash {

namespace {

// Delay between timer callbacks. Each one plays a tick sound.
constexpr int kTimerDelayInMS = 500;

// The number of ticks of the timer before the first sound is generated.
constexpr int kTimerTicksOfFirstSoundFeedback = 6;

// The number of ticks of the timer before toggling spoken feedback.
constexpr int kTimerTicksToToggleSpokenFeedback = 10;

}  // namespace

TouchAccessibilityEnabler::TouchAccessibilityEnabler(
    aura::Window* root_window,
    TouchAccessibilityEnablerDelegate* delegate)
    : root_window_(root_window),
      delegate_(delegate),
      state_(NO_FINGERS_DOWN),
      tick_clock_(nullptr) {
  DCHECK(root_window);
  DCHECK(delegate);
  AddEventHandler();
}

TouchAccessibilityEnabler::~TouchAccessibilityEnabler() {
  RemoveEventHandler();
}

void TouchAccessibilityEnabler::RemoveEventHandler() {
  if (event_handler_installed_) {
    root_window_->RemovePreTargetHandler(this);
    event_handler_installed_ = false;
    ResetToNoFingersDown();
  }
}

void TouchAccessibilityEnabler::AddEventHandler() {
  if (!event_handler_installed_) {
    root_window_->AddPreTargetHandler(this);
    event_handler_installed_ = true;
    ResetToNoFingersDown();
  }
}

void TouchAccessibilityEnabler::OnTouchEvent(ui::TouchEvent* event) {
  DCHECK(!(event->flags() & ui::EF_TOUCH_ACCESSIBILITY));
  HandleTouchEvent(*event);
}

void TouchAccessibilityEnabler::HandleTouchEvent(const ui::TouchEvent& event) {
  DCHECK(!(event.flags() & ui::EF_TOUCH_ACCESSIBILITY));
  const ui::EventType type = event.type();
  const gfx::PointF& location = event.location_f();
  const int touch_id = event.pointer_details().id;

  if (type == ui::ET_TOUCH_PRESSED) {
    touch_locations_.insert(std::pair<int, gfx::PointF>(touch_id, location));
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    auto iter = touch_locations_.find(touch_id);

    // Can happen if this object is constructed while fingers were down.
    if (iter == touch_locations_.end())
      return;

    touch_locations_.erase(touch_id);
  } else if (type == ui::ET_TOUCH_MOVED) {
    auto iter = touch_locations_.find(touch_id);

    // Can happen if this object is constructed while fingers were down.
    if (iter == touch_locations_.end())
      return;

    float delta = (location - iter->second).Length();
    if (delta > gesture_detector_config_.double_tap_slop) {
      state_ = WAIT_FOR_NO_FINGERS;
      CancelTimer();
      return;
    }
  } else {
    NOTREACHED() << "Unexpected event type received: " << event.GetName();
    return;
  }

  if (touch_locations_.size() == 0) {
    state_ = NO_FINGERS_DOWN;
    CancelTimer();
    return;
  }

  if (touch_locations_.size() > 2) {
    state_ = WAIT_FOR_NO_FINGERS;
    CancelTimer();
    return;
  }

  if (state_ == NO_FINGERS_DOWN && event.type() == ui::ET_TOUCH_PRESSED) {
    state_ = ONE_FINGER_DOWN;
  } else if (state_ == ONE_FINGER_DOWN &&
             event.type() == ui::ET_TOUCH_PRESSED) {
    state_ = TWO_FINGERS_DOWN;
    two_finger_start_time_ = Now();
    StartTimer();
    delegate_->OnTwoFingerTouchStart();
  }
}

base::WeakPtr<TouchAccessibilityEnabler>
TouchAccessibilityEnabler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::TimeTicks TouchAccessibilityEnabler::Now() {
  if (tick_clock_) {
    // This is the same as what EventTimeForNow() does, but here we do it
    // with a clock that can be replaced with a simulated clock for tests.
    return tick_clock_->NowTicks();
  }
  return ui::EventTimeForNow();
}

void TouchAccessibilityEnabler::StartTimer() {
  if (timer_.IsRunning())
    return;

  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kTimerDelayInMS),
               this, &TouchAccessibilityEnabler::OnTimer);
}

void TouchAccessibilityEnabler::CancelTimer() {
  if (timer_.IsRunning()) {
    timer_.Stop();
    delegate_->OnTwoFingerTouchStop();
  }
}

void TouchAccessibilityEnabler::OnTimer() {
  base::TimeTicks now = Now();
  double tick_count_f =
      (now - two_finger_start_time_).InMillisecondsF() / kTimerDelayInMS;
  int tick_count = roundf(tick_count_f);

  if (tick_count == kTimerTicksOfFirstSoundFeedback) {
    base::RecordAction(
        base::UserMetricsAction("Accessibility.TwoFingersHeldDown"));
  }

  if (tick_count >= kTimerTicksOfFirstSoundFeedback &&
      tick_count < kTimerTicksToToggleSpokenFeedback) {
    delegate_->PlaySpokenFeedbackToggleCountdown(tick_count);
  }
  if (tick_count == kTimerTicksToToggleSpokenFeedback) {
    delegate_->ToggleSpokenFeedback();
    state_ = WAIT_FOR_NO_FINGERS;
  }
}

void TouchAccessibilityEnabler::ResetToNoFingersDown() {
  state_ = NO_FINGERS_DOWN;
  touch_locations_.clear();
  CancelTimer();
}

}  // namespace ash
