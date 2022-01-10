// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_progress_indicator_animation.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/slide_animation.h"

namespace ash {

HoldingSpaceProgressIndicatorAnimation::HoldingSpaceProgressIndicatorAnimation(
    base::TimeDelta duration,
    bool is_cyclic)
    : duration_(duration), is_cyclic_(is_cyclic) {}

HoldingSpaceProgressIndicatorAnimation::
    ~HoldingSpaceProgressIndicatorAnimation() = default;

base::CallbackListSubscription
HoldingSpaceProgressIndicatorAnimation::AddAnimationUpdatedCallback(
    base::RepeatingClosureList::CallbackType callback) {
  return animation_updated_callback_list_.Add(std::move(callback));
}

void HoldingSpaceProgressIndicatorAnimation::Start() {
  StartInternal(/*is_cyclic_restart=*/false);
}

bool HoldingSpaceProgressIndicatorAnimation::IsAnimating() const {
  return animator_ && animator_->is_animating();
}

void HoldingSpaceProgressIndicatorAnimation::AnimationProgressed(
    const gfx::Animation* animation) {
  UpdateAnimatableProperties(animation->GetCurrentValue());
  animation_updated_callback_list_.Notify();
}

void HoldingSpaceProgressIndicatorAnimation::AnimationEnded(
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
        base::BindOnce(&HoldingSpaceProgressIndicatorAnimation::StartInternal,
                       weak_factory_.GetWeakPtr(), /*is_cyclic_restart=*/true));
    return;
  }

  StartInternal(/*is_cyclic_restart=*/true);
}

void HoldingSpaceProgressIndicatorAnimation::StartInternal(
    bool is_cyclic_restart) {
  animator_ = std::make_unique<gfx::SlideAnimation>(this);
  animator_->SetSlideDuration(
      ui::ScopedAnimationDurationScaleMode::duration_multiplier() * duration_);
  animator_->SetTweenType(gfx::Tween::Type::LINEAR);

  if (!is_cyclic_restart)
    start_time_ = base::TimeTicks::Now();

  animator_->Show();
}

}  // namespace ash
