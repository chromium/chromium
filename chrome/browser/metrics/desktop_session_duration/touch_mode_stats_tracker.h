// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_TOUCH_MODE_STATS_TRACKER_H_
#define CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_TOUCH_MODE_STATS_TRACKER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "ui/base/pointer/touch_ui_controller.h"

// Records active time spent in touch mode. Emits one sample to
// |kSessionTouchDurationHistogram| for every sample in
// "Session.TotalDuration", which is managed by
// |metrics::DesktopSessionDurationTracker|. Each sample is the time
// spent in touch mode within the corresponding session.
class TouchModeStatsTracker
    : public metrics::DesktopSessionDurationTracker::Observer {
 public:
  TouchModeStatsTracker(
      metrics::DesktopSessionDurationTracker* session_duration_tracker,
      ui::TouchUiController* touch_ui_controller);

  ~TouchModeStatsTracker() override;

  // Creates the global instance. Any call after the first is a no-op.
  static void Initialize(
      metrics::DesktopSessionDurationTracker* session_duration_tracker,
      ui::TouchUiController* touch_ui_controller);

  static const char kSessionTouchDurationHistogramName[];

 private:
  void TouchModeChanged();

  // metrics::DesktopSessionDurationTracker::Observer:
  void OnSessionStarted(base::TimeTicks session_start) override;
  void OnSessionEnded(base::TimeDelta session_length,
                      base::TimeTicks session_end) override;

  const raw_ptr<ui::TouchUiController> touch_ui_controller_;

  base::CallbackListSubscription mode_change_subscription_;

  // The time passed by OnSessionStarted() if there is an ongoing
  // session, or 0 otherwise.
  base::TimeTicks session_start_time_;

  // The time of the last TouchModeChanged() call, or
  // session_start_time_ if there's been no switch.
  base::TimeTicks last_touch_mode_switch_in_session_;

  // The total time spent in touch mode since the last OnSessionStarted() call,
  // and before the corresponding OnSessionEnded() call. This value is logged
  // and discarded upon OnSessionEnded().
  base::TimeDelta touch_mode_duration_in_session_;
};

#endif  // CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_TOUCH_MODE_STATS_TRACKER_H_
