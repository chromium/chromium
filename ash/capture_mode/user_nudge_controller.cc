// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/user_nudge_controller.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/animation_sequence_block.h"

namespace ash {

namespace {

constexpr float kBaseRingOpacity = 0.21f;
constexpr float kRippleRingOpacity = 0.5f;

constexpr float kBaseRingScaleUpFactor = 1.1f;
constexpr float kRippleRingScaleUpFactor = 3.0f;
constexpr float kHighlightedViewScaleUpFactor = 1.2f;

constexpr base::TimeDelta kVisibilityChangeDuration = base::Milliseconds(200);
constexpr base::TimeDelta kScaleUpDuration = base::Milliseconds(500);
constexpr base::TimeDelta kScaleDownDelay = base::Milliseconds(650);
constexpr base::TimeDelta kScaleDownOffset = kScaleUpDuration + kScaleDownDelay;
constexpr base::TimeDelta kScaleDownDuration = base::Milliseconds(1350);
constexpr base::TimeDelta kRippleAnimationDuration = base::Milliseconds(2000);

constexpr base::TimeDelta kDelayToShowNudge = base::Milliseconds(1000);
constexpr base::TimeDelta kDelayToRepeatNudge = base::Milliseconds(2500);

// Returns the given `view`'s layer bounds in root coordinates ignoring any
// transforms it or any of its ancestors may have.
gfx::Rect GetViewLayerBoundsInRootNoTransform(views::View* view) {
  auto* layer = view->layer();
  DCHECK(layer);
  gfx::Point origin;
  while (layer) {
    const auto layer_origin = layer->bounds().origin();
    origin.Offset(layer_origin.x(), layer_origin.y());
    layer = layer->parent();
  }
  return gfx::Rect(origin, view->layer()->size());
}

}  // namespace

UserNudgeController::UserNudgeController(CaptureModeSession* session,
                                         views::View* view_to_be_highlighted)
    : capture_session_(session),
      view_to_be_highlighted_(view_to_be_highlighted) {
  view_to_be_highlighted_->SetPaintToLayer();
  view_to_be_highlighted_->layer()->SetFillsBoundsOpaquely(false);

  // Rings are created initially with 0 opacity. Calling SetVisible() will
  // animate them towards their correct state.
  const SkColor ring_color =
      DarkLightModeControllerImpl::Get()->IsDarkModeEnabled() ? SK_ColorWHITE
                                                              : SK_ColorBLACK;
  base_ring_.SetColor(ring_color);
  base_ring_.SetFillsBoundsOpaquely(false);
  base_ring_.SetOpacity(0);
  ripple_ring_.SetColor(ring_color);
  ripple_ring_.SetFillsBoundsOpaquely(false);
  ripple_ring_.SetOpacity(0);

  Reposition();
}

UserNudgeController::~UserNudgeController() {
  if (should_dismiss_nudge_forever_)
    CaptureModeController::Get()->DisableUserNudgeForever();
  capture_session_->capture_toast_controller()->MaybeDismissCaptureToast(
      CaptureToastType::kUserNudge,
      /*animate=*/false);
}

void UserNudgeController::Reposition() {
  auto* parent_window = GetParentWindow();

  auto* parent_layer = parent_window->layer();
  if (parent_layer != base_ring_.parent()) {
    parent_layer->Add(&base_ring_);
    parent_layer->Add(&ripple_ring_);
  }

  const auto view_bounds_in_root =
      GetViewLayerBoundsInRootNoTransform(view_to_be_highlighted_);
  base_ring_.SetBounds(view_bounds_in_root);
  base_ring_.SetRoundedCornerRadius(
      gfx::RoundedCornersF(view_bounds_in_root.width() / 2.f));
  ripple_ring_.SetBounds(view_bounds_in_root);
  ripple_ring_.SetRoundedCornerRadius(
      gfx::RoundedCornersF(view_bounds_in_root.width() / 2.f));
}

void UserNudgeController::SetVisible(bool visible) {
  if (is_visible_ == visible)
    return;

  is_visible_ = visible;
  auto* capture_toast_controller = capture_session_->capture_toast_controller();

  views::AnimationBuilder builder;
  builder.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  if (!is_visible_) {
    // We should no longer repeat the nudge animation.
    timer_.Stop();
    // We should also stop any ongoing animation on the `base_ring_` in
    // particular since we observe this animation ending to schedule a repeat.
    // See OnBaseRingAnimationEnded().
    base_ring_.GetAnimator()->AbortAllAnimations();

    // Animate all animation layers and the toast widget to 0 opacity.
    builder.Once()
        .SetDuration(kVisibilityChangeDuration)
        .SetOpacity(&base_ring_, 0, gfx::Tween::FAST_OUT_SLOW_IN)
        .SetOpacity(&ripple_ring_, 0, gfx::Tween::FAST_OUT_SLOW_IN);
    capture_toast_controller->MaybeDismissCaptureToast(
        CaptureToastType::kUserNudge);
    return;
  }

  // Animate the `base_ring_` and the `toast_widget_` to their default shown
  // opacity. Note that we don't need to show the `ripple_ring_` since it only
  // shows as the nudge animation is being performed.
  // Once those elements reach their default shown opacity, we perform the nudge
  // animation.
  builder
      .OnEnded(base::BindOnce(&UserNudgeController::PerformNudgeAnimations,
                              weak_ptr_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(kDelayToShowNudge)
      .SetOpacity(&base_ring_, kBaseRingOpacity, gfx::Tween::FAST_OUT_SLOW_IN);
  capture_toast_controller->ShowCaptureToast(CaptureToastType::kUserNudge);
}

void UserNudgeController::PerformNudgeAnimations() {
  PerformBaseRingAnimation();
  PerformRippleRingAnimation();
  PerformViewScaleAnimation();
}

void UserNudgeController::PerformBaseRingAnimation() {
  // The `base_ring_` should scale up around the center of the
  // `view_to_be_highlighted_` to grab the user's attention, and then scales
  // back down to its original size.
  const gfx::Transform scale_up_transform =
      capture_mode_util::GetScaleTransformAboutCenter(&base_ring_,
                                                      kBaseRingScaleUpFactor);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&UserNudgeController::OnBaseRingAnimationEnded,
                              base::Unretained(this)))
      .Once()
      .SetDuration(kScaleUpDuration)
      .SetTransform(&base_ring_, scale_up_transform,
                    gfx::Tween::ACCEL_40_DECEL_20)
      .Offset(kScaleDownOffset)
      .SetDuration(kScaleDownDuration)
      .SetTransform(&base_ring_, gfx::Transform(),
                    gfx::Tween::FAST_OUT_SLOW_IN_3);
}

void UserNudgeController::PerformRippleRingAnimation() {
  // The ripple scales up to 3x the size of the `view_to_be_highlighted_` and
  // around its center while fading out.
  ripple_ring_.SetOpacity(kRippleRingOpacity);
  ripple_ring_.SetTransform(gfx::Transform());
  const gfx::Transform scale_up_transform =
      capture_mode_util::GetScaleTransformAboutCenter(&ripple_ring_,
                                                      kRippleRingScaleUpFactor);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kRippleAnimationDuration)
      .SetOpacity(&ripple_ring_, 0, gfx::Tween::ACCEL_0_80_DECEL_80)
      .SetTransform(&ripple_ring_, scale_up_transform,
                    gfx::Tween::ACCEL_0_40_DECEL_100);
}

