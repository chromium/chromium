// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/progress_indicator/progress_indicator_animation.h"

#include "base/task/sequenced_task_runner.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/slide_animation.h"

namespace ash {

ProgressIndicatorAnimation::ProgressIndicatorAnimation(base::TimeDelta duration,
                                                       bool is_cyclic)
    : duration_(duration), is_cyclic_(is_cyclic) {}

ProgressIndicatorAnimation::~ProgressIndicatorAnimation() = default;

base::CallbackListSubscription
ProgressIndicatorAnimation::AddAnimationUpdatedCallback(
    base::RepeatingClosureList::CallbackType callback) {
  return animation_updated_callback_list_.Add(std::move(callback));
}

void ProgressIndicatorAnimation::AddUnsafeAnimationUpdatedCallback(
    base::RepeatingClosureList::CallbackType callback) {
  animation_updated_callback_list_.AddUnsafe(std::move(callback));
}

void ProgressIndicatorAnimation::Start() {
  StartInternal(/*is_cyclic_restart=*/false);
}

bool ProgressIndicatorAnimation::HasAnimated() const {
  return animator_ != nullptr;
}

bool ProgressIndicatorAnimation::IsAnimating() const {
  if (animator_ && animator_->is_animating()) {
    return true;
  }

  // Cyclic animations, such as indeterminate ring animations, repeat forever
  // and never finish from the user's perspective. Therefore, this function
  // returns true if a cyclic animation has started.
  return is_cyclic_ && !start_time_.is_null();
}

void ProgressIndicatorAnimation::Init() {
  UpdateAnimatableProperties(/*fraction=*/0.f);
}

void ProgressIndicatorAnimation::AnimationProgressed(
    const gfx::Animation* animation) {
  UpdateAnimatableProperties(animation->GetCurrentValue());
  animation_updated_callback_list_.Notify();
}

void ProgressIndicatorAnimation::AnimationEnded(
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
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProgressIndicatorAnimation::StartInternal,
                       weak_factory_.GetWeakPtr(), /*is_cyclic_restart=*/true));
    return;
  }

  StartInternal(/*is_cyclic_restart=*/true);
}

void ProgressIndicatorAnimation::StartInternal(bool is_cyclic_restart) {
  animator_ = std::make_unique<gfx::SlideAnimation>(this);
  animator_->SetSlideDuration(
      ui::ScopedAnimationDurationScaleMode::duration_multiplier() * duration_);
  animator_->SetTweenType(gfx::Tween::Type::LINEAR);

  if (!is_cyclic_restart)
    start_time_ = base::TimeTicks::Now();

  animator_->Show();
}

}  // namespace ash
