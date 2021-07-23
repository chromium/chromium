// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_progress_ring_animation.h"
#include "ash/system/holding_space/holding_space_progress_ring.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/slide_animation.h"

namespace ash {

HoldingSpaceProgressRingAnimation::HoldingSpaceProgressRingAnimation(
    HoldingSpaceProgressRing* progress_ring,
    base::TimeDelta duration,
    bool is_cyclic)
    : progress_ring_(progress_ring),
      duration_(duration),
      is_cyclic_(is_cyclic) {}

HoldingSpaceProgressRingAnimation::~HoldingSpaceProgressRingAnimation() =
    default;

void HoldingSpaceProgressRingAnimation::Start() {
  StartInternal(/*is_cyclic_restart=*/false);
}

void HoldingSpaceProgressRingAnimation::AnimationProgressed(
    const gfx::Animation* animation) {
  UpdateAnimatableProperties(animation->GetCurrentValue(), &start_position_,
                             &end_position_);
  progress_ring_->InvalidateLayer();
}

void HoldingSpaceProgressRingAnimation::AnimationEnded(
    const gfx::Animation* animation) {
  if (is_cyclic_)
    StartInternal(/*is_cyclic_restart=*/true);
}

void HoldingSpaceProgressRingAnimation::StartInternal(bool is_cyclic_restart) {
  animator_ = std::make_unique<gfx::SlideAnimation>(this);
  animator_->SetSlideDuration(
      ui::ScopedAnimationDurationScaleMode::duration_multiplier() * duration_);
  animator_->SetTweenType(gfx::Tween::Type::LINEAR);

  if (!is_cyclic_restart)
    start_time_ = base::TimeTicks::Now();

  animator_->Show();
}

}  // namespace ash
