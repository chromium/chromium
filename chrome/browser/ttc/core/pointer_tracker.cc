// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/pointer_tracker.h"

#include <algorithm>

namespace ttc {

PointerTracker::PointerTracker() : PointerTracker(Config()) {}

PointerTracker::PointerTracker(const Config& config) : config_(config) {}

PointerTracker::~PointerTracker() = default;

void PointerTracker::RecordSample(base::TimeTicks time,
                                  const gfx::PointF& point,
                                  bool is_press) {
  if (!samples_.empty()) {
    if (!is_press && point == samples_.back().point) {
      return;
    }
    time = std::max(time, samples_.back().time);
  }
  samples_.emplace_back(point, time, is_press);
  Prune(time);
}

std::optional<PointerTracker::Sample> PointerTracker::ResolveSample(
    base::TimeTicks now) const {
  // If the pointer is still active, the user is most likely pointing at
  // wherever it is now; if it has been idle for too long it is parked.
  if (samples_.empty() || samples_.back().time < now - config_.idle_limit) {
    return std::nullopt;
  }
  return samples_.back();
}

void PointerTracker::Clear() {
  samples_.clear();
}

void PointerTracker::Prune(base::TimeTicks now) {
  const base::TimeTicks oldest_allowed = now - config_.history;
  while (!samples_.empty() && samples_.front().time < oldest_allowed) {
    samples_.pop_front();
  }
  while (samples_.size() > config_.max_samples) {
    samples_.pop_front();
  }
}

}  // namespace ttc
