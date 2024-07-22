// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/touch_usage_metrics_recorder.h"

#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"

namespace ash {

TouchscreenUsageRecorder::TouchscreenUsageRecorder(
    const char* const histogram_name,
    base::TimeDelta timer_duration,
    base::TimeDelta max_bucket_time,
    bool tablet_mode)
    : histogram_name_(histogram_name),
      max_bucket_time_(max_bucket_time),
      tablet_mode_(tablet_mode),
      timer_(FROM_HERE,
             timer_duration,
             base::BindRepeating(&TouchscreenUsageRecorder::LogUsage,
                                 base::Unretained(this))) {}

TouchscreenUsageRecorder::~TouchscreenUsageRecorder() = default;

void TouchscreenUsageRecorder::RecordTouch() {
  most_recent_touch_time_ = base::TimeTicks::Now();
  if (!timer_.IsRunning()) {
    start_time_ = most_recent_touch_time_;
  }
  timer_.Reset();
}

void TouchscreenUsageRecorder::LogUsage() {
  // Usage is only logged if two consecutive touches are detected.
  if (start_time_ != most_recent_touch_time_) {
    base::TimeDelta diff = most_recent_touch_time_ - start_time_;
    base::UmaHistogramCustomTimes(
        GetHistogramNameWithMode(histogram_name_, tablet_mode_), diff,
        base::Seconds(1), max_bucket_time_, 100);
  }
}

// TODO(b/270610982): Temporarily recording values for multiple timer periods
// in order to assess which ones gives the best usage result. Will remove
// other timers when a decision has been reached.
TouchUsageMetricsRecorder::TouchUsageMetricsRecorder()
    : clamshell_recorders_{TouchscreenUsageRecorder(
                               kTouchscreenUsage5SecondsHistogramName,
                               base::Seconds(5),
                               base::Minutes(20),
                               false),
                           TouchscreenUsageRecorder(
                               kTouchscreenUsage15SecondsHistogramName,
                               base::Seconds(15),
                               base::Minutes(40),
                               false),
                           TouchscreenUsageRecorder(
                               kTouchscreenUsage30SecondsHistogramName,
                               base::Seconds(30),
                               base::Hours(1),
                               false),
                           TouchscreenUsageRecorder(
                               kTouchscreenUsage1MinuteHistogramName,
                               base::Minutes(1),
                               base::Hours(1.5),
                               false),
                           TouchscreenUsageRecorder(
                               kTouchscreenUsage3MinutesHistogramName,
                               base::Minutes(3),
                               base::Hours(2),
                               false),
                           TouchscreenUsageRecorder(
                               kTouchscreenUsage5MinutesHistogramName,
                               base::Minutes(5),
                               base::Hours(3),
                               false),
                           TouchscreenUsageRecorder(
                               kTouchscreenUsage10MinutesHistogramName,
                               base::Minutes(10),
                               base::Hours(6),
                               false)},
      tablet_recorders_{
          TouchscreenUsageRecorder(kTouchscreenUsage5SecondsHistogramName,
                                   base::Seconds(5),
                                   base::Minutes(20),
                                   true),
          TouchscreenUsageRecorder(kTouchscreenUsage15SecondsHistogramName,
                                   base::Seconds(15),
                                   base::Minutes(40),
                                   true),
          TouchscreenUsageRecorder(kTouchscreenUsage30SecondsHistogramName,
                                   base::Seconds(30),
                                   base::Hours(1),
                                   true),
          TouchscreenUsageRecorder(kTouchscreenUsage1MinuteHistogramName,
                                   base::Minutes(1),
                                   base::Hours(1.5),
                                   true),
          TouchscreenUsageRecorder(kTouchscreenUsage3MinutesHistogramName,
                                   base::Minutes(3),
                                   base::Hours(2),
                                   true),
          TouchscreenUsageRecorder(kTouchscreenUsage5MinutesHistogramName,
                                   base::Minutes(5),
                                   base::Hours(3),
                                   true),
          TouchscreenUsageRecorder(kTouchscreenUsage10MinutesHistogramName,
                                   base::Minutes(10),
                                   base::Hours(6),
                                   true)}

{
  Shell::Get()->AddPreTargetHandler(this);
}

TouchUsageMetricsRecorder::~TouchUsageMetricsRecorder() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void TouchUsageMetricsRecorder::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::EventType::kTouchPressed) {
    NotifyTouchscreenUsageRecorders();
  }
}

void TouchUsageMetricsRecorder::NotifyTouchscreenUsageRecorders() {
  std::array<TouchscreenUsageRecorder, 7>& recorders =
      display::Screen::GetScreen()->InTabletMode() ? tablet_recorders_
                                                   : clamshell_recorders_;

  for (TouchscreenUsageRecorder& recorder : recorders) {
    recorder.RecordTouch();
  }
}

const std::string GetHistogramNameWithMode(const char* const histogram_name,
                                           bool tablet_mode) {
  return base::StrCat(
      {histogram_name, (tablet_mode ? ".TabletMode" : ".ClamshellMode")});
}

}  // namespace ash
