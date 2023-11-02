// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_JANK_METRICS_H_
#define CC_METRICS_JANK_METRICS_H_

#include <memory>
#include <queue>
#include <utility>

#include "base/time/time.h"
#include "cc/metrics/frame_sequence_metrics.h"

namespace cc {
// This class reports 3 sets of metrics related to janks:
// - Graphics.Smoothness.Jank.*: Percent of frames that have longer
//                               presentation interval than its previous frame.
//                               Reports one percentage number per tracker.
// - Graphics.Smoothness.Stale.*: The difference between the real presentation
//                                interval and its expected value. Reports one
//                                TimeDelta per frame.
// - Graphics.Smoothness.MaxStale.*: The maximum staleness value that occurred
//                                   during the course of an interaction.
//                                   Reports one TimeDelta per tracker.
class CC_EXPORT JankMetrics {
 public:
  JankMetrics(FrameSequenceTrackerType tracker_type,
              FrameInfo::SmoothEffectDrivingThread effective_thread);
  ~JankMetrics();

  JankMetrics(const JankMetrics&) = delete;
  JankMetrics& operator=(const JankMetrics&) = delete;

  void AddFrameWithNoUpdate(uint32_t sequence_number,
                            base::TimeDelta frame_interval);

  // Check if a jank occurs based on the timestamps of recent presentations.
  // If there is a jank, increment |jank_count_| and log a trace event.
  // Graphics.Smoothness.Stale.* metrics are reported in this function.
  void AddPresentedFrame(uint32_t presented_frame_token,
                         base::TimeTicks current_presentation_timestamp,
                         base::TimeDelta frame_interval);

  void AddSubmitFrame(uint32_t frame_token, uint32_t sequence_number);

  // Merge the current jank count with a previously unreported jank metrics.
  void Merge(std::unique_ptr<JankMetrics> jank_metrics);

  // Report Graphics.Smoothness.(Jank|MaxStale).* metrics.
  void ReportJankMetrics(int frames_expected);

  // Reset the internal jank count
  void Reset();

  int jank_count() const { return jank_count_; }

  base::TimeDelta max_staleness() const { return max_staleness_; }
  FrameInfo::SmoothEffectDrivingThread thread_type() const {
    return effective_thread_;
  }

 private:
  // The type of the tracker this JankMetrics object is attached to.
  const FrameSequenceTrackerType tracker_type_;

  // The thread that contributes to the janks detected by the current
  // JankMetrics object.
  const FrameInfo::SmoothEffectDrivingThread effective_thread_;

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

  // The maximum frame staleness that occurred during the tracker's lifetime.
  base::TimeDelta max_staleness_;
};

}  // namespace cc

#endif  // CC_METRICS_JANK_METRICS_H_
