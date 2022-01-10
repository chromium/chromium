// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_progress_ring_animation.h"

#include "ash/system/holding_space/holding_space_progress_ring_indeterminate_animation.h"
#include "ash/system/holding_space/holding_space_progress_ring_pulse_animation.h"

namespace ash {

HoldingSpaceProgressRingAnimation::HoldingSpaceProgressRingAnimation(
    Type type,
    base::TimeDelta duration,
    bool is_cyclic)
    : HoldingSpaceProgressIndicatorAnimation(duration, is_cyclic),
      type_(type) {}

HoldingSpaceProgressRingAnimation::~HoldingSpaceProgressRingAnimation() =
    default;

// static
std::unique_ptr<HoldingSpaceProgressRingAnimation>
HoldingSpaceProgressRingAnimation::CreateOfType(Type type) {
  switch (type) {
    case Type::kIndeterminate:
      return std::make_unique<HoldingSpaceProgressRingIndeterminateAnimation>();
    case Type::kPulse:
      return std::make_unique<HoldingSpaceProgressRingPulseAnimation>();
  }
}

void HoldingSpaceProgressRingAnimation::UpdateAnimatableProperties(
    double fraction) {
  UpdateAnimatableProperties(fraction, &start_position_, &end_position_,
                             &opacity_);
}

}  // namespace ash
