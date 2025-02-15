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

void GlicWindowAnimator::CreateNewAnimationAndStart(
    gfx::Rect target_bounds,
    base::TimeDelta duration,
    base::OnceClosure callback) {
  window_resize_animation_ = std::make_unique<GlicWindowResizeAnimation>(
      window_controller_, this, target_bounds, duration, std::move(callback));
}

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

  if (window_resize_animation_) {
    // TODO(394686499): Do something more graceful than jumping to the end.
    // This can cause re-entrancy, which can be problematic. If there are bugs
    // check here first.
    window_resize_animation_->End();
  }

  if (duration < base::Milliseconds(0)) {
    duration = base::Milliseconds(0);
  }

  CreateNewAnimationAndStart(target_bounds, duration, std::move(callback));
}

void GlicWindowAnimator::AnimateSize(const gfx::Size& target_size,
                                     base::TimeDelta duration,
                                     base::OnceClosure callback) {
  if (window_resize_animation_) {
    // TODO(394686499): refine how running bounds change animations are updated.
    // Moves the top-right corner, if we update size we must also update
    // position.
    window_resize_animation_->UpdateTargetSize(target_size,
                                               std::move(callback));
  } else {
    // Maintain the top-right corner.
    gfx::Rect current_bounds =
        window_controller_->GetGlicWidget()->GetWindowBoundsInScreen();
    int original_top_right = current_bounds.x() + current_bounds.width();
    current_bounds.set_size(target_size);
    current_bounds.set_x(original_top_right - target_size.width());
    AnimateBounds(current_bounds, duration, std::move(callback));
  }
}

void GlicWindowAnimator::AnimatePosition(const gfx::Point& target_position,
                                         base::TimeDelta duration,
                                         base::OnceClosure callback) {
  if (window_resize_animation_) {
    // TODO(394686499): Refine how running bounds change animations are updated.
    window_resize_animation_->UpdateTargetPosition(target_position,
                                                   std::move(callback));
  } else {
    // Maintain the size.
    gfx::Rect new_bounds =
        window_controller_->GetGlicWidget()->GetWindowBoundsInScreen();
    new_bounds.set_origin(target_position);
    AnimateBounds(new_bounds, duration, std::move(callback));
  }
}

void GlicWindowAnimator::ResizeFinished() {
  // Destroy window_resize_animation_.
  window_resize_animation_.reset();
}

}  // namespace glic
