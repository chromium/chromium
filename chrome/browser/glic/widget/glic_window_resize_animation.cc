// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_window_resize_animation.h"

#include "base/task/sequenced_task_runner.h"
#include "build/buildflag.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_animator.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/common/chrome_features.h"
#include "ui/base/win/hwnd_metrics.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "ui/accelerated_widget_mac/ca_transaction_observer.h"
#endif

namespace glic {
namespace {

void RunCallbackList(std::unique_ptr<base::OnceClosureList> callbacks) {
  callbacks->Notify();
}

}  // namespace

GlicWindowResizeAnimation::GlicWindowResizeAnimation(
    base::WeakPtr<GlicWidget> widget,
    GlicWindowAnimator* window_animator,
    const gfx::Rect& target_bounds,
    base::TimeDelta duration,
    base::OnceClosure destruction_callback)
    : gfx::LinearAnimation(duration, kDefaultFrameRate, this),
      widget_(widget),
      glic_window_animator_(window_animator),
      initial_bounds_(widget->GetWindowBoundsInScreen()),
      new_bounds_(target_bounds),
      destruction_callbacks_(std::make_unique<base::OnceClosureList>()) {
  // Using AddUnsafe() because the callback list is run on a task posted on
  // destruction of `this`, so we aren't able to hold CallbackSubscriptions
  // here.
  destruction_callbacks_->AddUnsafe(std::move(destruction_callback));
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
  if (!destruction_callbacks_->empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&RunCallbackList, std::move(destruction_callbacks_)));
  }
}

void GlicWindowResizeAnimation::AnimateToState(double state) {
  if (!widget_) {
    return;
  }
  gfx::Rect bounds_to_animate = gfx::Tween::RectValueBetween(
      gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_SLOW_IN_3, state),
      initial_bounds_, new_bounds_);

  // The widget is detached, so make sure the bounds don't go out-of-screen.
  widget_->SetBoundsConstrained(bounds_to_animate);

  duration_left_ = (1 - GetCurrentValue()) * duration();
}

void GlicWindowResizeAnimation::AnimationEnded(const Animation* animation) {
  // Destroys `this`.
  glic_window_animator_->ResizeFinished();
}

void GlicWindowResizeAnimation::UpdateTargetBounds(
    const gfx::Rect& target_bounds,
    base::OnceClosure callback) {
  new_bounds_ = target_bounds;
  destruction_callbacks_->AddUnsafe(std::move(callback));
}

}  // namespace glic
