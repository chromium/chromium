// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/browser_controls_animation.h"

#include <algorithm>
#include <variant>

#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace cc {

BrowserControlsAnimation::Curve::Curve(gfx::Tween::Type type) : curve_(type) {}

BrowserControlsAnimation::Curve::Curve(const gfx::CubicBezier& bezier)
    : curve_(bezier) {}

double BrowserControlsAnimation::Curve::Solve(double progress) const {
  return std::visit(absl::Overload{
                        [progress](gfx::Tween::Type type) {
                          return gfx::Tween::CalculateValue(type, progress);
                        },
                        [progress](const gfx::CubicBezier& bezier) {
                          return bezier.Solve(progress);
                        },
                    },
                    curve_);
}

BrowserControlsAnimation::BrowserControlsAnimation() = default;

BrowserControlsAnimation::~BrowserControlsAnimation() = default;

void BrowserControlsAnimation::Initialize(Direction direction,
                                          float start_value,
                                          float stop_value,
                                          base::TimeDelta duration,
                                          bool jump_to_end_on_reset,
                                          const Curve& curve) {
  direction_ = direction;
  start_value_ = start_value;
  stop_value_ = stop_value;
  duration_ = duration;
  initialized_ = true;
  jump_to_end_on_reset_ = jump_to_end_on_reset;
  curve_ = curve;
  SetBounds(std::min(start_value_, stop_value_),
            std::max(start_value_, stop_value_));
}

std::optional<float> BrowserControlsAnimation::Tick(
    base::TimeTicks monotonic_time) {
  if (!IsInitialized()) {
    return std::nullopt;
  }

  if (!started_) {
    start_time_ = monotonic_time;
    stop_time_ = start_time_ + duration_;
    started_ = true;
  }

  float value = ValueAt(monotonic_time);

  if (IsComplete(value)) {
    value = FinalValue();
    Reset();
  }

  return value;
}

float BrowserControlsAnimation::ValueAt(base::TimeTicks monotonic_time) const {
  if (monotonic_time <= start_time_) {
    return start_value_;
  }
  if (monotonic_time >= stop_time_) {
    return stop_value_;
  }
  const double linear_progress =
      (monotonic_time - start_time_) / (stop_time_ - start_time_);
  return gfx::Tween::FloatValueBetween(curve_.Solve(linear_progress),
                                       start_value_, stop_value_);
}

void BrowserControlsAnimation::SetBounds(float min, float max) {
  min_value_ = min;
  max_value_ = max;
}

std::optional<float> BrowserControlsAnimation::Reset() {
  auto ret =
      jump_to_end_on_reset_ ? std::make_optional(FinalValue()) : std::nullopt;

  started_ = false;
  initialized_ = false;
  start_time_ = base::TimeTicks();
  start_value_ = 0.0f;
  stop_time_ = base::TimeTicks();
  stop_value_ = 0.0f;
  direction_ = Direction::kNoAnimation;
  duration_ = base::TimeDelta();
  min_value_ = 0.0f;
  max_value_ = 1.0f;
  jump_to_end_on_reset_ = false;
  curve_ = Curve(gfx::Tween::LINEAR);

  return ret;
}

bool BrowserControlsAnimation::IsComplete(float value) const {
  return (direction_ == Direction::kShowingControls &&
          (value >= stop_value_ || value >= max_value_)) ||
         (direction_ == Direction::kHidingControls &&
          (value <= stop_value_ || value <= min_value_));
}

float BrowserControlsAnimation::FinalValue() const {
  return std::clamp(stop_value_, min_value_, max_value_);
}

}  // namespace cc
