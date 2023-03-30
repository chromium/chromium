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

}  // namespace

TabletModeMultitaskMenuEventHandler::TabletModeMultitaskMenuEventHandler() {
  multitask_cue_ = std::make_unique<TabletModeMultitaskCue>();
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
  return !window_state->IsFloated() && window_state->CanMaximize() &&
         window_state->CanResize();
}

void TabletModeMultitaskMenuEventHandler::ShowMultitaskMenu(
    aura::Window* window) {
  MaybeCreateMultitaskMenu(window);
  multitask_menu_->Animate(/*show=*/true);
}

void TabletModeMultitaskMenuEventHandler::ResetMultitaskMenu() {
  multitask_menu_.reset();
}

void TabletModeMultitaskMenuEventHandler::OnGestureEvent(
    ui::GestureEvent* event) {
  aura::Window* active_window = window_util::GetActiveWindow();
  auto* window_state = WindowState::Get(active_window);
  // No-op if there is no multitask menu and no active window that can open the
  // menu. If the menu is open, the checks in the second condition do not apply
  // since the menu is the active window.
  if (!multitask_menu_ && (!active_window || window_state->IsFloated() ||
                           !window_state->CanMaximize())) {
    return;
  }

  aura::Window* target = static_cast<aura::Window*>(event->target());
  gfx::PointF screen_location = event->location_f();
  wm::ConvertPointToScreen(target, &screen_location);

  // If the menu is closed, only handle events inside the target area that might
  // open the menu.
  const gfx::RectF window_bounds(active_window->GetBoundsInScreen());
  gfx::RectF hit_region(window_bounds);
  hit_region.ClampToCenteredSize(kHitRegionSize);
  hit_region.set_y(window_bounds.y());
  if (!multitask_menu_ && !hit_region.Contains(screen_location)) {
    return;
  }

  // Save the window coordinates to pass to the menu.
  gfx::PointF window_location = event->location_f();
  aura::Window::ConvertPointToTarget(target, active_window, &window_location);

  const ui::GestureEventDetails details = event->details();
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (std::fabs(details.scroll_y_hint()) <
          std::fabs(details.scroll_x_hint())) {
        return;
      }
      if (is_drag_active_) {
        return;
      }
      if (details.scroll_y_hint() > 0) {
        // If the menu hasn't been created yet and scroll down begins inside the
        // target area, start a drag.
        MaybeCreateMultitaskMenu(active_window);
        multitask_menu_->BeginDrag(window_location.y(), /*down=*/true);
        event->SetHandled();
        is_drag_active_ = true;
      } else if (details.scroll_y_hint() < 0 && multitask_menu_ &&
                 gfx::RectF(
                     multitask_menu_->widget()->GetWindowBoundsInScreen())
                     .Contains(screen_location)) {
        // If the menu is open and scroll up begins, only handle events inside
        // the menu to avoid consuming scroll events outside the menu.
        multitask_menu_->BeginDrag(window_location.y(), /*down=*/false);
        event->SetHandled();
        is_drag_active_ = true;
      }
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (is_drag_active_) {
        multitask_menu_->UpdateDrag(window_location.y(),
                                    /*down=*/details.scroll_y() > 0);
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_SCROLL_END:
      // If an unsupported gesture is sent, make sure we reset `is_drag_active_`
      // to stop consuming events.
      if (is_drag_active_) {
        multitask_menu_->EndDrag();
        event->SetHandled();
        is_drag_active_ = false;
      }
      break;
    case ui::ET_SCROLL_FLING_START:
      if (!is_drag_active_) {
        return;
      }
      // Normally ET_GESTURE_SCROLL_BEGIN will fire first and have already
      // created the multitask menu, however occasionally ET_SCROLL_FLING_START
      // may fire first (https://crbug.com/821237).
      MaybeCreateMultitaskMenu(active_window);
      multitask_menu_->Animate(details.velocity_y() > 0);
      event->SetHandled();
      is_drag_active_ = false;
      break;
    default:
      break;
  }
}

bool TabletModeMultitaskMenuEventHandler::CanProcessEvent(
    aura::Window* window) const {
  if (!window) {
    return false;
  }

  // If the multitask menu is shown, we can always drag the menu.
  if (multitask_menu_) {
    return true;
  }

  return CanShowMenu(window);
}

void TabletModeMultitaskMenuEventHandler::MaybeCreateMultitaskMenu(
    aura::Window* active_window) {
  if (!multitask_menu_) {
    multitask_menu_ =
        std::make_unique<TabletModeMultitaskMenu>(this, active_window);
    multitask_cue_->DismissCue(/*menu_opened=*/true);
  }
}

}  // namespace ash
