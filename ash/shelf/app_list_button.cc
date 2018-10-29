// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/app_list_button.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/assistant_overlay.h"
#include "ash/shelf/ink_drop_button_listener.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/shell_state.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/timer/timer.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/account_id/account_id.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr int kVoiceInteractionAnimationDelayMs = 200;
constexpr int kVoiceInteractionAnimationHideDelayMs = 500;
constexpr uint8_t kVoiceInteractionRunningAlpha = 255;     // 100% alpha
constexpr uint8_t kVoiceInteractionNotRunningAlpha = 138;  // 54% alpha

bool IsAssistantEnabled() {
  return chromeos::switches::IsVoiceInteractionEnabled() ||
         chromeos::switches::IsAssistantEnabled();
}

bool IsHomeLauncherShown() {
  return Shell::Get()
      ->app_list_controller()
      ->IsHomeLauncherEnabledInTabletMode();
}

}  // namespace

AppListButton::AppListButton(InkDropButtonListener* listener,
                             ShelfView* shelf_view,
                             Shelf* shelf)
    : views::ImageButton(nullptr),
      listener_(listener),
      shelf_view_(shelf_view),
      shelf_(shelf),
      voice_interaction_binding_(this) {
  DCHECK(listener_);
  DCHECK(shelf_view_);
  DCHECK(shelf_);
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);

  mojom::VoiceInteractionObserverPtr ptr;
  voice_interaction_binding_.Bind(mojo::MakeRequest(&ptr));
  Shell::Get()->voice_interaction_controller()->AddObserver(std::move(ptr));
  SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
  set_ink_drop_base_color(kShelfInkDropBaseColor);
  set_ink_drop_visible_opacity(kShelfInkDropVisibleOpacity);
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE));
  SetSize(gfx::Size(kShelfControlSize, kShelfControlSize));
  SetFocusPainter(TrayPopupUtils::CreateFocusPainter());
  set_notify_action(Button::NOTIFY_ON_PRESS);

  // Initialize voice interaction overlay and sync the flags if active user
  // session has already started. This could happen when an external monitor
  // is plugged in.
  if (Shell::Get()->session_controller()->IsActiveUserSessionStarted() &&
      IsAssistantEnabled()) {
    InitializeVoiceInteractionOverlay();
  }
}

AppListButton::~AppListButton() {
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void AppListButton::OnAppListShown() {
  // Do not show a highlight in tablet mode since the "homecher" view is always
  // open in the background.
  if (!IsHomeLauncherShown())
    AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);
  is_showing_app_list_ = true;
  shelf_->UpdateAutoHideState();
}

void AppListButton::OnAppListDismissed() {
  AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
  is_showing_app_list_ = false;
  shelf_->UpdateAutoHideState();
}

void AppListButton::OnGestureEvent(ui::GestureEvent* event) {
  // Handle gesture events that are on the app list circle.
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      AnimateInkDrop(views::InkDropState::HIDDEN, event);
      ImageButton::OnGestureEvent(event);
      return;
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_TAP_CANCEL:
      if (UseVoiceInteractionStyle()) {
        assistant_overlay_->EndAnimation();
        assistant_animation_delay_timer_->Stop();
      }
      if (IsHomeLauncherShown())
        AnimateInkDrop(views::InkDropState::ACTION_TRIGGERED, event);
      ImageButton::OnGestureEvent(event);
      return;
    case ui::ET_GESTURE_TAP_DOWN:
      if (UseVoiceInteractionStyle()) {
        assistant_animation_delay_timer_->Start(
            FROM_HERE,
            base::TimeDelta::FromMilliseconds(
                kVoiceInteractionAnimationDelayMs),
            base::Bind(&AppListButton::StartVoiceInteractionAnimation,
                       base::Unretained(this)));
      }
      if (!Shell::Get()->app_list_controller()->IsVisible() ||
          IsHomeLauncherShown()) {
        AnimateInkDrop(views::InkDropState::ACTION_PENDING, event);
      }
      ImageButton::OnGestureEvent(event);
      return;
    case ui::ET_GESTURE_LONG_PRESS:
      if (UseVoiceInteractionStyle()) {
        base::RecordAction(base::UserMetricsAction(
            "VoiceInteraction.Started.AppListButtonLongPress"));
        assistant_overlay_->BurstAnimation();
        event->SetHandled();
        Shell::Get()->shell_state()->SetRootWindowForNewWindows(
            GetWidget()->GetNativeWindow()->GetRootWindow());
        if (chromeos::switches::IsAssistantEnabled()) {
          Shell::Get()->assistant_controller()->ui_controller()->ShowUi(
              AssistantSource::kLongPressLauncher);
        } else {
          Shell::Get()->app_list_controller()->StartVoiceInteractionSession();
        }
      } else {
        ImageButton::OnGestureEvent(event);
      }
      return;
    case ui::ET_GESTURE_LONG_TAP:
      if (UseVoiceInteractionStyle()) {
        // Also consume the long tap event. This happens after the user long
        // presses and lifts the finger. We already handled the long press
        // ignore the long tap to avoid bringing up the context menu again.
        AnimateInkDrop(views::InkDropState::HIDDEN, event);
        event->SetHandled();
      } else {
        ImageButton::OnGestureEvent(event);
      }
      return;
    default:
      ImageButton::OnGestureEvent(event);
      return;
  }
}

