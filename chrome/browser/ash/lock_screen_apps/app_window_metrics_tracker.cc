// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lock_screen_apps/app_window_metrics_tracker.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "extensions/browser/app_window/app_window.h"

namespace {

// Name of the count histogram used to keep track of (1-indexed) app launch
// request ordinal number in a lock screen session.
const char kAppLaunchOrdinalNumberInLockSession[] =
    "Apps.LockScreen.NoteTakingApp.LaunchRequestOrdinalNumber";

// Name of the histogram that tracks total amount of time an app window was
// active for a single launch event - i.e. time from app window being shown to
// app window closure.
const char kTotalAppWindowSessionTime[] =
    "Apps.LockScreen.NoteTakingApp.AppWindowLifeTime.TotalActive";

// Name of the histogram that tracks amount of time an app window was in
// foreground, on top the lock screen.
const char kTimeAppWindowInForeground[] =
    "Apps.LockScreen.NoteTakingApp.AppWindowLifeTime.Foreground";

// Name of the histogram that track amount of time an app window was in
// background, behind the lock screen.
const char kTimeAppWindowInBackground[] =
    "Apps.LockScreen.NoteTakingApp.AppWindowLifeTime.Background";

// Name of the histogram that tracks the amount time needed to show an app
// window (the window render view being created) after the app launch was
// requested.
const char kTimeFromLaunchToWindowBeingShown[] =
    "Apps.LockScreen.NoteTakingApp.TimeToShowWindow";

// Name of the histogram that tracks the amount of time needed to load the an
// app window contents after the app launch was requested.
const char kTimeToLoadAppWindowContents[] =
    "Apps.LockScreen.NoteTakingApp.TimeToLoadAppWindowContents";

// The name of the histogram that tracks the amount of time the app was in
// launching state when the launch got canceled.
const char kLaunchDurationAtLaunchCancel[] =
    "Apps.LockScreen.NoteTakingApp.LaunchDurationAtLaunchCancel";

// The name of the histogram that track the app state when the app lock screen
// ends.
const char kAppWindowStateOnRemoval[] =
    "Apps.LockScreen.NoteTakingApp.FinalAppSessionState";

}  // namespace

namespace lock_screen_apps {

AppWindowMetricsTracker::AppWindowMetricsTracker(const base::TickClock* clock)
    : clock_(clock) {}

AppWindowMetricsTracker::~AppWindowMetricsTracker() = default;

void AppWindowMetricsTracker::AppLaunchRequested() {
  DCHECK_EQ(State::kInitial, state_);
  SetState(State::kLaunchRequested);

  ++app_launch_count_;
  UMA_HISTOGRAM_COUNTS_100(kAppLaunchOrdinalNumberInLockSession,
                           app_launch_count_);
}

void AppWindowMetricsTracker::MovedToForeground() {
  DCHECK_NE(State::kInitial, state_);

  if (state_ == State::kForeground)
    return;

  if (state_ == State::kBackground) {
    SetState(State::kForeground);
    return;
  }

  // Not expected to be in a state different than foreground or background
  // after |state_after_window_contents_load_| is reset.
  DCHECK(state_after_window_contents_load_.has_value());
  state_after_window_contents_load_ = State::kForeground;
}

void AppWindowMetricsTracker::MovedToBackground() {
  DCHECK_NE(State::kInitial, state_);

  if (state_ == State::kBackground)
    return;

  if (state_ == State::kForeground) {
    SetState(State::kBackground);
    return;
  }

  // Not expected to be in a state different than foreground or background
  // after |state_after_window_contents_load_| is reset.
  DCHECK(state_after_window_contents_load_.has_value());
  state_after_window_contents_load_ = State::kBackground;
}

void AppWindowMetricsTracker::AppWindowCreated(
    extensions::AppWindow* app_window) {
  Observe(app_window->web_contents());

  SetState(State::kWindowCreated);
}

void AppWindowMetricsTracker::Reset() {
  if (state_ == State::kInitial)
    return;

  UMA_HISTOGRAM_ENUMERATION(kAppWindowStateOnRemoval, state_, State::kCount);

  if (state_ != State::kLaunchRequested && state_ != State::kWindowCreated) {
    UMA_HISTOGRAM_LONG_TIMES(
        kTotalAppWindowSessionTime,
        clock_->NowTicks() - time_stamps_[State::kWindowShown]);
  } else {
    UMA_HISTOGRAM_TIMES(
        kLaunchDurationAtLaunchCancel,
        clock_->NowTicks() - time_stamps_[State::kLaunchRequested]);
  }

  SetState(State::kInitial);

  state_after_window_contents_load_ = State::kForeground;

  time_stamps_.clear();
}

void AppWindowMetricsTracker::RenderFrameCreated(
    content::RenderFrameHost* frame_host) {
  if (frame_host->GetParentOrOuterDocument())
    return;
  SetState(State::kWindowShown);

  UMA_HISTOGRAM_TIMES(
      kTimeFromLaunchToWindowBeingShown,
      clock_->NowTicks() - time_stamps_[State::kLaunchRequested]);
}

void AppWindowMetricsTracker::DocumentOnLoadCompletedInPrimaryMainFrame() {
  State next_state = state_after_window_contents_load_.value();
  state_after_window_contents_load_.reset();
  SetState(next_state);

  UMA_HISTOGRAM_TIMES(kTimeToLoadAppWindowContents,
                      clock_->NowTicks() - time_stamps_[State::kWindowShown]);
}

void AppWindowMetricsTracker::SetState(State state) {
  if (state_ == state)
    return;

  if (state_ == State::kForeground) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kTimeAppWindowInForeground,
                               clock_->NowTicks() - time_stamps_[state_]);
  } else if (state_ == State::kBackground) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kTimeAppWindowInBackground,
                               clock_->NowTicks() - time_stamps_[state_]);
  }

  state_ = state;
  time_stamps_[state] = clock_->NowTicks();
}

}  // namespace lock_screen_apps
