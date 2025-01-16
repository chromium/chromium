// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_window_resize_animation.h"

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/glic/glic_view.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/animation/compositor_animation_runner.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "ui/accelerated_widget_mac/ca_transaction_observer.h"
#endif

namespace glic {

GlicWindowResizeAnimation::GlicWindowResizeAnimation(
    views::Widget* widget,
    gfx::Size new_size,
    base::TimeDelta duration,
    FinishedCallback finished_callback)
    : gfx::LinearAnimation(duration, kDefaultFrameRate, this),
      widget_(widget),
      initial_size_(widget->GetWindowBoundsInScreen().size()),
      new_size_(new_size),
      finished_callback_(std::move(finished_callback)) {
  // Use a CompositorAnimationRunner for smoother vsync driven resize animation.
  auto container = base::MakeRefCounted<gfx::AnimationContainer>();
  container->SetAnimationRunner(
      std::make_unique<views::CompositorAnimationRunner>(widget));
  SetContainer(container.get());

  Start();
}

GlicWindowResizeAnimation::~GlicWindowResizeAnimation() = default;

void GlicWindowResizeAnimation::AnimateToState(double state) {
#if BUILDFLAG(IS_MAC)
  // This ensures that the web contents resize occurs in sync with the window
  // resize.
  ui::CATransactionCoordinator::Get().Synchronize();
#endif

  widget_->SetSize(gfx::Tween::SizeValueBetween(
      gfx::Tween::CalculateValue(gfx::Tween::EASE_IN_OUT_EMPHASIZED, state),
      initial_size_, new_size_));
}

void GlicWindowResizeAnimation::AnimationEnded(const Animation* animation) {
  // Destroys `this`.
  std::move(finished_callback_).Run();
}

}  // namespace glic
