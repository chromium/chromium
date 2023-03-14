// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/ui_metrics_recorder.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"

#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

using EventType = cc::EventMetrics::EventType;

namespace ash {

namespace {

bool IsCoreMeric(EventType event_type) {
  return event_type == EventType::kMouseDragged ||
         event_type == EventType::kMousePressed ||
         event_type == EventType::kMouseReleased ||
         event_type == EventType::kKeyPressed ||
         event_type == EventType::kKeyReleased;
}

}  // namespace

UiMetricsRecorder::UiMetricsRecorder() = default;
UiMetricsRecorder::~UiMetricsRecorder() = default;

void UiMetricsRecorder::OnUserLoggedIn() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // OnUserLoggedIn could be called multiple times from any states.
  // e.g.
  //   from kBeforeLogin: sign-in from the login screen and on cryptohome mount
  //   from kDuringLogin: during user profile loading after checking ownership
  //   from kInSession: adding a new user to the existing session.
  // Only set kDuringLogin on first OnUserLoggedIn call from kBeforeLogin so
  // that kDuringLogin starts from cryptohome mount.
  if (state_ == State::kBeforeLogin) {
    state_ = State::kDuringLogin;
    check_session_init_ = true;
    user_logged_in_time_ = base::TimeTicks::Now();
  }
}

void UiMetricsRecorder::OnPostLoginAnimationFinish() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This happens when adding a user to the existing session. Ignore it to
  // treat secondary user login as in session since multiple profile feature is
  // deprecating.
  if (state_ == State::kInSession)
    return;

  DCHECK_EQ(State::kDuringLogin, state_);
  state_ = State::kInSession;
  user_session_start_time_ = base::TimeTicks::Now();
}

void UiMetricsRecorder::ReportPercentDroppedFramesInOneSecondWindow(
    double percent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UMA_HISTOGRAM_PERCENTAGE("Ash.Smoothness.PercentDroppedFrames_1sWindow",
                           percent);

  switch (state_) {
    case State::kBeforeLogin:
      UMA_HISTOGRAM_PERCENTAGE(
          "Ash.Smoothness.PercentDroppedFrames_1sWindow.BeforeLogin", percent);
      break;
    case State::kDuringLogin:
      UMA_HISTOGRAM_PERCENTAGE(
          "Ash.Smoothness.PercentDroppedFrames_1sWindow.DuringLogin", percent);
      break;
    case State::kInSession:
      UMA_HISTOGRAM_PERCENTAGE(
          "Ash.Smoothness.PercentDroppedFrames_1sWindow.InSession", percent);
      break;
  }

  // When `check_session_init_` is set, probing for 5 seconds of good ADF
  // numbers (i.e. ADF <= 20%). When the first such duration is detected,
  // take is as the signal that the user session is full initialized and set
  // `session_initialized_` flag.
  if (check_session_init_) {
    // Threshold for `percent` to be considered good.
    constexpr double kGoodAdf = 20;
    if (!last_good_dropped_frame_time_.has_value() && percent <= kGoodAdf) {
      last_good_dropped_frame_time_ = base::TimeTicks::Now();
    } else if (last_good_dropped_frame_time_.has_value() &&
               percent > kGoodAdf) {
      last_good_dropped_frame_time_.reset();
    }

    // Minimum duration for `percent` to stay in good before the user session
    // is considered as fully initialized.
    constexpr base::TimeDelta kMinGoodAdfDuration = base::Seconds(5);
    if (last_good_dropped_frame_time_.has_value()) {
      const base::TimeTicks now = base::TimeTicks::Now();
      if (now - last_good_dropped_frame_time_.value() >= kMinGoodAdfDuration) {
        base::UmaHistogramCustomMicrosecondsTimes(
            "Ash.Login.TimeUntilGoodADF", now - user_logged_in_time_.value(),
            base::Milliseconds(1), base::Minutes(10), 100);

        check_session_init_ = false;
        session_initialized_ = true;
      }
    }
  }

  if (session_initialized_) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Ash.Smoothness.PercentDroppedFrames_1sWindow.InSession2", percent);
  }
}

void UiMetricsRecorder::ReportPercentDroppedFramesInOneSecondWindow2(
    double percent) {
  UMA_HISTOGRAM_PERCENTAGE("Ash.Smoothness.PercentDroppedFrames_1sWindow2",
                           percent);

  // Time to exclude from user session to be reported under "InSession" metric.
  constexpr base::TimeDelta chopped_user_session_time = base::Minutes(1);
  if (user_session_start_time_ &&
      base::TimeTicks::Now() - user_session_start_time_.value() >=
          chopped_user_session_time) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Ash.Smoothness.PercentDroppedFrames_1sWindow2.InSession", percent);
  }
}

void UiMetricsRecorder::ReportEventLatency(
    std::vector<cc::EventLatencyTracker::LatencyData> latencies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  constexpr base::TimeDelta kLongLatency = base::Milliseconds(500);
  for (auto& latency : latencies) {
    const char* event_type = cc::EventMetrics::GetTypeName(latency.event_type);
    base::UmaHistogramCustomMicrosecondsTimes(
        base::StrCat({"Ash.EventLatency.", event_type, ".TotalLatency"}),
        latency.total_latency, base::Milliseconds(1), base::Seconds(5), 100);
    UMA_HISTOGRAM_CUSTOM_TIMES("Ash.EventLatency.TotalLatency",
                               latency.total_latency, base::Milliseconds(1),
                               base::Seconds(5), 100);

    if (IsCoreMeric(latency.event_type)) {
      UMA_HISTOGRAM_CUSTOM_TIMES("Ash.EventLatency.Core.TotalLatency",
                                 latency.total_latency, base::Milliseconds(1),
                                 base::Seconds(5), 100);
    }

    if (latency.event_type != EventType::kGestureLongPress &&
        latency.event_type != EventType::kGestureLongTap &&
        latency.total_latency > kLongLatency) {
      VLOG(1) << "Ash event latency is longer than usual"
              << ", type=" << event_type
              << ", latency= " << latency.total_latency.InMilliseconds()
              << " ms";
    }
  }
}
}  // namespace ash
