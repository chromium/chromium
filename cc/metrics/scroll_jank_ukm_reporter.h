// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_UKM_REPORTER_H_
#define CC_METRICS_SCROLL_JANK_UKM_REPORTER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"

namespace cc {
class UkmManager;

class CC_EXPORT ScrollJankUkmReporter {
 public:
  ScrollJankUkmReporter();
  ~ScrollJankUkmReporter();

  ScrollJankUkmReporter(const ScrollJankUkmReporter&) = delete;

  void IncrementFrameCount();
  void IncrementDelayedFrameCount();
  void AddVsyncs(int vsyncs);
  void AddMissedVsyncs(int missed_vsyncs);
  void IncrementPredictorJankyFrames();
  void SetEarliestScrollEvent(ScrollUpdateEventMetrics& earliest_event);

  void EmitScrollJankUkm();
  void UpdateLatestFrameAndEmitPredictorJank(base::TimeTicks latest_timestamp);
  void ResetPredictorMetrics();

  void set_max_missed_vsyncs(int max_missed_vsyncs) {
    max_missed_vsyncs_ = max_missed_vsyncs;
  }

  void set_max_delta(int max_delta) { max_delta_ = max_delta; }

  void set_frame_with_missed_vsync(int frame_with_missed_vsync) {
    frame_with_missed_vsync_ = frame_with_missed_vsync;
  }

  void set_frame_with_no_missed_vsync(int frame_with_no_missed_vsync) {
    frame_with_no_missed_vsync_ = frame_with_no_missed_vsync;
  }

  void set_ukm_manager(UkmManager* manager) { ukm_manager_ = manager; }

  void set_first_frame_timestamp_for_testing(base::TimeTicks timestamp) {
    first_frame_timestamp_ = timestamp;
  }

  void set_latest_frame_timestamp_for_testing(base::TimeTicks timestamp) {
    final_frame_presentation_timestamp_ = timestamp;
  }

 private:
  void WriteScrollTraceEvent();

  // Scroll metrics
  int num_frames_ = 0;
  int num_vsyncs_ = 0;
  int num_missed_vsyncs_ = 0;
  int max_missed_vsyncs_ = 0;
  int num_delayed_frames_ = 0;
  int predictor_jank_frames_ = 0;

  // Scroll update/predictor metrics

  // The max delta can be used to determine if this is a fast or slow scroll.
  // If this value is > kScrollDeltaThreshold in PredictorJankTracker, then the
  // scroll is fast. This value can also let us know the jank threshold
  // (kSlowJankyThreshold or kFastJankyThreshold in PredictorJankTracker).
  int max_delta_ = 0;

  // These values represent the PredictorJankTracker janky_value. These values
  // are only recorded if they are larger than the jank threshold.
  int frame_with_missed_vsync_ = 0;
  int frame_with_no_missed_vsync_ = 0;

  // Top level scroll trace event values.
  base::TimeTicks first_frame_timestamp_ = base::TimeTicks::Min();
  base::TimeTicks final_frame_presentation_timestamp_ = base::TimeTicks::Min();

  // This is pointing to the LayerTreeHostImpl::ukm_manager_, which is
  // initialized right after the LayerTreeHostImpl is created. So when this
  // pointer is initialized, there should be no trackers yet. Moreover, the
  // LayerTreeHostImpl::ukm_manager_ lives as long as the LayerTreeHostImpl, so
  // this pointer should never be null as long as LayerTreeHostImpl is alive.
  raw_ptr<UkmManager> ukm_manager_ = nullptr;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_UKM_REPORTER_H_
