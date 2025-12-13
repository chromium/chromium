// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/touch_ui_controller_stats_tracker.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "ui/base/pointer/touch_ui_controller.h"

// static
void TouchUIControllerStatsTracker::Initialize(
    metrics::DesktopSessionDurationTracker* session_duration_tracker,
    ui::TouchUiController* touch_ui_controller) {
  static base::NoDestructor<TouchUIControllerStatsTracker> stats_tracker(
      session_duration_tracker, touch_ui_controller);
}

TouchUIControllerStatsTracker::TouchUIControllerStatsTracker(
    metrics::DesktopSessionDurationTracker* session_duration_tracker,
    ui::TouchUiController* touch_ui_controller)
    : touch_ui_controller_(touch_ui_controller) {
  session_duration_tracker->AddObserver(this);

  // If this instance is destroyed, |touch_mode_change_subscription_|'s
  // destructor will unregister the callback. Hence Unretained is safe.
  touch_mode_change_subscription_ = touch_ui_controller->RegisterCallback(
      base::BindRepeating(&TouchUIControllerStatsTracker::TouchModeChanged,
                          base::Unretained(this)));
#if BUILDFLAG(IS_WIN)
  tablet_mode_change_subscription_ =
      touch_ui_controller->RegisterTabletModeCallback(
          base::BindRepeating(&TouchUIControllerStatsTracker::TabletModeChanged,
                              base::Unretained(this)));
#endif  // BUILDFLAG(IS_WIN)
}

TouchUIControllerStatsTracker::~TouchUIControllerStatsTracker() = default;

// static
const char TouchUIControllerStatsTracker::kSessionTouchDurationHistogramName[] =
    "Session.TotalDuration.TouchMode";
#if BUILDFLAG(IS_WIN)
const char
    TouchUIControllerStatsTracker::kSessionTabletDurationHistogramName[] =
        "Session.TotalDuration.TabletMode";
#endif  // BUILDFLAG(IS_WIN)

void TouchUIControllerStatsTracker::TouchModeChanged() {
  if (session_start_time_.is_null()) {
    return;
  }

  auto switch_time = base::TimeTicks::Now();
  DCHECK_GE(switch_time, last_touch_mode_switch_in_session_);

  // If we changed to non-touch mode, we were in touch mode in the span
  // of time from last_touch_mode_switch_in_session_ to switch_time.
  if (!touch_ui_controller_->touch_ui()) {
    touch_mode_duration_in_session_ +=
        switch_time - last_touch_mode_switch_in_session_;
  }

  last_touch_mode_switch_in_session_ = switch_time;
}

#if BUILDFLAG(IS_WIN)
void TouchUIControllerStatsTracker::TabletModeChanged() {
  if (session_start_time_.is_null()) {
    return;
  }

  auto switch_time = base::TimeTicks::Now();
  DCHECK_GE(switch_time, last_tablet_mode_switch_in_session_);

  // If we changed to desktop mode, we were in tablet mode in the span
  // of time from last_tablet_mode_switch_in_session_ to switch_time.
  if (!touch_ui_controller_->tablet_mode()) {
    tablet_mode_duration_in_session_ +=
        switch_time - last_tablet_mode_switch_in_session_;
  }

  last_tablet_mode_switch_in_session_ = switch_time;
}
#endif  // BUILDFLAG(IS_WIN)

void TouchUIControllerStatsTracker::OnSessionStarted(
    base::TimeTicks session_start) {
  session_start_time_ = session_start;
  last_touch_mode_switch_in_session_ = session_start_time_;
  touch_mode_duration_in_session_ = base::TimeDelta();
#if BUILDFLAG(IS_WIN)
  last_tablet_mode_switch_in_session_ = session_start_time_;
  tablet_mode_duration_in_session_ = base::TimeDelta();
#endif  // BUILDFLAG(IS_WIN)
}

void TouchUIControllerStatsTracker::OnSessionEnded(
    base::TimeDelta session_length,
    base::TimeTicks session_end) {
  // If we end in touch mode, we must count the time from
  // last_touch_mode_switch_in_session_ to session_end.
  //
  // |session_end| may be slightly less than
  // |last_touch_mode_switch_in_session_| because an OnSessionEnded()
  // call may happen slightly after the session end time. Assuming the
  // difference is small, the touch mode time left unaccounted for is small.
  // Accept this error and ignore this time. See crbug.com/1165462.
  if (touch_ui_controller_->touch_ui() &&
      session_end >= last_touch_mode_switch_in_session_) {
    touch_mode_duration_in_session_ +=
        session_end - last_touch_mode_switch_in_session_;
  }
#if BUILDFLAG(IS_WIN)
  if (touch_ui_controller_->tablet_mode() &&
      session_end >= last_tablet_mode_switch_in_session_) {
    tablet_mode_duration_in_session_ +=
        session_end - last_tablet_mode_switch_in_session_;
  }
#endif  // BUILDFLAG(IS_WIN)
  // The samples here correspond 1:1 with Session.TotalDuration, so the
  // bucketing matches too.
  base::UmaHistogramLongTimes(kSessionTouchDurationHistogramName,
                              touch_mode_duration_in_session_);

  session_start_time_ = base::TimeTicks();
  last_touch_mode_switch_in_session_ = base::TimeTicks();
  touch_mode_duration_in_session_ = base::TimeDelta();
#if BUILDFLAG(IS_WIN)
  base::UmaHistogramLongTimes(kSessionTabletDurationHistogramName,
                              tablet_mode_duration_in_session_);
  last_tablet_mode_switch_in_session_ = base::TimeTicks();
  tablet_mode_duration_in_session_ = base::TimeDelta();
#endif  // BUILDFLAG(IS_WIN)
}
