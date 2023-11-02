// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_FRAME_RATE_ESTIMATOR_H_
#define CC_TREES_FRAME_RATE_ESTIMATOR_H_

#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "cc/base/delayed_unique_notifier.h"
#include "cc/cc_export.h"

namespace cc {

class CC_EXPORT FrameRateEstimator {
 public:
  explicit FrameRateEstimator(base::SequencedTaskRunner* task_runner);
  ~FrameRateEstimator();

  void SetFrameEstimationEnabled(bool enabled);
  void WillDraw(base::TimeTicks now);
  void NotifyInputEvent();
  base::TimeDelta GetPreferredInterval() const;
  bool input_priority_mode() const { return input_priority_mode_; }

 private:
  void OnExitInputPriorityMode();

  // Set if an estimated frame rate should be used or we should assume the
  // highest frame rate available.
  bool frame_rate_estimation_enabled_ = false;

  // The frame time for the last drawn frame since frame estimation was
  // enabled.
  base::TimeTicks last_draw_time_;

  // The number of consecutive frames drawn within the time delta required to
  // lower the frame rate.
  size_t num_of_consecutive_frames_with_min_delta_ = 0u;

  // We conservatively switch to high frame rate after an input event to lower
  // the input latency for a minimum duration. This tracks when we are in this
  // mode.
  bool input_priority_mode_ = false;
  DelayedUniqueNotifier notifier_;
};

}  // namespace cc

#endif  // CC_TREES_FRAME_RATE_ESTIMATOR_H_
