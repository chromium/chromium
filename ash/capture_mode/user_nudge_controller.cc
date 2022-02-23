// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/user_nudge_controller.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/animation_sequence_block.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

constexpr int kToastSpacingFromBar = 8;
constexpr int kToastDefaultHeight = 36;
constexpr int kToastVerticalPadding = 8;
constexpr int kToastHorizontalPadding = 16;
constexpr int kToastBorderThickness = 1;
constexpr int kToastCornerRadius = 16;
constexpr gfx::RoundedCornersF kToastRoundedCorners{kToastCornerRadius};

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

// Returns the local center point of the given `layer`.
gfx::Point GetLocalCenterPoint(ui::Layer* layer) {
  return gfx::Rect(layer->size()).CenterPoint();
}

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

// Returns the init params that will be used for the toast widget.
views::Widget::InitParams CreateWidgetParams(aura::Window* parent,
                                             const gfx::Rect& bounds) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = parent;
  params.bounds = bounds;
  params.name = "UserNudgeToastWidget";
  params.accept_events = false;
  return params;
}

}  // namespace

UserNudgeController::UserNudgeController(views::View* view_to_be_highlighted)
    : view_to_be_highlighted_(view_to_be_highlighted) {
  view_to_be_highlighted_->SetPaintToLayer();
  view_to_be_highlighted_->layer()->SetFillsBoundsOpaquely(false);

  // Rings are created initially with 0 opacity. Calling SetVisible() will
  // animate them towards their correct state.
  base_ring_.SetColor(SK_ColorWHITE);
  base_ring_.SetFillsBoundsOpaquely(false);
  base_ring_.SetOpacity(0);
  ripple_ring_.SetColor(SK_ColorWHITE);
  ripple_ring_.SetFillsBoundsOpaquely(false);
  ripple_ring_.SetOpacity(0);

  BuildToastWidget();
  Reposition();
}

UserNudgeController::~UserNudgeController() {
  DCHECK(toast_widget_);
  toast_widget_->CloseNow();
  if (should_dismiss_nudge_forever_)
    CaptureModeController::Get()->DisableUserNudgeForever();
}

void UserNudgeController::Reposition() {
  auto* parent_window = GetParentWindow();
  if (toast_widget_->GetNativeWindow()->parent() != parent_window)
    parent_window->AddChild(toast_widget_->GetNativeWindow());

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
  toast_widget_->SetBounds(CalculateToastWidgetScreenBounds());
}

void UserNudgeController::SetVisible(bool visible) {
  if (is_visible_ == visible)
    return;

  is_visible_ = visible;

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
        .SetOpacity(&ripple_ring_, 0, gfx::Tween::FAST_OUT_SLOW_IN)
        .SetOpacity(toast_widget_->GetLayer(), 0, gfx::Tween::FAST_OUT_SLOW_IN);
    return;
  }

  // Animate the `base_ring_` and the `toast_widget_` to their default shown
  // opacity. Note that we don't need to show the `ripple_ring_` since it only
  // shows as the nudge animation is being performed.
  // Once those elements reach their default shown opacity, we perform the nudge
  // animation.
  builder
      .OnEnded(base::BindOnce(&UserNudgeController::PerformNudgeAnimations,
                              base::Unretained(this)))
      .Once()
      .SetDuration(kDelayToShowNudge)
      .SetOpacity(&base_ring_, kBaseRingOpacity, gfx::Tween::FAST_OUT_SLOW_IN)
      .SetOpacity(toast_widget_->GetLayer(), 1.f, gfx::Tween::FAST_OUT_SLOW_IN);
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
  gfx::Transform scale_up_transform;
  scale_up_transform.Scale(kBaseRingScaleUpFactor, kBaseRingScaleUpFactor);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&UserNudgeController::OnBaseRingAnimationEnded,
                              base::Unretained(this)))
      .Once()
      .SetDuration(kScaleUpDuration)
      .SetTransform(&base_ring_,
                    gfx::TransformAboutPivot(GetLocalCenterPoint(&base_ring_),
                                             scale_up_transform),
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
  gfx::Transform scale_up_transform;
  scale_up_transform.Scale(kRippleRingScaleUpFactor, kRippleRingScaleUpFactor);
  scale_up_transform = gfx::TransformAboutPivot(
      GetLocalCenterPoint(&ripple_ring_), scale_up_transform);
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
  gfx::Transform scale_up_transform;
  scale_up_transform.Scale(kHighlightedViewScaleUpFactor,
                           kHighlightedViewScaleUpFactor);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kScaleUpDuration)
      .SetTransform(view_layer,
                    gfx::TransformAboutPivot(GetLocalCenterPoint(view_layer),
                                             scale_up_transform),
                    gfx::Tween::ACCEL_40_DECEL_20)
      .Offset(kScaleDownOffset)
      .SetDuration(kScaleDownDuration)
      .SetTransform(view_layer, gfx::Transform(),
                    gfx::Tween::FAST_OUT_SLOW_IN_3);
}

