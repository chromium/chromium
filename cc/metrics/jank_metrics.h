// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_JANK_METRICS_H_
#define CC_METRICS_JANK_METRICS_H_

#include <memory>

#include "cc/metrics/frame_sequence_metrics.h"

namespace cc {
class CC_EXPORT JankMetrics {
 public:
  JankMetrics(FrameSequenceTrackerType tracker_type,
              FrameSequenceMetrics::ThreadType effective_thread);
  ~JankMetrics();

  JankMetrics(const JankMetrics&) = delete;
  JankMetrics& operator=(const JankMetrics&) = delete;

  // Check if a jank occurs based on the timestamps of recent presentations.
  // If there is a jank, increment |jank_count_| and log a trace event.
  void AddPresentedFrame(base::TimeTicks current_presentation_timestamp,
                         base::TimeDelta frame_interval);

  // Report the occurrence rate of janks as a UMA metric.
  void ReportJankMetrics(int frames_expected);

  // Merge the current jank count with a previously unreported jank metrics.
  void Merge(std::unique_ptr<JankMetrics> jank_metrics);

  FrameSequenceMetrics::ThreadType thread_type() const {
    return effective_thread_;
  }

 private:
  // The type of the tracker this JankMetrics object is attached to.
  const FrameSequenceTrackerType tracker_type_;

  // The thread that contributes to the janks detected by the current
  // JankMetrics object.
  const FrameSequenceMetrics::ThreadType effective_thread_;

  // Number of janks detected.
  int jank_count_ = 0;

  // The time when the last presentation occurs
  base::TimeTicks last_presentation_timestamp_;

  // The interval before the previous frame presentation.
  base::TimeDelta prev_frame_delta_;
};

}  // namespace cc

#endif  // CC_METRICS_JANK_METRICS_H_
