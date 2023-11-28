// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_UI_METRICS_RECORDER_H_
#define ASH_METRICS_UI_METRICS_RECORDER_H_

#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "cc/metrics/custom_metrics_recorder.h"
#include "cc/metrics/event_latency_tracker.h"

namespace ash {

// Records metrics for ash UI. Note this class is not thread-safe.
class ASH_EXPORT UiMetricsRecorder : public cc::CustomMetricRecorder {
 public:
  UiMetricsRecorder();
  ~UiMetricsRecorder() override;

  // Invoked on a user login. This is expected to be called after cryptohome
  // mount but before user profile loading.
  void OnUserLoggedIn();

  // Invoked after post-login animation finishes.
  void OnPostLoginAnimationFinish();

  // cc::CustomMetricRecorder:
  void ReportPercentDroppedFramesInOneSecondWindow2(double percent) override;
  void ReportEventLatency(
      std::vector<cc::EventLatencyTracker::LatencyData> latencies) override;

 private:
  // State to split "Ash.Smoothness.PercentDroppedFrames_1sWindow".
  enum class State {
    kBeforeLogin,
    kDuringLogin,
    kInSession,
  };

  State state_ = State::kBeforeLogin;
  SEQUENCE_CHECKER(sequence_checker_);

  // Login time and session start time of the primary user.
  std::optional<base::TimeTicks> user_logged_in_time_;
  std::optional<base::TimeTicks> user_session_start_time_;

  std::optional<base::TimeTicks> last_good_dropped_frame_time_;
};

}  // namespace ash

#endif  // ASH_METRICS_UI_METRICS_RECORDER_H_