bool AppListButton::OnMousePressed(const ui::MouseEvent& event) {
  ImageButton::OnMousePressed(event);
  shelf_view_->PointerPressedOnButton(this, ShelfView::MOUSE, event);
  return true;
}

void AppListButton::OnMouseReleased(const ui::MouseEvent& event) {
  ImageButton::OnMouseReleased(event);
  shelf_view_->PointerReleasedOnButton(this, ShelfView::MOUSE, false);
}

void AppListButton::OnMouseCaptureLost() {
  shelf_view_->PointerReleasedOnButton(this, ShelfView::MOUSE, true);
  ImageButton::OnMouseCaptureLost();
}

bool AppListButton::OnMouseDragged(const ui::MouseEvent& event) {
  ImageButton::OnMouseDragged(event);
  shelf_view_->PointerDraggedOnButton(this, ShelfView::MOUSE, event);
  return true;
}

void AppListButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kButton;
  node_data->SetName(shelf_view_->GetTitleForView(this));
}

std::unique_ptr<views::InkDropRipple> AppListButton::CreateInkDropRipple()
    const {
  const int app_list_button_radius = ShelfConstants::app_list_button_radius();
  gfx::Point center = GetCenterPoint();
  gfx::Rect bounds(center.x() - app_list_button_radius,
                   center.y() - app_list_button_radius,
                   2 * app_list_button_radius, 2 * app_list_button_radius);
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetLocalBounds().InsetsFrom(bounds),
      GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
      ink_drop_visible_opacity());
}

void AppListButton::NotifyClick(const ui::Event& event) {
  ImageButton::NotifyClick(event);
  if (listener_)
    listener_->ButtonPressed(this, event, GetInkDrop());
}

bool AppListButton::ShouldEnterPushedState(const ui::Event& event) {
  if (!shelf_view_->ShouldEventActivateButton(this, event))
    return false;
  if (Shell::Get()->app_list_controller()->IsVisible())
    return false;
  return views::ImageButton::ShouldEnterPushedState(event);
}

std::unique_ptr<views::InkDrop> AppListButton::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      Button::CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropMask> AppListButton::CreateInkDropMask() const {
  return std::make_unique<views::CircleInkDropMask>(
      size(), GetCenterPoint(), ShelfConstants::app_list_button_radius());
}

void AppListButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::PointF circle_center(GetCenterPoint());

  // Paint a white ring as the foreground for the app list circle. The ceil/dsf
  // math assures that the ring draws sharply and is centered at all scale
  // factors.
  float ring_outer_radius_dp = 7.f;
  float ring_thickness_dp = 1.5f;
  if (UseVoiceInteractionStyle()) {
    ring_outer_radius_dp = 8.f;
    ring_thickness_dp = 1.f;
  }
  {
    gfx::ScopedCanvas scoped_canvas(canvas);
    const float dsf = canvas->UndoDeviceScaleFactor();
    circle_center.Scale(dsf);
    cc::PaintFlags fg_flags;
    fg_flags.setAntiAlias(true);
    fg_flags.setStyle(cc::PaintFlags::kStroke_Style);
    fg_flags.setColor(kShelfIconColor);

    if (UseVoiceInteractionStyle()) {
      mojom::VoiceInteractionState state = Shell::Get()
                                               ->voice_interaction_controller()
                                               ->voice_interaction_state();
      // active: 100% alpha, inactive: 54% alpha
      fg_flags.setAlpha(state == mojom::VoiceInteractionState::RUNNING
                            ? kVoiceInteractionRunningAlpha
                            : kVoiceInteractionNotRunningAlpha);
    }

    const float thickness = std::ceil(ring_thickness_dp * dsf);
    const float radius = std::ceil(ring_outer_radius_dp * dsf) - thickness / 2;
    fg_flags.setStrokeWidth(thickness);
    // Make sure the center of the circle lands on pixel centers.
    canvas->DrawCircle(circle_center, radius, fg_flags);

    if (UseVoiceInteractionStyle()) {
      fg_flags.setAlpha(255);
      const float kCircleRadiusDp = 5.f;
      fg_flags.setStyle(cc::PaintFlags::kFill_Style);
      canvas->DrawCircle(circle_center, std::ceil(kCircleRadiusDp * dsf),
                         fg_flags);
    }
  }
}

