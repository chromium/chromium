// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_PREDICTOR_JANK_TRACKER_H_
#define CC_METRICS_PREDICTOR_JANK_TRACKER_H_

#include <optional>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_ukm_reporter.h"

namespace cc {

class CC_EXPORT PredictorJankTracker {
 public:
  PredictorJankTracker();
  ~PredictorJankTracker();

  PredictorJankTracker(const PredictorJankTracker&) = delete;
  PredictorJankTracker& operator=(const PredictorJankTracker);

  // Emits predictor scroll jank metrics for every frame relative
  // to the previous and the next frame.
  // For more details about the how a frame is deemed janky after
  // delta prediction is applied please check:
  // http://doc/1Y0u0Tq5eUZff75nYUzQVw6JxmbZAW9m64pJidmnGWsY
  void ReportLatestScrollDelta(float delta,
                               base::TimeTicks presentation_ts,
                               base::TimeDelta vsync_interval,
                               std::optional<EventMetrics::TraceId> trace_id);

  // Whenever a new scroll starts, data inside this class will be erased
  // as it should be comparing neighbouring frames only.
  void ResetCurrentScrollReporting();

  void set_scroll_jank_ukm_reporter(
      ScrollJankUkmReporter* scroll_jank_ukm_reporter) {
    scroll_jank_ukm_reporter_ = scroll_jank_ukm_reporter;
  }

  static float GetSlowScrollDeltaThreshold();
  static float GetSlowScrollJankyThreshold();
  static float GetFastScrollJankyThreshold();

 private:
  // The metric works by storing a sliding window of the previous two
  // frames, this function moves the sliding window storing the newer
  // frame information.
  void StoreLatestFrameData(float delta,
                            base::TimeTicks presentation_ts,
                            std::optional<EventMetrics::TraceId> trace_id);

  void ReportJankyFrame(float next_delta,
                        float janky_value,
                        bool contains_missed_vsyncs,
                        bool slow_scroll,
                        std::optional<EventMetrics::TraceId> trace_id);

  // Finds if a sequence of 3 consecutive frames were presnted in
  // consecutive vsyncs, or some vsyncs were missed.
  bool ContainsMissedVSync(base::TimeTicks& presentation_ts,
                           base::TimeDelta& vsync_interval);
  void ReportJankyFramePercentage();

  // Data holder for deltas and presentation timestamps in previous frames
  struct FrameData {
    // Delta for the previous frame in pixels.
    float prev_delta_ = 0;
    // The EventLatency event_trace_id value if available.
    std::optional<EventMetrics::TraceId> prev_trace_id_;
    // Delta for the current frame in pixels.
    float cur_delta_ = 0;
    // The EventLatency event_trace_id value if available.
    std::optional<EventMetrics::TraceId> cur_trace_id_;

    // Presentation timestamp of the previous frame.
    base::TimeTicks prev_presentation_ts_;
    // Presentation timestamp for the currentframe.
    base::TimeTicks cur_presentation_ts_;
  } frame_data_;

  float total_frames_ = 0;
  float janky_frames_ = 0;

  raw_ptr<ScrollJankUkmReporter> scroll_jank_ukm_reporter_ = nullptr;
};

}  // namespace cc

#endif  // CC_METRICS_PREDICTOR_JANK_TRACKER_H_
