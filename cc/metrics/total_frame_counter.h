// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_TOTAL_FRAME_COUNTER_H_
#define CC_METRICS_TOTAL_FRAME_COUNTER_H_

#include "base/time/time.h"
#include "cc/cc_export.h"

namespace viz {
struct BeginFrameArgs;
}

namespace cc {

// This class keeps track of how many vsyncs (frames) the compositor was visible
// for.
class CC_EXPORT TotalFrameCounter {
 public:
  TotalFrameCounter();

  TotalFrameCounter(const TotalFrameCounter&) = delete;
  TotalFrameCounter& operator=(const TotalFrameCounter&) = delete;

  void Reset();

  void OnShow(base::TimeTicks timestamp);
  void OnHide(base::TimeTicks timestamp);
  void OnBeginFrame(const viz::BeginFrameArgs& args);

  size_t ComputeTotalVisibleFrames(base::TimeTicks until) const;

  size_t total_frames() const { return total_frames_; }

  void set_total_frames_for_testing(size_t total_frames) {
    total_frames_ = total_frames;
  }

 private:
  void UpdateTotalFramesSinceLastVisible(base::TimeTicks until);

  size_t total_frames_ = 0;

  // The most recent vsync-interval set by the display compositor.
  // Set only if the compositor is currently visible, otherwise not set.
  base::TimeDelta latest_interval_;

  // The time the compositor was made visible.
  // Set only if the compositor is currently visible, otherwise not set.
  base::TimeTicks last_shown_timestamp_;
};

}  // namespace cc

#endif  // CC_METRICS_TOTAL_FRAME_COUNTER_H_
