// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/total_frame_counter.h"

#include <cmath>

#include "base/logging.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc {

TotalFrameCounter::TotalFrameCounter() = default;

void TotalFrameCounter::OnShow(base::TimeTicks timestamp) {
  DCHECK(last_shown_timestamp_.is_null());
  DCHECK(latest_interval_.is_zero());
  last_shown_timestamp_ = timestamp;
}

void TotalFrameCounter::OnHide(base::TimeTicks timestamp) {
  // It is possible to hide right after being shown before receiving any
  // BeginFrameArgs.
  if (!latest_interval_.is_zero())
    UpdateTotalFramesSinceLastVisible(timestamp);
  last_shown_timestamp_ = base::TimeTicks();
  latest_interval_ = base::TimeDelta();
}

void TotalFrameCounter::OnBeginFrame(const viz::BeginFrameArgs& args) {
  // In tests, it is possible to receive begin-frames when invisible. Ignore
  // these.
  if (last_shown_timestamp_.is_null())
    return;

  if (!latest_interval_.is_zero() && latest_interval_ != args.interval) {
    UpdateTotalFramesSinceLastVisible(args.frame_time);
    last_shown_timestamp_ = args.frame_time;
  }

  latest_interval_ = args.interval;
}

void TotalFrameCounter::Reset() {
  total_frames_ = 0;
  latest_interval_ = {};
  // If the compositor is visible, then update the visible timestamp to current
  // time.
  if (!last_shown_timestamp_.is_null())
    last_shown_timestamp_ = base::TimeTicks::Now();
}

void TotalFrameCounter::UpdateTotalFramesSinceLastVisible(
    base::TimeTicks until) {
  total_frames_ = ComputeTotalVisibleFrames(until);
}

size_t TotalFrameCounter::ComputeTotalVisibleFrames(
    base::TimeTicks until) const {
  DCHECK(!until.is_null());

  if (last_shown_timestamp_.is_null() || latest_interval_.is_zero()) {
    // The compositor may be currently invisible, or has just been made visible
    // but has yet to receive a BeginFrameArgs.
    return total_frames_;
  }

  // We have two sources for timestamps. Show/Hide uses the Renderer time
  // source. While viz::BeginFrameArgs will be either timestamps from the
  // physical GPU, or fallbacks in the GPU/Viz process. This could be the cause
  // of a drift. We don't error on these edgecases, we just return the
  // `total_frames_` which reflects the latest OnBeginFrame.
  if (until < last_shown_timestamp_) {
    return total_frames_;
  }
  auto frames_since =
      std::ceil((until - last_shown_timestamp_) / latest_interval_);
  return total_frames_ + frames_since;
}

}  // namespace cc
