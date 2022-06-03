// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_progress_ring_animation.h"
#include "ash/system/holding_space/holding_space_progress_ring_indeterminate_animation.h"
#include "ash/system/holding_space/holding_space_progress_ring_pulse_animation.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/slide_animation.h"

namespace ash {

HoldingSpaceProgressRingAnimation::HoldingSpaceProgressRingAnimation(
    Type type,
    base::TimeDelta duration,
    bool is_cyclic)
    : type_(type), duration_(duration), is_cyclic_(is_cyclic) {}

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

base::RepeatingClosureList::Subscription
HoldingSpaceProgressRingAnimation::AddAnimationUpdatedCallback(
    base::RepeatingClosureList::CallbackType callback) {
  return animation_updated_callback_list_.Add(std::move(callback));
}

void HoldingSpaceProgressRingAnimation::Start() {
  StartInternal(/*is_cyclic_restart=*/false);
}

bool HoldingSpaceProgressRingAnimation::IsAnimating() const {
  return animator_ && animator_->is_animating();
}

void HoldingSpaceProgressRingAnimation::AnimationProgressed(
    const gfx::Animation* animation) {
  UpdateAnimatableProperties(animation->GetCurrentValue(), &start_position_,
                             &end_position_, &opacity_);
  animation_updated_callback_list_.Notify();
}

void HoldingSpaceProgressRingAnimation::AnimationEnded(
    const gfx::Animation* animation) {
  if (!is_cyclic_) {
    animation_updated_callback_list_.Notify();
    return;
  }

  // In tests, animations may be scaled such that duration is zero. When this
  // happens, we need to post cyclic restarts rather than restarting them
  // immediately. Otherwise the animation will loop endlessly without providing
  // other code an opportunity to run.
  if (ui::ScopedAnimationDurationScaleMode::duration_multiplier() == 0.f) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&HoldingSpaceProgressRingAnimation::StartInternal,
                       weak_factory_.GetWeakPtr(), /*is_cyclic_restart=*/true));
    return;
  }

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
