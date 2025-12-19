// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_controller.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_cue_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/functional/bind.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/env.h"
#include "ui/events/event.h"
#include "ui/events/event_target.h"
#include "ui/events/gestures/gesture_recognizer_impl.h"
#include "ui/events/types/event_type.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The dimensions of the region that can activate the multitask menu.
// It's intentionally short in order to reduce the chance of clashing with
// window/tab dragging.
constexpr gfx::SizeF kHitRegionSize(200.f, 10.f);

// Returns true if `window` can show the menu and `screen_location` is in the
// menu hit bounds.
bool HitTestRect(aura::Window* window, const gfx::PointF& screen_location) {
  if (!TabletModeMultitaskMenuController::CanShowMenu(window)) {
    return false;
  }
  const gfx::RectF window_bounds(window->GetBoundsInScreen());
  gfx::RectF hit_region(window_bounds);
  hit_region.ClampToCenteredSize(kHitRegionSize);
  hit_region.set_y(window_bounds.y());
  return hit_region.Contains(screen_location);
}

// Returns the toplevel window for `target`, or active window if there isn't
// one. Note this can be the multitask menu, since we drag up in the menu
// coordinates.
aura::Window* GetTargetWindow(aura::Window* target) {
  views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(target);
  return widget ? widget->GetNativeWindow() : window_util::GetActiveWindow();
}

}  // namespace

TabletModeMultitaskMenuController::TabletModeMultitaskMenuController()
    : multitask_cue_controller_(
          std::make_unique<TabletModeMultitaskCueController>()) {
  Shell::Get()->AddPreTargetHandler(this);
}

TabletModeMultitaskMenuController::~TabletModeMultitaskMenuController() {
  // The cue needs to be destroyed first so that it doesn't do any work when
  // window activation changes as a result of destroying `this`.
  multitask_cue_controller_.reset();

  Shell::Get()->RemovePreTargetHandler(this);
}

// static
bool TabletModeMultitaskMenuController::CanShowMenu(aura::Window* window) {
  // Cannot show the menu in the lock screen, or in app/kiosk mode.
  if (Shell::Get()->session_controller()->IsScreenLocked() ||
      Shell::Get()->session_controller()->IsRunningInAppMode()) {
    return false;
  }

  auto* window_state = WindowState::Get(window);
  return window_state && window_state->CanMaximize() &&
         window_state->CanResize() && !window_state->IsFloated() &&
         !window_state->IsPinned();
}

void TabletModeMultitaskMenuController::ShowMultitaskMenu(
    aura::Window* window) {
  MaybeCreateMultitaskMenu(window);
  if (multitask_menu_) {
    multitask_menu_->Animate(/*show=*/true);
  }
}

void TabletModeMultitaskMenuController::ResetMultitaskMenu() {
  multitask_menu_.reset();
}

void TabletModeMultitaskMenuController::OnTouchEvent(ui::TouchEvent* event) {
  if (is_drag_active_) {
    if (!reserved_for_gesture_sent_) {
      reserved_for_gesture_sent_ = true;
      event->SetFlags(event->flags() | ui::EF_RESERVED_FOR_GESTURE);
      return;
    }
    event->StopPropagation();
    event->ForceProcessGesture();
  }
}

