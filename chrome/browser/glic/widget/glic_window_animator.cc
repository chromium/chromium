// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_window_animator.h"

#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/glic/widget/glic_window_resize_animation.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/webview/webview.h"

namespace glic {

namespace {

constexpr static int kResizeAnimationDurationMs = 300;
constexpr static int kAttachedWidgetOpacityDurationMs = 150;
constexpr static int kDetachedWidgetOpacityDurationMs = 100;

}  // namespace

class GlicWindowAnimator::GlicWindowOpacityAnimation
    : public gfx::LinearAnimation {
 public:
  GlicWindowOpacityAnimation(GlicWindowAnimator* window_animator,
                             GlicWindowController* window_controller,
                             base::TimeDelta duration,
                             float start_opacity,
                             float target_opacity)
      : gfx::LinearAnimation(duration, kDefaultFrameRate, window_animator),
        window_animator_(window_animator),
        window_controller_(window_controller),
        start_opacity_(start_opacity),
        target_opacity_(target_opacity) {}

  GlicWindowOpacityAnimation(const GlicWindowOpacityAnimation&) = delete;
  GlicWindowOpacityAnimation& operator=(const GlicWindowOpacityAnimation&) =
      delete;
  ~GlicWindowOpacityAnimation() override = default;

  // gfx::LinearAnimation:
  void AnimateToState(double state) override {
    window_controller_->GetGlicWidget()->SetOpacity(
        gfx::Tween::FloatValueBetween(GetCurrentValue(), start_opacity_,
                                      target_opacity_));
  }

  // gfx::LinearAnimation:
  void AnimationEnded(const Animation* animation) {
    // Destroys `this`.
    window_animator_->OnWindowOpacityAnimationEnded();
  }

 private:
  raw_ptr<GlicWindowAnimator> window_animator_;
  raw_ptr<GlicWindowController> window_controller_;
  const float start_opacity_;
  const float target_opacity_;
};

class GlicWindowAnimator::GlicViewOpacityAnimation {
 public:
  GlicViewOpacityAnimation(GlicWindowAnimator* window_animator,
                           GlicWindowController* window_controller)
      : window_animator_(window_animator),
        window_controller_(window_controller) {}

  GlicViewOpacityAnimation(const GlicViewOpacityAnimation&) = delete;
  GlicViewOpacityAnimation& operator=(const GlicViewOpacityAnimation&) = delete;
  ~GlicViewOpacityAnimation() = default;

  void StartFade(base::TimeDelta duration,
                 float start_opacity,
                 float target_opacity) {
    views::WebView* web_view = window_controller_->GetGlicView()->web_view();
    window_animator_->SetGlicWebViewVisibility(true);
    if (!web_view->layer()) {
      web_view->SetPaintToLayer();
    }
    web_view->layer()->SetOpacity(start_opacity);

    views::AnimationBuilder()
        .SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .OnEnded(base::BindOnce(&GlicViewOpacityAnimation::AnimationEnded,
                                base::Unretained(this)))
        .Once()
        .SetDuration(duration)
        .SetOpacity(web_view->layer(), target_opacity);
  }

  void AnimationEnded() {
    // Destroys `this`.
    window_animator_->OnViewOpacityAnimationEnded();
  }

 private:
  raw_ptr<GlicWindowAnimator> window_animator_;
  raw_ptr<GlicWindowController> window_controller_;
};

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

  // Fade in widget while resizing out.
  AnimateWindowOpacity(0.0f, 1.0f,
                       base::Milliseconds(kAttachedWidgetOpacityDurationMs));
  AnimateBounds(target_bounds, base::Milliseconds(kResizeAnimationDurationMs),
                std::move(callback));
}

void GlicWindowAnimator::RunOpenDetachedAnimation(base::OnceClosure callback,
                                                  int animate_down_distance) {
  gfx::Rect target_bounds =
      window_controller_->GetGlicWidget()->GetWindowBoundsInScreen();
  // Only set the detached Y position if there isn't a browser.
  target_bounds.set_y(target_bounds.y() + animate_down_distance);

  // Fade in widget while animating down.
  AnimateWindowOpacity(0.0f, 1.0f,
                       base::Milliseconds(kDetachedWidgetOpacityDurationMs));
  AnimateBounds(target_bounds, base::Milliseconds(kResizeAnimationDurationMs),
                std::move(callback));
}

void GlicWindowAnimator::RunCloseAnimation(GlicButton* glic_button,
                                           base::OnceClosure callback) {
  // The widget is going away so it's fine to replace any existing animation.
  AnimateBounds(glic_button->GetBoundsWithInset(),
                base::Milliseconds(kResizeAnimationDurationMs),
                std::move(callback));
}

void GlicWindowAnimator::FadeInWebView() {
  AnimateViewOpacity(0.0f, 1.0f,
                     base::Milliseconds(kAttachedWidgetOpacityDurationMs));
}

void GlicWindowAnimator::AnimateViewOpacity(float start_opacity,
                                            float target_opacity,
                                            base::TimeDelta duration) {
  CHECK(window_controller_->GetGlicView());

  // Ensure that GlicView is visible before running its opacity animation.
  window_controller_->GetGlicView()->SetVisible(true);
  glic_view_opacity_animation_ =
      std::make_unique<GlicViewOpacityAnimation>(this, window_controller_);
  glic_view_opacity_animation_->StartFade(duration, start_opacity,
                                          target_opacity);
}

void GlicWindowAnimator::AnimateWindowOpacity(float start_opacity,
                                              float target_opacity,
                                              base::TimeDelta duration) {
  CHECK(window_controller_->GetGlicWidget());

  window_controller_->GetGlicWidget()->SetOpacity(start_opacity);
  glic_window_opacity_animation_ = std::make_unique<GlicWindowOpacityAnimation>(
      this, window_controller_, duration, start_opacity, target_opacity);
  glic_window_opacity_animation_->Start();
}

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

  last_target_size_ = target_size;
  // Maintain the top-right corner whether there's an ongoing animation or not.
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
  if (window_resize_animation_) {
    // Get the ongoing animation's target bounds if they exist.
    return window_resize_animation_->target_bounds();
  } else {
    return window_controller_->GetGlicWidget()->GetWindowBoundsInScreen();
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

void GlicWindowAnimator::SetGlicWebViewVisibility(bool is_visible) {
  views::WebView* web_view = window_controller_->GetGlicView()->web_view();
  if (web_view->GetVisible() != is_visible) {
    web_view->SetVisible(is_visible);
  }
}

void GlicWindowAnimator::ResizeFinished() {
  // Destroy window_resize_animation_.
  window_resize_animation_.reset();
}

void GlicWindowAnimator::OnWindowOpacityAnimationEnded() {
  // Destroy glic_window_opacity_animation_.
  glic_window_opacity_animation_.reset();
}

void GlicWindowAnimator::OnViewOpacityAnimationEnded() {
  // Destroy glic_view_opacity_animation_.
  glic_view_opacity_animation_.reset();
}

}  // namespace glic
