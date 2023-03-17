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

// The dimensions of the area that can activate the multitask menu.
constexpr gfx::SizeF kTargetAreaSize(200.f, 16.f);

// The minimum distance a touch has to be moved by before it is considered to be
// a drag on the menu.
constexpr int kDragYThreshold = 8;

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

void TabletModeMultitaskMenuEventHandler::OnTouchEvent(ui::TouchEvent* event) {
  // The event target may be the active window, multitask menu, or multitask
  // cue, so convert to screen coordinates for consistency.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  gfx::PointF screen_location = event->location_f();
  wm::ConvertPointToScreen(target, &screen_location);

  switch (event->type()) {
    case ui::ET_TOUCH_PRESSED: {
      aura::Window* active_window = window_util::GetActiveWindow();
      // Only process events on the active window, since the target might not
      // be the active window yet and we don't want to handle before window
      // activation.
      if (!CanProcessEvent(active_window)) {
        initial_drag_data_.reset();
        return;
      }
      const gfx::RectF window_bounds(active_window->GetBoundsInScreen());
      gfx::RectF target_area(window_bounds);
      target_area.ClampToCenteredSize(kTargetAreaSize);
      target_area.set_y(window_bounds.y());
      if (!multitask_menu_ && target_area.Contains(screen_location)) {
        // On the first touch, we don't know yet if this touch will turn into a
        // drag, but mark `SetHandled()` to avoid turning into a long press.
        initial_drag_data_ =
            InitialDragData{screen_location, /*is_drag=*/false};
        event->SetHandled();
      }
      if (multitask_menu_ &&
          gfx::RectF(multitask_menu_->widget()->GetWindowBoundsInScreen())
              .Contains(screen_location)) {
        initial_drag_data_ =
            InitialDragData{screen_location, /*is_drag=*/false};
        // Do not mark `SetHandled()` since the press may be on a button.
      }
      break;
    }
    case ui::ET_TOUCH_MOVED: {
      if (!initial_drag_data_) {
        return;
      }
      const gfx::Vector2dF scroll =
          screen_location - initial_drag_data_->initial_location;
      if (multitask_menu_ && std::fabs(scroll.y()) < kDragYThreshold) {
        // The scroll might not have moved enough for us to process a drag yet.
        return;
      }
      const bool down = scroll.y() >= 0;
      // Save the window coordinates to pass to the menu. For us to arrive here
      // the event target must be the active window now.
      gfx::PointF window_location = event->location_f();
      aura::Window* active_window = window_util::GetActiveWindow();
      aura::Window::ConvertPointToTarget(target, active_window,
                                         &window_location);

      if (!multitask_menu_) {
        if (!down) {
          // If no menu is shown and we drag up, do nothing.
          initial_drag_data_.reset();
          return;
        }
        MaybeCreateMultitaskMenu(active_window);
      }
      if (!initial_drag_data_->is_drag) {
        // If this is the first move after the touch, begin a drag. Note that
        // `initial_location` was saved in screen coordinates, so we must
        // convert to window coordinates to pass to the menu.
        gfx::PointF initial_location = initial_drag_data_->initial_location;
        wm::ConvertPointFromScreen(target, &initial_location);
        multitask_menu_->BeginDrag(initial_location.y(), down);
      }
      multitask_menu_->UpdateDrag(window_location.y(), down);
      event->SetHandled();
      // Reset `initial_drag_data_->is_drag` so we don't handle it in
      // ET_TOUCH_RELEASED.
      initial_drag_data_->is_drag = true;
      break;
    }
    case ui::ET_TOUCH_RELEASED:
      if (!initial_drag_data_) {
        return;
      }
      if (!initial_drag_data_->is_drag) {
        // If the touch was pressed and released immediately without dragging,
        // it may have been a press in the target area and we do not handle
        // here.
        initial_drag_data_.reset();
        return;
      }
      if (multitask_menu_) {
        multitask_menu_->EndDrag();
      }
      initial_drag_data_.reset();
      event->SetHandled();
      break;
    case ui::ET_TOUCH_CANCELLED:
      initial_drag_data_.reset();
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
