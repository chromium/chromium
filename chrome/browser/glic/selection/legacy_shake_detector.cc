// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/legacy_shake_detector.h"

namespace glic {

namespace {

// Minimum distance in DIPs required for a mouse move step to establish or
// change movement direction.
constexpr float kMinShakeDistance = 10.0f;
// Required number of direction changes to detect a shake.
constexpr int kRequiredDirectionChanges = 4;
// Maximum time allowed between direction changes before the detector resets.
constexpr base::TimeDelta kShakeTimeout = base::Milliseconds(1000);

}  // namespace

LegacyShakeDetector::LegacyShakeDetector() = default;

LegacyShakeDetector::~LegacyShakeDetector() = default;

bool LegacyShakeDetector::AddSample(const gfx::PointF& position_in_dips,
                                    base::TimeTicks timestamp) {
  if (direction_change_count_ > 0 &&
      (timestamp - last_direction_change_time_) > kShakeTimeout) {
    Reset();
  }

  if (!last_point_.has_value()) {
    last_point_ = position_in_dips;
    return false;
  }

  gfx::Vector2dF delta = position_in_dips - *last_point_;
  float dist = delta.Length();
  if (dist < kMinShakeDistance) {
    return false;
  }

  gfx::Vector2dF current_dir(delta.x() / dist, delta.y() / dist);

  if (!last_direction_.has_value()) {
    last_direction_ = current_dir;
    last_point_ = position_in_dips;
    last_direction_change_time_ = timestamp;
    return false;
  }

  float dot = last_direction_->x() * current_dir.x() +
              last_direction_->y() * current_dir.y();
  if (dot >= -0.5f) {
    last_point_ = position_in_dips;
    return false;
  }

  direction_change_count_++;
  last_direction_ = current_dir;
  last_point_ = position_in_dips;
  last_direction_change_time_ = timestamp;
  if (direction_change_count_ < kRequiredDirectionChanges) {
    return false;
  }
  Reset();
  return true;
}

void LegacyShakeDetector::Reset() {
  last_point_.reset();
  last_direction_.reset();
  direction_change_count_ = 0;
  last_direction_change_time_ = base::TimeTicks();
}

}  // namespace glic
