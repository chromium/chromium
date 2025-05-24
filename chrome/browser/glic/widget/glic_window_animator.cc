// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_window_animator.h"

#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/glic/widget/glic_window_resize_animation.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/webview/webview.h"

namespace glic {

GlicWindowAnimator::GlicWindowAnimator(GlicWindowController* window_controller)
    : window_controller_(window_controller) {}

GlicWindowAnimator::~GlicWindowAnimator() = default;

void GlicWindowAnimator::AnimateBounds(const gfx::Rect& target_bounds,
                                       base::TimeDelta duration,
                                       base::OnceClosure callback) {
  CHECK(window_controller_->GetGlicWidget());

  if (duration < base::Milliseconds(0)) {
    duration = base::Milliseconds(0);
  }

  if (duration > base::Seconds(60)) {
    duration = base::Seconds(60);
  }

  auto* glic_widget = window_controller_->GetGlicWidget();
  gfx::Rect widget_target_bounds =
      glic_widget->VisibleToWidgetBounds(target_bounds);

  if (window_resize_animation_) {
    // Update the ongoing animation with the new bounds and new duration.
    window_resize_animation_->UpdateTargetBounds(widget_target_bounds,
                                                 std::move(callback));
    window_resize_animation_->SetDuration(
        std::max(window_resize_animation_->duration_left(), duration));
  } else {
    window_resize_animation_ = std::make_unique<GlicWindowResizeAnimation>(
        window_controller_, this, widget_target_bounds, duration,
        std::move(callback));
  }
}

void GlicWindowAnimator::AnimateSize(const gfx::Size& target_size,
                                     base::TimeDelta duration,
                                     base::OnceClosure callback) {
  last_target_size_ = target_size;
  gfx::Rect target_bounds = GetCurrentTargetBounds();
  target_bounds.set_size(target_size);
  AnimateBounds(target_bounds, duration, std::move(callback));
}

void GlicWindowAnimator::AnimatePosition(const gfx::Point& target_position,
                                         base::TimeDelta duration,
                                         base::OnceClosure callback) {
  // Maintain the size whether there's an ongoing animation or not.
  gfx::Rect new_bounds = GetCurrentTargetBounds();
  new_bounds.set_origin(target_position);
  AnimateBounds(new_bounds, duration, std::move(callback));
}

gfx::Rect GlicWindowAnimator::GetCurrentTargetBounds() {
  auto* glic_widget = window_controller_->GetGlicWidget();
  if (window_resize_animation_) {
    // Get the ongoing animation's target bounds if they exist.
    return glic_widget->WidgetToVisibleBounds(
        window_resize_animation_->target_bounds());
  } else {
    return glic_widget->WidgetToVisibleBounds(
        glic_widget->GetWindowBoundsInScreen());
  }
}

void GlicWindowAnimator::ResetLastTargetSize() {
  last_target_size_ = gfx::Size();
}

void GlicWindowAnimator::MaybeAnimateToTargetSize() {
  if (!last_target_size_.IsEmpty() &&
      last_target_size_ != window_controller_->GetGlicWidget()
                               ->GetWindowBoundsInScreen()
                               .size()) {
    AnimateSize(last_target_size_, base::Milliseconds(300), base::DoNothing());
  }
  ResetLastTargetSize();
}

void GlicWindowAnimator::ResizeFinished() {
  // Destroy window_resize_animation_.
  window_resize_animation_.reset();
}

}  // namespace glic