void TabletModeMultitaskMenuController::OnGestureEvent(
    ui::GestureEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* window = GetTargetWindow(target);
  if (!window ||
      !Shell::Get()->shell_delegate()->AllowDefaultTouchActions(window)) {
    return;
  }

  const ui::GestureEventDetails details = event->details();
  // Do not handle PEN and ERASER events. PEN events can come from stylus
  // device.
  if (details.primary_pointer_type() == ui::EventPointerType::kPen ||
      details.primary_pointer_type() == ui::EventPointerType::kEraser) {
    return;
  }

  gfx::PointF screen_location = event->location_f();
  wm::ConvertPointToScreen(target, &screen_location);

  // Save the window coordinates to pass to the menu.
  gfx::PointF window_location = event->location_f();
  aura::Window::ConvertPointToTarget(target, window, &window_location);
  switch (event->type()) {
    case ui::EventType::kGestureScrollBegin:
      if (std::fabs(details.scroll_y_hint()) <
          std::fabs(details.scroll_x_hint())) {
        return;
      }
      is_drag_active_ = false;
      reserved_for_gesture_sent_ = false;
      if (details.scroll_y_hint() > 0 && HitTestRect(window, screen_location)) {
        // We may need to recreate `multitask_menu_` on the new target window.
        multitask_menu_ =
            std::make_unique<TabletModeMultitaskMenu>(this, window);
        multitask_cue_controller_->OnMenuOpened(window);
        BeginDrag(event, window_location.y(), /*down=*/true);
      } else if (details.scroll_y_hint() < 0 && multitask_menu_ &&
                 gfx::RectF(
                     multitask_menu_->widget()->GetWindowBoundsInScreen())
                     .Contains(screen_location)) {
        // If the menu is open and scroll up begins, only handle events inside
        // the menu to avoid consuming scroll events outside the menu.
        // TODO(b/279816982): Fix the cue reappearing when the menu is dismissed
        // by swiping up or not dragging far enough.
        BeginDrag(event, window_location.y(), /*down=*/false);
      }
      break;
    case ui::EventType::kGestureScrollUpdate:
      if (is_drag_active_ && multitask_menu_) {
        multitask_menu_->UpdateDrag(window_location.y(),
                                    /*down=*/details.scroll_y() > 0);
        event->SetHandled();
      }
      break;
    case ui::EventType::kGestureScrollEnd:
    case ui::EventType::kGestureEnd:
      // If an unsupported gesture is sent, make sure we reset `is_drag_active_`
      // to stop consuming events.
      if (is_drag_active_ && multitask_menu_) {
        multitask_menu_->EndDrag();
        event->SetHandled();
      }
      is_drag_active_ = false;
      reserved_for_gesture_sent_ = false;
      break;
    case ui::EventType::kScrollFlingStart:
      if (!is_drag_active_) {
        return;
      }
      // Normally EventType::kGestureScrollBegin will fire first and have
      // already created the multitask menu, however occasionally
      // EventType::kScrollFlingStart may fire first (https://crbug.com/821237).
      MaybeCreateMultitaskMenu(window);
      if (multitask_menu_) {
        multitask_menu_->Animate(details.velocity_y() > 0);
        event->SetHandled();
      }
      break;
    default:
      if (is_drag_active_ && multitask_menu_) {
        // Do not reset `is_drag_active_` to handle until the gesture ends.
        event->SetHandled();
      }
      break;
  }
}

void TabletModeMultitaskMenuController::MaybeCreateMultitaskMenu(
    aura::Window* window) {
  if (!multitask_menu_ && CanShowMenu(window)) {
    multitask_menu_ = std::make_unique<TabletModeMultitaskMenu>(this, window);
    multitask_cue_controller_->OnMenuOpened(window);
  }
}

void TabletModeMultitaskMenuController::BeginDrag(ui::GestureEvent* event,
                                                  int y,
                                                  bool down) {
  multitask_menu_->BeginDrag(y, down);

  event->SetHandled();
  is_drag_active_ = true;

  // If the gesture started on consumer other than the multitask menu, transfer
  // to the multi task menu so that the gesture sequence can contunue even after
  // capture is released.
  auto* window = multitask_menu_->widget()->GetNativeWindow();
  auto* target = static_cast<aura::Window*>(event->target());
  if (window != target) {
    aura::Env::GetInstance()->gesture_recognizer()->TransferEventsTo(
        target, window, ui::TransferTouchesBehavior::kDontCancel);
  }

  // A client window might have captured events. Release it so that the menu can
  // process the events.
  auto* capture_window = aura::client::GetCaptureWindow(window);
  if (capture_window && capture_window != window) {
    capture_window->ReleaseCapture();
  }
}

}  // namespace ash
