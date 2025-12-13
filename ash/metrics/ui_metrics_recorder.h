// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_UI_METRICS_RECORDER_H_
#define ASH_METRICS_UI_METRICS_RECORDER_H_

#include <optional>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "base/containers/span.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "cc/metrics/custom_metrics_recorder.h"
#include "cc/metrics/event_latency_tracker.h"

namespace ash {

// Records metrics for ash UI. Note this class is not thread-safe.
class ASH_EXPORT UiMetricsRecorder : public cc::CustomMetricRecorder {
 public:
  // Used to classify the frame rate when reporting event latency. These values
  // are used to map relevant metrics to corresponding histogram indexes. Keep
  // the list in sync with `EventLatencyFps` variant in ash/histograms.xml.
  enum class FpsBucket {
    k30Fps,
    k60Fps,
    k120Fps,
    kOtherFps,
    kUnset,
    kMaxValue = kUnset,
  };
  static constexpr int kMaxFpsBucketIndex =
      static_cast<int>(FpsBucket::kMaxValue) + 1;

  // A subset of cc::EventMetrics::EventType that treated as core events. This
  // is used with FpsBucket above are used to together to map to histogram
  // indexes without wasting memory on non-core event types. Keep the list in
  // sync with `EventLatencyCoreEventType` variant in ash/histograms.xml.
  enum class CoreEventType {
    kKeyPressed,
    kKeyReleased,
    kMousePressed,
    kMouseReleased,
    kMouseDragged,
    kMaxValue = kMouseDragged,
  };
  static constexpr int kMaxCoreEventTypeIndex =
      static_cast<int>(CoreEventType::kMaxValue) + 1;

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
      const viz::BeginFrameArgs& args,
      std::vector<cc::EventLatencyTracker::LatencyData> latencies) override;

  // Expose the fixed table of event latency histogram names for testing.
  static base::span<const std::string_view>
  GetEventLatencyHistogramNamesForTest();

  // Expose the fixed table of core event latency histogram names for testing.
  static base::span<const std::string_view>
  GetCoreEventLatencyHistogramNamesForTest();

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