void UserNudgeController::OnBaseRingAnimationEnded() {
  timer_.Start(FROM_HERE, kDelayToRepeatNudge,
               base::BindOnce(&UserNudgeController::PerformNudgeAnimations,
                              base::Unretained(this)));
}

aura::Window* UserNudgeController::GetParentWindow() const {
  auto* root_window =
      view_to_be_highlighted_->GetWidget()->GetNativeWindow()->GetRootWindow();
  DCHECK(root_window);
  return root_window->GetChildById(kShellWindowId_OverlayContainer);
}

gfx::Rect UserNudgeController::CalculateToastWidgetScreenBounds() const {
  const auto bar_widget_bounds_in_screen =
      view_to_be_highlighted_->GetWidget()->GetWindowBoundsInScreen();

  auto bounds = bar_widget_bounds_in_screen;
  if (toast_label_view_) {
    const auto preferred_size = toast_label_view_->GetPreferredSize();
    // We don't want the toast width to go beyond the capture bar width, but if
    // it can use a smaller width, then we align the horizontal centers of the
    // bar the toast together.
    const int fitted_width =
        preferred_size.width() + 2 * kToastHorizontalPadding;
    if (fitted_width < bar_widget_bounds_in_screen.width()) {
      bounds.set_width(fitted_width);
      bounds.set_x(bar_widget_bounds_in_screen.CenterPoint().x() -
                   fitted_width / 2);
    }
    // Note that the toast is allowed to have multiple lines if the width
    // doesn't fit the contents.
    bounds.set_height(toast_label_view_->GetHeightForWidth(bounds.width()) +
                      2 * kToastVerticalPadding);
  } else {
    // The content view hasn't been created yet, so we use a default height.
    // Calling Reposition() after the widget has been initialization will fix
    // any wrong bounds.
    bounds.set_height(kToastDefaultHeight);
  }

  bounds.set_y(bar_widget_bounds_in_screen.y() - bounds.height() -
               kToastSpacingFromBar);

  return bounds;
}

void UserNudgeController::BuildToastWidget() {
  toast_widget_->Init(CreateWidgetParams(GetParentWindow(),
                                         CalculateToastWidgetScreenBounds()));

  const int message_id =
      features::IsCaptureModeSelfieCameraEnabled()
          ? IDS_ASH_SCREEN_CAPTURE_SHOW_CAMERA_USER_NUDGE
          : IDS_ASH_SCREEN_CAPTURE_FOLDER_SELECTION_USER_NUDGE;
  toast_label_view_ = toast_widget_->SetContentsView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(message_id)));
  toast_label_view_->SetMultiLine(true);
  auto* color_provider = AshColorProvider::Get();
  SkColor background_color = color_provider->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80);
  toast_label_view_->SetBackground(
      views::CreateSolidBackground(background_color));
  toast_label_view_->SetBorder(views::CreateRoundedRectBorder(
      kToastBorderThickness, kToastCornerRadius,
      color_provider->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kHighlightColor1)));
  toast_label_view_->SetAutoColorReadabilityEnabled(false);
  const SkColor text_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  toast_label_view_->SetEnabledColor(text_color);
  toast_label_view_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  toast_label_view_->SetVerticalAlignment(gfx::ALIGN_MIDDLE);

  toast_label_view_->SetPaintToLayer();
  auto* label_layer = toast_label_view_->layer();
  label_layer->SetFillsBoundsOpaquely(false);
  label_layer->SetRoundedCornerRadius(kToastRoundedCorners);
  label_layer->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  label_layer->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  // The widget is created initially with 0 opacity, and will animate to be
  // fully visible when SetVisible() is called.
  toast_widget_->Show();
  auto* widget_layer = toast_widget_->GetLayer();
  widget_layer->SetOpacity(0);
}

}  // namespace ash
