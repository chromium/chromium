// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"

#include "ash/accelerators/debug_commands.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_cue.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/functional/bind.h"
#include "ui/events/event.h"
#include "ui/events/event_target.h"
#include "ui/events/types/event_type.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The dimensions of the region that can activate the multitask menu.
constexpr gfx::SizeF kHitRegionSize(200.f, 100.f);

// Returns true if `window` can show the menu and `screen_location` is in the
// menu hit bounds.
bool HitTestRect(aura::Window* window, const gfx::PointF& screen_location) {
  if (!TabletModeMultitaskMenuEventHandler::CanShowMenu(window)) {
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

TabletModeMultitaskMenuEventHandler::TabletModeMultitaskMenuEventHandler()
    : multitask_cue_(std::make_unique<TabletModeMultitaskCue>()) {
  Shell::Get()->AddPreTargetHandler(this);
}

TabletModeMultitaskMenuEventHandler::~TabletModeMultitaskMenuEventHandler() {
  // The cue needs to be destroyed first so that it doesn't do any work when
  // window activation changes as a result of destroying `this`.
  multitask_cue_.reset();

  Shell::Get()->RemovePreTargetHandler(this);
}

// static
bool TabletModeMultitaskMenuEventHandler::CanShowMenu(aura::Window* window) {
  auto* window_state = WindowState::Get(window);
  return window_state && window_state->CanMaximize() &&
         window_state->CanResize() && !window_state->IsFloated() &&
         !window_state->IsPinned();
}

void TabletModeMultitaskMenuEventHandler::ShowMultitaskMenu(
    aura::Window* window) {
  MaybeCreateMultitaskMenu(window);
  multitask_menu_->Animate(/*show=*/true);
}

void TabletModeMultitaskMenuEventHandler::ResetMultitaskMenu() {
  multitask_cue_->ResetPosition();
  multitask_menu_.reset();
}

void TabletModeMultitaskMenuEventHandler::OnGestureEvent(
    ui::GestureEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* window = GetTargetWindow(target);
  if (!window) {
    return;
  }

  gfx::PointF screen_location = event->location_f();
  wm::ConvertPointToScreen(target, &screen_location);

  // Save the window coordinates to pass to the menu.
  gfx::PointF window_location = event->location_f();
  aura::Window::ConvertPointToTarget(target, window, &window_location);

  const ui::GestureEventDetails details = event->details();
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (std::fabs(details.scroll_y_hint()) <
          std::fabs(details.scroll_x_hint())) {
        return;
      }
      is_drag_active_ = false;
      if (details.scroll_y_hint() > 0 && HitTestRect(window, screen_location)) {
        // We may need to recreate `multitask_menu_` on the new target window.
        multitask_menu_ =
            std::make_unique<TabletModeMultitaskMenu>(this, window);
        multitask_cue_->OnMenuOpened(window);
        multitask_menu_->BeginDrag(window_location.y(), /*down=*/true);
        event->SetHandled();
        is_drag_active_ = true;
      } else if (details.scroll_y_hint() < 0 && multitask_menu_ &&
                 gfx::RectF(
                     multitask_menu_->widget()->GetWindowBoundsInScreen())
                     .Contains(screen_location)) {
        // If the menu is open and scroll up begins, only handle events inside
        // the menu to avoid consuming scroll events outside the menu.
        // TODO(b/279816982): Fix the cue reappearing when the menu is dismissed
        // by swiping up or not dragging far enough.
        multitask_menu_->BeginDrag(window_location.y(), /*down=*/false);
        event->SetHandled();
        is_drag_active_ = true;
      }
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (is_drag_active_ && multitask_menu_) {
        multitask_menu_->UpdateDrag(window_location.y(),
                                    /*down=*/details.scroll_y() > 0);
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_GESTURE_END:
      // If an unsupported gesture is sent, make sure we reset `is_drag_active_`
      // to stop consuming events.
      if (is_drag_active_ && multitask_menu_) {
        multitask_menu_->EndDrag();
        event->SetHandled();
      }
      is_drag_active_ = false;
      break;
    case ui::ET_SCROLL_FLING_START:
      if (!is_drag_active_) {
        return;
      }
      // Normally ET_GESTURE_SCROLL_BEGIN will fire first and have already
      // created the multitask menu, however occasionally ET_SCROLL_FLING_START
      // may fire first (https://crbug.com/821237).
      MaybeCreateMultitaskMenu(window);
      multitask_menu_->Animate(details.velocity_y() > 0);
      event->SetHandled();
      break;
    default:
      if (is_drag_active_ && multitask_menu_) {
        // Do not reset `is_drag_active_` to handle until the gesture ends.
        event->SetHandled();
      }
      break;
  }
}

void TabletModeMultitaskMenuEventHandler::MaybeCreateMultitaskMenu(
    aura::Window* window) {
  if (!multitask_menu_) {
    multitask_menu_ = std::make_unique<TabletModeMultitaskMenu>(this, window);
    multitask_cue_->OnMenuOpened(window);
  }
}

}  // namespace ash
