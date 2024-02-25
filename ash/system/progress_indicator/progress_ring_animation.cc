// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/progress_indicator/progress_ring_animation.h"

#include "ash/system/progress_indicator/progress_ring_indeterminate_animation.h"
#include "ash/system/progress_indicator/progress_ring_pulse_animation.h"

namespace ash {

ProgressRingAnimation::ProgressRingAnimation(Type type,
                                             base::TimeDelta duration,
                                             bool is_cyclic)
    : ProgressIndicatorAnimation(duration, is_cyclic), type_(type) {}

ProgressRingAnimation::~ProgressRingAnimation() = default;

// static
std::unique_ptr<ProgressRingAnimation> ProgressRingAnimation::CreateOfType(
    Type type) {
  std::unique_ptr<ProgressRingAnimation> animation;

  switch (type) {
    case Type::kIndeterminate:
      animation = std::make_unique<ProgressRingIndeterminateAnimation>();
      break;
    case Type::kPulse:
      animation = std::make_unique<ProgressRingPulseAnimation>();
      break;
  }

  animation->Init();
  return animation;
}

void ProgressRingAnimation::UpdateAnimatableProperties(double fraction) {
  UpdateAnimatableProperties(fraction, &start_position_, &end_position_,
                             &outer_ring_opacity_);
}

}  // namespace ash
