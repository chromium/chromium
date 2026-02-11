// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_velocity_tracker.h"

#include <limits>

#include "base/check.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

namespace {
float ZeroOrSignedMaxFloat(float value) {
  if (value == 0.f) {
    return 0.f;
  }

  return value > 0.f ? std::numeric_limits<float>::max()
                     : -std::numeric_limits<float>::max();
}
}  // namespace

ScrollVelocityTracker::ScrollVelocityTracker(base::TimeDelta window_delta)
    : window_delta_(window_delta) {}

ScrollVelocityTracker::~ScrollVelocityTracker() = default;

gfx::Vector2dF ScrollVelocityTracker::CurrentVelocity() const {
  if (samples_.empty()) {
    return gfx::Vector2dF();
  }

  if (samples_.size() == 1) {
    const auto& scroll_delta = samples_.front().scroll_delta;

    return gfx::Vector2dF(ZeroOrSignedMaxFloat(scroll_delta.x()),
                          ZeroOrSignedMaxFloat(scroll_delta.y()));
  }

  const auto& oldest_sample = samples_.front();
  const auto& latest_sample = samples_.back();

  float duration_units =
      (latest_sample.timestamp - oldest_sample.timestamp).InMillisecondsF();
  CHECK_GT(duration_units, 0.f);

  gfx::Vector2dF total_delta;
  for (const auto& samples : samples_) {
    total_delta += samples.scroll_delta;
  }

  total_delta.InvScale(duration_units);
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
