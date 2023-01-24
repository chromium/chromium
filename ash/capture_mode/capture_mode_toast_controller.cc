// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_toast_controller.h"

#include <memory>

#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

// Specs for `capture_toast_widget_`.
constexpr int kToastSpacingFromBar = 8;
constexpr int kToastDefaultHeight = 36;
constexpr int kToastVerticalPadding = 8;
constexpr int kToastHorizontalPadding = 16;
constexpr int kToastBorderThickness = 1;

// Animation duration for updating the visibility of `capture_toast_widget_`.
constexpr base::TimeDelta kCaptureToastVisibilityChangeDuration =
    base::Milliseconds(200);

// The duration that `capture_toast_widget_` will remain visible after it's
// created when there are no actions taken. After this, the toast widget will be
// dismissed.
constexpr base::TimeDelta kDelayToDismissToast = base::Seconds(6);

std::u16string GetCaptureToastLabelOnToastType(
    CaptureToastType capture_toast_type) {
  const int nudge_message_id =
      features::AreCaptureModeDemoToolsEnabled()
          ? IDS_ASH_SCREEN_CAPTURE_SHOW_DEMO_TOOLS_USER_NUDGE
          : IDS_ASH_SCREEN_CAPTURE_SHOW_CAMERA_USER_NUDGE;
  const int message_id =
      capture_toast_type == CaptureToastType::kCameraPreview
          ? IDS_ASH_SCREEN_CAPTURE_SURFACE_TOO_SMALL_USER_NUDGE
          : nudge_message_id;
  return l10n_util::GetStringUTF16(message_id);
}

// Returns the init params that will be used for the toast widget.
views::Widget::InitParams CreateWidgetParams(aura::Window* parent,
                                             const gfx::Rect& bounds) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = parent;
  params.bounds = bounds;
  params.name = "CaptureModeToastWidget";
  params.accept_events = false;
  return params;
}

}  // namespace

CaptureModeToastController::CaptureModeToastController(
    CaptureModeSession* session)
    : capture_session_(session) {}

CaptureModeToastController::~CaptureModeToastController() {
  if (capture_toast_widget_)
    capture_toast_widget_->CloseNow();
}

void CaptureModeToastController::ShowCaptureToast(
    CaptureToastType capture_toast_type) {
  current_toast_type_ = capture_toast_type;
  const std::u16string capture_toast_label =
      GetCaptureToastLabelOnToastType(capture_toast_type);

  if (!capture_toast_widget_)
    BuildCaptureToastWidget(capture_toast_label);
  else
    toast_label_view_->SetText(capture_toast_label);

  MaybeRepositionCaptureToast();
  const bool did_visibility_change = capture_mode_util::SetWidgetVisibility(
      capture_toast_widget_.get(), /*target_visibility=*/true,
      capture_mode_util::AnimationParams{kCaptureToastVisibilityChangeDuration,
                                         gfx::Tween::FAST_OUT_SLOW_IN,
                                         /*apply_scale_up_animation=*/false});

  // Only if the capture toast type is the `kCameraPreview`, the capture toast
  // should be auto dismissed after `kDelayToDismissToast`.
  if (did_visibility_change &&
      capture_toast_type == CaptureToastType::kCameraPreview) {
    capture_toast_dismiss_timer_.Start(
        FROM_HERE, kDelayToDismissToast,
        base::BindOnce(&CaptureModeToastController::MaybeDismissCaptureToast,
                       base::Unretained(this), capture_toast_type,
                       /*animate=*/true));
  }
}

void CaptureModeToastController::MaybeDismissCaptureToast(
    CaptureToastType capture_toast_type,
    bool animate) {
  if (!current_toast_type_) {
    DCHECK(!capture_toast_widget_ ||
           !capture_mode_util::GetWidgetCurrentVisibility(
               capture_toast_widget_.get()));
    return;
  }

  if (!capture_toast_widget_) {
    DCHECK(!current_toast_type_);
    return;
  }

  if (capture_toast_type != current_toast_type_)
    return;

  capture_toast_dismiss_timer_.Stop();

  current_toast_type_.reset();
  if (animate) {
    capture_mode_util::SetWidgetVisibility(
        capture_toast_widget_.get(), /*target_visibility=*/false,
        capture_mode_util::AnimationParams{
            kCaptureToastVisibilityChangeDuration, gfx::Tween::FAST_OUT_SLOW_IN,
            /*apply_scale_up_animation=*/false});
    return;
  }

  capture_toast_widget_->Hide();
}

