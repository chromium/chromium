// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_RING_INDETERMINATE_ANIMATION_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_RING_INDETERMINATE_ANIMATION_H_

#include "ash/system/holding_space/holding_space_progress_ring_animation.h"

namespace ash {

// An animation for a `HoldingSpaceProgressRing` to paint an indeterminate
// progress ring in lieu of the determinate progress ring that would otherwise
// be painted.
class HoldingSpaceProgressRingIndeterminateAnimation
    : public HoldingSpaceProgressRingAnimation {
 public:
  HoldingSpaceProgressRingIndeterminateAnimation();
  HoldingSpaceProgressRingIndeterminateAnimation(
      const HoldingSpaceProgressRingIndeterminateAnimation&) = delete;
  HoldingSpaceProgressRingIndeterminateAnimation& operator=(
      const HoldingSpaceProgressRingIndeterminateAnimation&) = delete;
  ~HoldingSpaceProgressRingIndeterminateAnimation() override;

 private:
  // HoldingSpaceProgressRingAnimation:
  void UpdateAnimatableProperties(double fraction,
                                  float* start_value,
                                  float* end_value,
                                  float* opacity) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_RING_INDETERMINATE_ANIMATION_H_
