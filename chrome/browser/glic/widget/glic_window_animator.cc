// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_window_animator.h"

#include "base/functional/callback.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_resize_animation.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/webview/webview.h"

namespace glic {

GlicWindowAnimator::GlicWindowAnimator(
    base::WeakPtr<GlicWidget> widget,
    base::RepeatingClosure resize_finished_callback)
    : widget_(widget),
      resize_finished_callback_(std::move(resize_finished_callback)) {}

GlicWindowAnimator::~GlicWindowAnimator() = default;

void GlicWindowAnimator::AnimateBounds(const gfx::Rect& target_bounds,
                                       base::TimeDelta duration,
                                       base::OnceClosure callback) {
  if (!widget_) {
    return;
  }

  if (duration < base::Milliseconds(0)) {
    duration = base::Milliseconds(0);
  }

  if (duration > base::Seconds(60)) {
    duration = base::Seconds(60);
  }

  gfx::Rect widget_target_bounds =
      widget_->VisibleToWidgetBounds(target_bounds);

  if (window_resize_animation_) {
    // Update the ongoing animation with the new bounds and new duration.
    window_resize_animation_->UpdateTargetBounds(widget_target_bounds,
                                                 std::move(callback));
    window_resize_animation_->SetDuration(
        std::max(window_resize_animation_->duration_left(), duration));
  } else {
    window_resize_animation_ = std::make_unique<GlicWindowResizeAnimation>(
        widget_, this, widget_target_bounds, duration, std::move(callback));
  }
}

void GlicWindowAnimator::AnimateSize(const gfx::Size& target_size,
                                     base::TimeDelta duration,
                                     base::OnceClosure callback) {
  if (!widget_) {
    return;
  }
  last_target_size_ = target_size;
  gfx::Rect target_bounds = GetCurrentTargetBounds();
  target_bounds.set_size(target_size);
  AnimateBounds(target_bounds, duration, std::move(callback));
}

void GlicWindowAnimator::AnimatePosition(const gfx::Point& target_position,
                                         base::TimeDelta duration,
                                         base::OnceClosure callback) {
  if (!widget_) {
    return;
  }
  // Maintain the size whether there's an ongoing animation or not.
  gfx::Rect new_bounds = GetCurrentTargetBounds();
  new_bounds.set_origin(target_position);
  AnimateBounds(new_bounds, duration, std::move(callback));
}

gfx::Rect GlicWindowAnimator::GetCurrentTargetBounds() {
  if (!widget_) {
    return gfx::Rect();
  }
  if (window_resize_animation_) {
    // Get the ongoing animation's target bounds if they exist.
    return widget_->WidgetToVisibleBounds(
        window_resize_animation_->target_bounds());
  } else {
    return widget_->WidgetToVisibleBounds(widget_->GetWindowBoundsInScreen());
  }
}

bool GlicWindowAnimator::IsAnimating() const {
  return window_resize_animation_ != nullptr;
}

void GlicWindowAnimator::ResetLastTargetSize() {
  last_target_size_ = gfx::Size();
}

void GlicWindowAnimator::MaybeAnimateToTargetSize() {
  if (!widget_) {
    return;
  }
  if (!last_target_size_.IsEmpty() &&
      last_target_size_ != widget_->GetWindowBoundsInScreen().size()) {
    AnimateSize(last_target_size_, base::Milliseconds(300), base::DoNothing());
  }
  ResetLastTargetSize();
}

void GlicWindowAnimator::CancelAnimation() {
  window_resize_animation_.reset();
}

void GlicWindowAnimator::ResizeFinished() {
  if (!widget_) {
    return;
  }
  // Destroy window_resize_animation_.
  window_resize_animation_.reset();
  resize_finished_callback_.Run();
}

}  // namespace glic
