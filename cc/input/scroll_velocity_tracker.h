// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLL_VELOCITY_TRACKER_H_
#define CC_INPUT_SCROLL_VELOCITY_TRACKER_H_

#include "base/containers/circular_deque.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

// A moving time-window scroll velocity tracker.
class CC_EXPORT ScrollVelocityTracker {
 public:
  // Initializes a scroll velocity tracker object that only retains samples
  // recorded at most `window_delta` before the latest sample.
  explicit ScrollVelocityTracker(base::TimeDelta window_delta);
  ~ScrollVelocityTracker();

  // Returns the current velocity in scroll units per millisecond. The velocity
  // is the sum of all scroll deltas divided by the duration between the oldest
  // and latest sample in milliseconds.
  //
  // NOTE:
  //  - If there are no samples, the velocity is zero.
  //  - If there is only one sample, the velocity is zero in the directions
  //    where the sample scroll delta is zero, positive max-float in the
  //    directions where the sample scroll delta is positive and negative
  //    max-float in the directions where the sample scroll delta is negative.
  gfx::Vector2dF CurrentVelocity() const;

  // Adds a scroll delta to the set of samples. All previously added samples
  // older than the window duration from `timestamp` will be discarded.
  void AddSample(base::TimeTicks timestamp, const gfx::Vector2dF& scroll_detla);

  // Discards all recorded samples and resets state.
  void Reset();

 private:
  struct Sample {
    base::TimeTicks timestamp;
    gfx::Vector2dF scroll_delta;
  };

  base::circular_deque<Sample> samples_;
  const base::TimeDelta window_delta_;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLL_VELOCITY_TRACKER_H_
