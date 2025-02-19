// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_window_animator.h"

#include "chrome/browser/glic/glic_view.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/glic/glic_window_resize_animation.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/background.h"

namespace glic {

namespace {

constexpr static int kAnimationDurationMs = 300;
constexpr static SkColor kDefaultBackgroundColor =
    SkColorSetARGB(255, 27, 28, 29);
constexpr static int kCornerRadius = 12;
constexpr static int kInitialDetachedYPosition = 48;

}  // namespace

GlicWindowAnimator::GlicWindowAnimator(GlicWindowController* window_controller)
    : window_controller_(window_controller) {}

GlicWindowAnimator::~GlicWindowAnimator() = default;

void GlicWindowAnimator::RunOpenAttachedAnimation(GlicButton* glic_button,
                                                  const gfx::Size& target_size,
                                                  base::OnceClosure callback) {
  CHECK(window_controller_->GetGlicWidget());
  gfx::Rect target_bounds =
      window_controller_->GetGlicWidget()->GetWindowBoundsInScreen();
  int top_left_x =
      glic_button->GetBoundsWithInset().top_right().x() - target_size.width();
  target_bounds.set_x(top_left_x);
  target_bounds.set_width(target_size.width());
  target_bounds.set_height(target_size.height());

  // TODO(crbug.com/389982576): Match the background color of the widget with
  // the web client background.
  window_controller_->GetGlicView()->SetBackground(
      views::CreateRoundedRectBackground(kDefaultBackgroundColor,
                                         kCornerRadius));

  AnimateBounds(target_bounds, base::Milliseconds(kAnimationDurationMs),
                std::move(callback));
}

void GlicWindowAnimator::RunOpenDetachedAnimation(base::OnceClosure callback) {
  gfx::Rect target_bounds =
      window_controller_->GetGlicWidget()->GetWindowBoundsInScreen();
  target_bounds.set_y(target_bounds.y() + kInitialDetachedYPosition);

  // TODO(crbug.com/389982576): Match the background color of the widget with
  // the web client background.
  window_controller_->GetGlicView()->SetBackground(
      views::CreateRoundedRectBackground(kDefaultBackgroundColor,
                                         kCornerRadius));

  AnimateBounds(target_bounds, base::Milliseconds(kAnimationDurationMs),
                std::move(callback));
}

void GlicWindowAnimator::AnimateBounds(const gfx::Rect& target_bounds,
                                       base::TimeDelta duration,
                                       base::OnceClosure callback) {
  CHECK(window_controller_->GetGlicWidget());

  if (duration < base::Milliseconds(0)) {
    duration = base::Milliseconds(0);
  }

  if (window_resize_animation_) {
    // Update the ongoing animation with the new bounds and new duration.
    window_resize_animation_->UpdateTargetBounds(target_bounds,
                                                 std::move(callback));
    window_resize_animation_->SetDuration(
        std::max(window_resize_animation_->duration_left(), duration));
  } else {
    window_resize_animation_ = std::make_unique<GlicWindowResizeAnimation>(
        window_controller_, this, target_bounds, duration, std::move(callback));
  }
}

void GlicWindowAnimator::AnimateSize(const gfx::Size& target_size,
                                     base::TimeDelta duration,
                                     base::OnceClosure callback) {
  // Maintain the top-right corner whether there's an ongoing animation or not.
  gfx::Rect target_bounds = GetCurrentTargetBounds();
  int original_right = target_bounds.right();
  target_bounds.set_size(target_size);
  target_bounds.set_x(original_right - target_size.width());
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
  if (window_resize_animation_) {
    // Get the ongoing animation's target bounds if they exist.
    return window_resize_animation_->target_bounds();
  } else {
    return window_controller_->GetGlicWidget()->GetWindowBoundsInScreen();
  }
}

void GlicWindowAnimator::ResizeFinished() {
  // Destroy window_resize_animation_.
  window_resize_animation_.reset();
}

}  // namespace glic
