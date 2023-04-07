// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_TOUCH_USAGE_METRICS_RECORDER_H_
#define ASH_METRICS_TOUCH_USAGE_METRICS_RECORDER_H_

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/events/event_handler.h"

namespace ash {

// These histograms measure touchscreen usage, both in Clamshell mode and in
// Tablet mode. Touchscreen usage in this context is defined as a set of
// consecutive touches, where each touch occurred within a set time period of
// the previous.
// TODO(b/270610982): Temporarily recording 7 time periods. Once a
// singular time period has been determined to be the best. Excess
// time periods will be removed.

constexpr char kTouchscreenUsage5SecondsHistogramName[] =
    "ChromeOS.Inputs.TouchscreenUsage.Temporary.5Seconds";
constexpr char kTouchscreenUsage15SecondsHistogramName[] =
    "ChromeOS.Inputs.TouchscreenUsage.Temporary.15Seconds";
constexpr char kTouchscreenUsage30SecondsHistogramName[] =
    "ChromeOS.Inputs.TouchscreenUsage.Temporary.30Seconds";
constexpr char kTouchscreenUsage1MinuteHistogramName[] =
    "ChromeOS.Inputs.TouchscreenUsage.Temporary.1Minute";
constexpr char kTouchscreenUsage3MinutesHistogramName[] =
    "ChromeOS.Inputs.TouchscreenUsage.Temporary.3Minutes";
constexpr char kTouchscreenUsage5MinutesHistogramName[] =
    "ChromeOS.Inputs.TouchscreenUsage.Temporary.5Minutes";
constexpr char kTouchscreenUsage10MinutesHistogramName[] =
    "ChromeOS.Inputs.TouchscreenUsage.Temporary.10Minutes";

// Monitors series of consecutive touchscreen touches and generates touchscreen
// usage histogram entries.
class ASH_EXPORT TouchscreenUsageRecorder {
 public:
  // `timer_duration`: The maximum amount of time that two consecutive touches
  // can be separated by and still be considered in the same usage session. Once
  // this time has elapsed, a recording will be made of the previous session
  // length, and any future touches will begin a new usage session.
  TouchscreenUsageRecorder(const char* const histogram_name,
                           base::TimeDelta timer_duration,
                           base::TimeDelta max_bucket_time,
                           bool tablet_mode);

  TouchscreenUsageRecorder(const TouchscreenUsageRecorder&) = delete;
  TouchscreenUsageRecorder& operator=(const TouchscreenUsageRecorder&) = delete;

  ~TouchscreenUsageRecorder();

  // Extends the current usage session if one is currently in progress, or
  // creates a new usage session if one is not in progress.
  void RecordTouch();

 private:
  // Runs as a callback after `timer_duration` has passed, since the last touch.
  void LogUsage();

  // Stores the time of the first initial touch in a usage session.
  base::TimeTicks start_time_;

  // Stores the most recent touch in a usage session.
  base::TimeTicks most_recent_touch_time_;

  // Name of the histogram that will be recorded to.
  const char* const histogram_name_;

  // The maximum usage time that can be recorded. Any usage times greater than
  // this will be placed into an overflow bucket.
  const base::TimeDelta max_bucket_time_;

  // Determines if the ".TabletMode" or ".ClamshellMode" suffix should be
  // appended to the histogram name.
  bool tablet_mode_;

  // Monitors the time remaining in a usage session, and calls a callback to
  // generate a histogram entry when the usage session has ended.
  base::RetainingOneShotTimer timer_;
};

// Holds a number of TouchscreenUsageRecorders that measure usage over a variety
// of conditions. Monitors for incoming touchscreen touches and notifies the
// corresponding TouchscreenUsageRecorders when appropriate.
class ASH_EXPORT TouchUsageMetricsRecorder : public ui::EventHandler {
 public:
  TouchUsageMetricsRecorder();

  TouchUsageMetricsRecorder(const TouchUsageMetricsRecorder&) = delete;
  TouchUsageMetricsRecorder& operator=(const TouchUsageMetricsRecorder&) =
      delete;

  ~TouchUsageMetricsRecorder() override;

  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override;

 private:
  // Notifies TouchscreenUsageRecorders that a touch has been received, if they
  // monitor the form factor that the touch was in.
  void NotifyTouchscreenUsageRecorders();

  // Contains 7 TouchscreenUsageRecorders used to monitor touch usage in
  // clamshell mode.
  // TODO(b/270610982): Remove excess TouchscreenUsageRecorders once a singular
  // timer length has been selected for Clamshell touchscreen usage.
  std::array<TouchscreenUsageRecorder, 7> clamshell_recorders_;

  // Contains 7 TouchscreenUsageRecorders used to monitor touch usage in tablet
  // mode.
  // TODO(b/270610982): Remove excess TouchscreenUsageRecorders once a singular
  // timer length has been selected for Tablet touchscreen usage.
  std::array<TouchscreenUsageRecorder, 7> tablet_recorders_;
};

// Returns the histogram name with an appended suffix, corresponding to either
// clamshell or tablet mode.
ASH_EXPORT const std::string GetHistogramNameWithMode(
    const char* const histogram_name,
    bool tablet_mode);

}  // namespace ash

#endif  // ASH_METRICS_TOUCH_USAGE_METRICS_RECORDER_H_
