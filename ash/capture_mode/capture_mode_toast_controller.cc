// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_toast_controller.h"

#include <memory>

#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/system_toast_view.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

constexpr int kToastSpacingFromBar = 8;

// Animation duration for updating the visibility of `capture_toast_widget_`.
constexpr base::TimeDelta kCaptureToastVisibilityChangeDuration =
    base::Milliseconds(200);

// The duration that `capture_toast_widget_` remains visible after been created
// and there are no actions taken, after which the toast widget will be
// dismissed.
constexpr base::TimeDelta kDelayToDismissToast = base::Seconds(6);

std::u16string GetCaptureToastTextOnToastType(
    CaptureToastType capture_toast_type) {
  const int nudge_message_id =
      IDS_ASH_SCREEN_CAPTURE_SHOW_DEMO_TOOLS_USER_NUDGE;

  const int message_id =
      capture_toast_type == CaptureToastType::kCameraPreview
          ? IDS_ASH_SCREEN_CAPTURE_SURFACE_TOO_SMALL_USER_NUDGE
          : nudge_message_id;
  return l10n_util::GetStringUTF16(message_id);
}

// Returns the init params that will be used for the toast widget.
views::Widget::InitParams CreateWidgetParams(aura::Window* parent) {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = parent;
  params.name = "CaptureModeToastWidget";
  params.accept_events = false;
  return params;
}

}  // namespace

CaptureModeToastController::CaptureModeToastController(
    CaptureModeSession* session)
    : capture_session_(session) {}

CaptureModeToastController::~CaptureModeToastController() {
  // Widget needs to be closed immediately so it does not show in the
  // screenshot.
  if (capture_toast_widget_) {
    capture_toast_widget_->CloseNow();
  }
}

void CaptureModeToastController::ShowCaptureToast(
    CaptureToastType capture_toast_type) {
  current_toast_type_ = capture_toast_type;
  const std::u16string capture_toast_text =
      GetCaptureToastTextOnToastType(capture_toast_type);

  if (!capture_toast_widget_) {
    BuildCaptureToastWidget(capture_toast_text);
  } else {
    toast_contents_view_->SetText(capture_toast_text);
  }

  capture_mode_util::TriggerAccessibilityAlertSoon(
      base::UTF16ToUTF8(capture_toast_text));

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
    // toast in case that the residual animation continues in the old position
    // after been repositioned to a new bounds.
    layer->SetOpacity(layer->GetTargetOpacity());
  }

  capture_toast_widget_->SetBounds(CalculateToastWidgetBoundsInScreen());
}

ui::Layer* CaptureModeToastController::MaybeGetToastLayer() {
  return capture_toast_widget_ ? capture_toast_widget_->GetLayer() : nullptr;
}

void CaptureModeToastController::OnWidgetDestroying(views::Widget* widget) {
  toast_contents_view_ = nullptr;
  if (capture_toast_widget_) {
    capture_toast_widget_->RemoveObserver(this);
  }
}

void CaptureModeToastController::BuildCaptureToastWidget(
    const std::u16string& text) {
  // Create the widget before init it to ensure that the `capture_toast_widget_`
  // is available when the window gets added to the parent container.
  capture_toast_widget_ = std::make_unique<views::Widget>();
  capture_toast_widget_->Init(
      CreateWidgetParams(capture_session_->current_root()->GetChildById(
          kShellWindowId_MenuContainer)));
  capture_toast_widget_->AddObserver(this);
  toast_contents_view_ = capture_toast_widget_->SetContentsView(
      std::make_unique<SystemToastView>(text));

  // We animate the `capture_toast_widget_` explicitly in `ShowCaptureToast()`
  // and `MaybeDismissCaptureToast()`. Any default visibility animations added
  // by the widget's window should be disabled.
  capture_toast_widget_->SetVisibilityAnimationTransition(
      views::Widget::ANIMATE_NONE);
  const auto toast_bounds_in_screen = CalculateToastWidgetBoundsInScreen();
  capture_toast_widget_->SetBounds(toast_bounds_in_screen);
  toast_contents_view_->SetPaintToLayer();
  toast_contents_view_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(toast_bounds_in_screen.height() / 2.f));
  capture_toast_widget_->Show();

  // The widget is created initially with 0 opacity, and will animate to be
  // fully visible when `ShowCaptureToast` is called.
  capture_toast_widget_->GetLayer()->SetOpacity(0);
}

gfx::Rect CaptureModeToastController::CalculateToastWidgetBoundsInScreen()
    const {
  DCHECK(toast_contents_view_);

  gfx::Rect bounds;
  const auto preferred_size = toast_contents_view_->GetPreferredSize();
  bounds = gfx::Rect(preferred_size);

  // Align the centers of the capture mode bar and the toast horizontally.
  const auto bar_widget_bounds_in_screen =
      capture_session_->GetCaptureModeBarWidget()->GetWindowBoundsInScreen();
  bounds.set_x(bar_widget_bounds_in_screen.CenterPoint().x() -
               preferred_size.width() / 2);
  bounds.set_y(bar_widget_bounds_in_screen.y() - bounds.height() -
               kToastSpacingFromBar);
  return bounds;
}

}  // namespace ash