void UserNudgeController::PerformViewScaleAnimation() {
  // The `view_to_be_highlighted_` scales up and down around its own center in
  // a similar fashion to that of the `base_ring_`.
  auto* view_layer = view_to_be_highlighted_->layer();
  const gfx::Transform scale_up_transform =
      capture_mode_util::GetScaleTransformAboutCenter(
          view_layer, kHighlightedViewScaleUpFactor);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kScaleUpDuration)
      .SetTransform(view_layer, scale_up_transform,
                    gfx::Tween::ACCEL_40_DECEL_20)
      .Offset(kScaleDownOffset)
      .SetDuration(kScaleDownDuration)
      .SetTransform(view_layer, gfx::Transform(),
                    gfx::Tween::FAST_OUT_SLOW_IN_3);
}

void UserNudgeController::OnBaseRingAnimationEnded() {
  timer_.Start(FROM_HERE, kDelayToRepeatNudge,
               base::BindOnce(&UserNudgeController::PerformNudgeAnimations,
                              weak_ptr_factory_.GetWeakPtr()));
}

aura::Window* UserNudgeController::GetParentWindow() const {
  auto* root_window =
      view_to_be_highlighted_->GetWidget()->GetNativeWindow()->GetRootWindow();
  DCHECK(root_window);
  return root_window->GetChildById(kShellWindowId_OverlayContainer);
}

}  // namespace ash