void CaptureModeToastController::DismissCurrentToastIfAny() {
  if (current_toast_type_)
    MaybeDismissCaptureToast(*current_toast_type_, /*animate=*/false);
}

void CaptureModeToastController::MaybeRepositionCaptureToast() {
  if (!capture_toast_widget_)
    return;

  auto* parent_window = capture_session_->current_root()->GetChildById(
      kShellWindowId_MenuContainer);

  if (capture_toast_widget_->GetNativeWindow()->parent() != parent_window) {
    parent_window->AddChild(capture_toast_widget_->GetNativeWindow());
    auto* layer = capture_toast_widget_->GetLayer();
    // Any ongoing opacity animation should be committed when we reparent the
    // toast, otherwise it doesn't look good.
    layer->SetOpacity(layer->GetTargetOpacity());
  }

  capture_toast_widget_->SetBounds(CalculateToastWidgetScreenBounds());
}

ui::Layer* CaptureModeToastController::MaybeGetToastLayer() {
  return capture_toast_widget_ ? capture_toast_widget_->GetLayer() : nullptr;
}

void CaptureModeToastController::BuildCaptureToastWidget(
    const std::u16string& label) {
  // Create the widget before init it to ensure when the window gets added to
  // the parent container, `capture_toast_widget_` is already available.
  capture_toast_widget_ = std::make_unique<views::Widget>();
  const gfx::Rect toast_widget_screen_bounds =
      CalculateToastWidgetScreenBounds();
  capture_toast_widget_->Init(
      CreateWidgetParams(capture_session_->current_root()->GetChildById(
                             kShellWindowId_MenuContainer),
                         toast_widget_screen_bounds));

  // We animate the toast widget explicitly in `ShowCaptureToast()` and
  // `MaybeDismissCaptureToast()`. Any default visibility animations added by
  // the widget's window should be disabled.
  capture_toast_widget_->SetVisibilityAnimationTransition(
      views::Widget::ANIMATE_NONE);

  toast_label_view_ = capture_toast_widget_->SetContentsView(
      std::make_unique<views::Label>(label));
  toast_label_view_->SetMultiLine(true);
  toast_label_view_->SetBackground(
      views::CreateThemedSolidBackground(kColorAshShieldAndBase80));
  const float toast_corner_radius = toast_widget_screen_bounds.height() / 2.f;
  toast_label_view_->SetBorder(views::CreateThemedRoundedRectBorder(
      kToastBorderThickness, toast_corner_radius,
      ui::kColorHighlightBorderHighlight1));
  toast_label_view_->SetAutoColorReadabilityEnabled(false);
  toast_label_view_->SetEnabledColorId(kColorAshTextColorPrimary);
  toast_label_view_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  toast_label_view_->SetVerticalAlignment(gfx::ALIGN_MIDDLE);

  toast_label_view_->SetPaintToLayer();
  auto* label_layer = toast_label_view_->layer();
  label_layer->SetFillsBoundsOpaquely(false);
  label_layer->SetRoundedCornerRadius(
      gfx::RoundedCornersF(toast_corner_radius));
  label_layer->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  label_layer->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  // The widget is created initially with 0 opacity, and will animate to be
  // fully visible when `ShowCaptureToast` is called.
  capture_toast_widget_->Show();
  auto* widget_layer = capture_toast_widget_->GetLayer();
  widget_layer->SetOpacity(0);
}

gfx::Rect CaptureModeToastController::CalculateToastWidgetScreenBounds() const {
  const auto bar_widget_bounds_in_screen =
      capture_session_->capture_mode_bar_widget()->GetWindowBoundsInScreen();

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

}  // namespace ash
