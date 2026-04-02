// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_velocity_tracker.h"

#include <limits>

#include "base/check.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

ScrollVelocityTracker::ScrollVelocityTracker(base::TimeDelta window_delta)
    : window_delta_(window_delta) {}

ScrollVelocityTracker::~ScrollVelocityTracker() = default;

gfx::Vector2dF ScrollVelocityTracker::CurrentVelocity() const {
  if (samples_.empty()) {
    return gfx::Vector2dF();
  }

  // Find the smallest duration encompassing all samples. If there is only one
  // sample, use the window delta as an approximation.
  float duration_ms = window_delta_.InMillisecondsF();
  if (samples_.size() > 1) {
    const auto& oldest_sample = samples_.front();
    const auto& latest_sample = samples_.back();

    duration_ms =
        (latest_sample.timestamp - oldest_sample.timestamp).InMillisecondsF();
  }
  CHECK_GT(duration_ms, 0.f);

  gfx::Vector2dF total_delta;
  for (const auto& samples : samples_) {
    total_delta += samples.scroll_delta;
  }

  total_delta.InvScale(duration_ms);
  return total_delta;
}

void ScrollVelocityTracker::AddSample(base::TimeTicks timestamp,
                                      const gfx::Vector2dF& scroll_delta) {
  if (window_delta_ != base::TimeDelta::Max()) {
    // Remove samples older than `window_`
    base::EraseIf(samples_, [&](const Sample& sample) {
      return (timestamp - sample.timestamp) > window_delta_;
    });
  }

  if (!samples_.empty()) {
    // Timestamps are assumed to be monotonically increasing. If there are two
    // samples for the same timestamp, they are coalesced here to prevent edge
    // cases while calculating velocity.
    CHECK_LE(samples_.back().timestamp, timestamp);
    if (samples_.back().timestamp == timestamp) {
      samples_.back().scroll_delta += scroll_delta;
      return;
    }
  }

  samples_.emplace_back(timestamp, scroll_delta);
}

void ScrollVelocityTracker::Reset() {
  samples_.clear();
}

}  // namespace cc
