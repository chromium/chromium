// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_JANK_METRICS_H_
#define CC_METRICS_JANK_METRICS_H_

#include <memory>
#include <queue>
#include <utility>

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
  void AddPresentedFrame(uint32_t presented_frame_token,
                         base::TimeTicks current_presentation_timestamp,
                         base::TimeDelta frame_interval);

  // Report the occurrence rate of janks as a UMA metric.
  void ReportJankMetrics(int frames_expected);

  // Merge the current jank count with a previously unreported jank metrics.
  void Merge(std::unique_ptr<JankMetrics> jank_metrics);

  void AddSubmitFrame(uint32_t frame_token, uint32_t sequence_number);

  void AddFrameWithNoUpdate(uint32_t sequence_number,
                            base::TimeDelta frame_interval);
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

  // The sequence number associated with the last presented frame
  uint32_t last_presentation_frame_id_ = 0u;

  // The interval before the previous frame presentation.
  base::TimeDelta prev_frame_delta_;

  // A queue storing {frame token, sequence number} for all submitted
  // frames, in ascending order of frame token.
  std::queue<std::pair<uint32_t, uint32_t>> queue_frame_token_and_id_;

  // A queue storing {sequence number, frame interval} of unprocessed no-update
  // frames, in ascending order of sequence number.
  std::queue<std::pair<uint32_t, base::TimeDelta>> queue_frame_id_and_interval_;
};

}  // namespace cc

#endif  // CC_METRICS_JANK_METRICS_H_