gfx::Point AppListButton::GetCenterPoint() const {
  // For a bottom-aligned shelf, the button bounds could have a larger height
  // than width (in the case of touch-dragging the shelf upwards) or a larger
  // width than height (in the case of a shelf hide/show animation), so adjust
  // the y-position of the circle's center to ensure correct layout. Similarly
  // adjust the x-position for a left- or right-aligned shelf.
  const int x_mid = width() / 2.f;
  const int y_mid = height() / 2.f;

  const ShelfAlignment alignment = shelf_->alignment();
  if (alignment == SHELF_ALIGNMENT_BOTTOM ||
      alignment == SHELF_ALIGNMENT_BOTTOM_LOCKED) {
    return gfx::Point(x_mid, x_mid);
  } else if (alignment == SHELF_ALIGNMENT_RIGHT) {
    return gfx::Point(y_mid, y_mid);
  } else {
    DCHECK_EQ(alignment, SHELF_ALIGNMENT_LEFT);
    return gfx::Point(width() - y_mid, y_mid);
  }
}

void AppListButton::OnAppListVisibilityChanged(bool shown,
                                               aura::Window* root_window) {
  aura::Window* window = GetWidget() ? GetWidget()->GetNativeWindow() : nullptr;
  if (!window || window->GetRootWindow() != root_window)
    return;

  if (shown)
    OnAppListShown();
  else
    OnAppListDismissed();
}

void AppListButton::OnVoiceInteractionStatusChanged(
    mojom::VoiceInteractionState state) {
  SchedulePaint();

  if (!assistant_overlay_)
    return;

  switch (state) {
    case mojom::VoiceInteractionState::STOPPED:
      UMA_HISTOGRAM_TIMES(
          "VoiceInteraction.OpenDuration",
          base::TimeTicks::Now() - voice_interaction_start_timestamp_);
      break;
    case mojom::VoiceInteractionState::NOT_READY:
      // If we are showing the bursting or waiting animation, no need to do
      // anything. Otherwise show the waiting animation now.
      // NOTE: No waiting animation for native assistant.
      if (!chromeos::switches::IsAssistantEnabled() &&
          !assistant_overlay_->IsBursting() &&
          !assistant_overlay_->IsWaiting()) {
        assistant_overlay_->WaitingAnimation();
      }
      break;
    case mojom::VoiceInteractionState::RUNNING:
      // we start hiding the animation if it is running.
      if (assistant_overlay_->IsBursting() || assistant_overlay_->IsWaiting()) {
        assistant_animation_hide_delay_timer_->Start(
            FROM_HERE,
            base::TimeDelta::FromMilliseconds(
                kVoiceInteractionAnimationHideDelayMs),
            base::Bind(&AssistantOverlay::HideAnimation,
                       base::Unretained(assistant_overlay_)));
      }

      voice_interaction_start_timestamp_ = base::TimeTicks::Now();
      break;
  }
}

void AppListButton::OnVoiceInteractionSettingsEnabled(bool enabled) {
  SchedulePaint();
}

void AppListButton::OnVoiceInteractionSetupCompleted(bool completed) {
  SchedulePaint();
}

void AppListButton::OnActiveUserSessionChanged(const AccountId& account_id) {
  SchedulePaint();
  // Initialize voice interaction overlay when primary user session becomes
  // active.
  if (Shell::Get()->session_controller()->IsUserPrimary() &&
      !assistant_overlay_ && IsAssistantEnabled()) {
    InitializeVoiceInteractionOverlay();
  }
}

void AppListButton::StartVoiceInteractionAnimation() {
  // We only show the voice interaction icon and related animation when the
  // shelf is at the bottom position and voice interaction is not running and
  // voice interaction setup flow has completed.
  ShelfAlignment alignment = shelf_->alignment();
  mojom::VoiceInteractionState state =
      Shell::Get()->voice_interaction_controller()->voice_interaction_state();
  bool show_icon =
      (alignment == SHELF_ALIGNMENT_BOTTOM ||
       alignment == SHELF_ALIGNMENT_BOTTOM_LOCKED) &&
      state == mojom::VoiceInteractionState::STOPPED &&
      Shell::Get()->voice_interaction_controller()->setup_completed() &&
      chromeos::switches::IsVoiceInteractionEnabled();
  assistant_overlay_->StartAnimation(show_icon);
}

bool AppListButton::UseVoiceInteractionStyle() {
  VoiceInteractionController* controller =
      Shell::Get()->voice_interaction_controller();
  bool settings_enabled = controller->settings_enabled();
  bool setup_completed = controller->setup_completed();
  bool is_feature_allowed =
      controller->allowed_state() == mojom::AssistantAllowedState::ALLOWED;
  if (assistant_overlay_ && is_feature_allowed &&
      (settings_enabled || !setup_completed)) {
    return true;
  }
  return false;
}

void AppListButton::InitializeVoiceInteractionOverlay() {
  assistant_overlay_ = new AssistantOverlay(this);
  AddChildView(assistant_overlay_);
  assistant_overlay_->SetVisible(false);
  assistant_animation_delay_timer_ = std::make_unique<base::OneShotTimer>();
  assistant_animation_hide_delay_timer_ =
      std::make_unique<base::OneShotTimer>();
}

}  // namespace ash
