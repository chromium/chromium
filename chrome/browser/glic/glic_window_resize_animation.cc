// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_window_resize_animation.h"

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/glic/glic_view.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "ui/accelerated_widget_mac/ca_transaction_observer.h"
#endif

namespace glic {

GlicWindowResizeAnimation::GlicWindowResizeAnimation(
    GlicWindowController* window_controller,
    const gfx::Rect& target_bounds,
    base::TimeDelta duration,
    DestructionCallback destruction_callback)
    : gfx::LinearAnimation(duration, kDefaultFrameRate, this),
      window_controller_(window_controller),
      initial_bounds_(
          window_controller_->GetGlicWidget()->GetWindowBoundsInScreen()),
      new_bounds_(target_bounds),
      destruction_callback_(std::move(destruction_callback)) {
  // TODO(crbug.com/389238233): CompositorAnimationRunner does not appear to
  // be fully functional.
  // Use a CompositorAnimationRunner for smoother vsync driven resize animation.
  // auto container = base::MakeRefCounted<gfx::AnimationContainer>();
  // container->SetAnimationRunner(
  //     std::make_unique<views::CompositorAnimationRunner>(widget));
  // SetContainer(container.get());

  Start();
}

GlicWindowResizeAnimation::~GlicWindowResizeAnimation() {
  if (destruction_callback_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(destruction_callback_));
  }
}

void GlicWindowResizeAnimation::AnimateToState(double state) {
  window_controller_->GetGlicWidget()->SetBounds(gfx::Tween::RectValueBetween(
      gfx::Tween::CalculateValue(gfx::Tween::EASE_IN_OUT_EMPHASIZED, state),
      initial_bounds_, new_bounds_));
}

void GlicWindowResizeAnimation::AnimationEnded(const Animation* animation) {
  // Destroys `this`.
  window_controller_->ResizeFinished();
}

}  // namespace glic
