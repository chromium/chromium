// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_FRAME_RATE_ESTIMATOR_H_
#define CC_TREES_FRAME_RATE_ESTIMATOR_H_

#include <cstdint>

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "cc/base/delayed_unique_notifier.h"
#include "cc/cc_export.h"

namespace cc {

// The class is used to decide the preferred begin frame rate which related
// compositor frame sink should send based on video conference mode,
// NotifyInputEvent, WillDraw and DidNotProduceFrame. It will enter
// InputPriorityMode in the next 250ms(kInputPriorityDelay) if
// NotifyInputEvent() is called and exit after the delay. Throttling is
// disabled in InputPriorityMode despite other conditions.
// 1. In video conference mode:
// The frame rate will be throttled to half of the default when：
//   Video conference mode is set and
//   the last drawn consecutive_frames_with_min_delta_ <
//   4(kMinNumOfFramesWithMinDelta).
// The throttling will be stopped when:
//   Video conference mode is unset or
//   the last drawn consecutive_frames_with_min_delta_ >= 4.
// 2. In consecutive didNotProduceFrame
// mode(features::kThrottleFrameRateOnManyDidNotProduceFrame):
// The frame rate will be throttled to half of the default when：
//   The num_did_not_produce_frame_since_last_draw_ >
//   4(kNumDidNotProduceFrameBeforeThrottle).
// The throttling will be stopped when:
//   There's a drawn frame.
class CC_EXPORT FrameRateEstimator {
 public:
  explicit FrameRateEstimator(base::SequencedTaskRunner* task_runner);
  ~FrameRateEstimator();

  void SetVideoConferenceMode(bool enabled);
  void WillDraw(base::TimeTicks now);
  void NotifyInputEvent();
  base::TimeDelta GetPreferredInterval() const;
  bool input_priority_mode() const { return input_priority_mode_; }
  void DidNotProduceFrame();

 private:
  void OnExitInputPriorityMode();

  // Number of "did not produce frame" since the last draw.
  uint64_t num_did_not_produce_frame_since_last_draw_ = 0;

  // Whether videoconference mode is enabled. In this mode, frame rate is
  // reduced when there is no recent input and many frames with a small
  // interval were produced.
  bool in_video_conference_mode_ = false;

  // Time of the last draw while in videoconference mode. Reset when exiting
  // videoconference mode.
  base::TimeTicks last_draw_time_in_video_conference_mode_;

  // Number of consecutive frames drawn in videoconference mode with an interval
  // lower than twice the default interval.
  uint64_t num_of_consecutive_frames_with_min_delta_ = 0u;

  // We conservatively switch to high frame rate after an input event to lower
  // the input latency for a minimum duration. This tracks when we are in this
  // mode.
  bool input_priority_mode_ = false;
  DelayedUniqueNotifier notifier_;
};

}  // namespace cc

#endif  // CC_TREES_FRAME_RATE_ESTIMATOR_H_
