// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/wm_fling_handler.h"

#include "ui/aura/window.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/events/gestures/fling_curve.h"

namespace ash {

namespace {

// The amount the fling curve's offsets are scaled down.
constexpr float kFlingScaleDown = 3.f;

}  // namespace

WmFlingHandler::WmFlingHandler(const gfx::Vector2dF& initial_velocity,
                               const aura::Window* root_window,
                               StepCallback on_step_callback,
                               base::RepeatingClosure on_end_callback)
    : fling_velocity_(initial_velocity),
      on_step_callback_(std::move(on_step_callback)),
      on_end_callback_(std::move(on_end_callback)) {
  DCHECK(!on_step_callback_.is_null());
  DCHECK(!on_end_callback_.is_null());

  fling_curve_ =
      std::make_unique<ui::FlingCurve>(fling_velocity_, base::TimeTicks::Now());
  observed_compositor_ =
      const_cast<ui::Compositor*>(root_window->layer()->GetCompositor());
  observed_compositor_->AddAnimationObserver(this);
}

WmFlingHandler::~WmFlingHandler() {
  EndFling();
}

void WmFlingHandler::OnAnimationStep(base::TimeTicks timestamp) {
  DCHECK(observed_compositor_);

  gfx::Vector2dF offset;
  bool continue_fling =
      fling_curve_->ComputeScrollOffset(timestamp, &offset, &fling_velocity_);
  offset.Scale(1 / kFlingScaleDown);

  // The below `on_step_callback_` (which is bound to
  // `WindowCycleView::OnFlingStep()`) will trigger layout, which in turn can
  // trigger an `OnFlingEnd()` leading to the destruction of `this` while still
  // in the middle of this function. Here we use a `WeakPtr` to detect this and
  // early exit and skip the rest this function to avoid a UAF.
  // https://crbug.com/1350558.
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();

  // Note that order matters here. We want to stop flinging if the API for fling
  // says to finish or if the user of this class wants to stop. Notify the user
  // even if the API says to stop flinging as it still produces an usable
  // `offset`, but end the fling afterwards.
  continue_fling = on_step_callback_.Run(
                       fling_last_offset_ ? offset.x() - fling_last_offset_->x()
                                          : offset.x()) &&
                   continue_fling;

  if (!weak_ptr)
    return;

  fling_last_offset_ = std::make_optional(offset);

  if (!continue_fling)
    EndFling();
}

void WmFlingHandler::OnCompositingShuttingDown(ui::Compositor* compositor) {
  DCHECK_EQ(observed_compositor_, compositor);
  EndFling();
}

void WmFlingHandler::EndFling() {
  if (!observed_compositor_)
    return;

  observed_compositor_->RemoveAnimationObserver(this);
  observed_compositor_ = nullptr;
  fling_curve_.reset();
  fling_last_offset_ = std::nullopt;

  on_end_callback_.Run();
}

}  // namespace ash
