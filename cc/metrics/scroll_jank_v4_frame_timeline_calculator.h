// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_FRAME_TIMELINE_CALCULATOR_H_
#define CC_METRICS_SCROLL_JANK_V4_FRAME_TIMELINE_CALCULATOR_H_

#include <memory>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "cc/metrics/scroll_jank_v4_frame_stage_calculator.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc {

class CC_EXPORT ScrollJankV4FrameTimelineCalculator {
 public:
  ScrollJankV4FrameTimelineCalculator();
  ~ScrollJankV4FrameTimelineCalculator();

  // Calculates the frame timeline (for the purposes of evaluating scroll jank)
  // based on `events_metrics` which were first presented at `presentation_ts`
  // in a frame with `presented_args`.
  //
  // This method groups scroll updates and ends into frames as follows:
  //
  //   * If there's at least one damaging scroll update/end in `events_metrics`:
  //
  //       1. This method finds the minimum begin frame ID
  //          (`scroll_event.begin_frame_args().frame_id`) over all damaging
  //          scroll events.
  //       2. It associates all scroll events whose begin frame ID is greater
  //          than or equal to the minimum damaging begin frame ID with the
  //          presented frame (`presented_args`). It marks the presented frame
  //          as damaging.
  //       3. It associates all scroll events whose begin frame ID is less than
  //          the minimum damaging begin frame ID with their original frame
  //          (`scroll_event.begin_frame_args()`). It marks each of the original
  //          frames as non-damaging.
  //
  //   * If there are no damaging scroll updates/ends in `events_metrics`, this
  //     method assigns all scroll events to their original frames
  //     (`scroll_event.begin_frame_args()`). It marks each of the original
  //     frames as non-damaging. Note that, if any of the scroll events'
  //     original frame is the presented frame (i.e.
  //     `scroll_event.begin_frame_args().frame_id == presented_args.frame_id`),
  //     this method will mark the presented frame as non-damaging as well.
  //
  // This method returns a timeline of frames sorted in ascending order of begin
  // frame ID (`frame.args->frame_id`). Each frame in the returned timeline is
  // guaranteed to have different begin frame arguments. This method assumes
  // that the presented frame's begin frame ID is greater than or equal to the
  // begin frame IDs of all scroll events in `events_metrics` (i.e.
  // `presented_frame.frame_id >= scroll_event.begin_frame_args().frame_id`).
  // Given this assumption, all frames in the returned timeline will be
  // non-damaging EXCEPT the last frame, which can be either damaging or
  // non-damaging. In other words, the timeline matches the informal regular
  // expression "(non-damaging-frame)*(damaging-frame)?". If the last frame is
  // damaging, its begin frame arguments will be `presented_args`. If
  // `events_metrics` doesn't contain any scroll updates/ends, this method will
  // return an empty timeline.
  //
  // This method assigns a unique result ID to each frame in the returned
  // timeline (`ScrollJankV4Frame::BeginFrameArgsForScrollJank::result_id`) and
  // sets `ScrollEventMetrics::scroll_jank_v4_result_id()` to `result_id` for
  // all scroll updates and ends which this method uses to calculate the frame's
  // stages. Otherwise doesn't modify `event_metrics`. This method does NOT
  // require that `events_metrics` is sorted.
  ScrollJankV4Frame::Timeline CalculateTimeline(
      EventMetrics::List& events_metrics,
      const viz::BeginFrameArgs& presented_args,
      base::TimeTicks presentation_ts);

 private:
  std::unique_ptr<ScrollJankV4FrameStageCalculator> stage_calculator_ =
      ScrollJankV4FrameStageCalculator::Create();
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_FRAME_TIMELINE_CALCULATOR_H_
