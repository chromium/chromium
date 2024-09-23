// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_toggle_fullscreen_event_handler.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"

namespace ash {
namespace {

// The height of the area in which a touch operation leads to exiting the
// full screen mode.
constexpr int kLeaveFullScreenAreaHeightInPixel = 2;

}  // namespace

TabletModeToggleFullscreenEventHandler::
    TabletModeToggleFullscreenEventHandler() {
  Shell::Get()->AddPreTargetHandler(this);
}

TabletModeToggleFullscreenEventHandler::
    ~TabletModeToggleFullscreenEventHandler() {
  ResetDragData();
  Shell::Get()->RemovePreTargetHandler(this);
}

void TabletModeToggleFullscreenEventHandler::OnTouchEvent(
    ui::TouchEvent* event) {
  if (ProcessEvent(*event)) {
    event->SetHandled();
    event->StopPropagation();
  }
}

void TabletModeToggleFullscreenEventHandler::OnWindowDestroying(
    aura::Window* window) {
  DCHECK(drag_data_);
  DCHECK_EQ(drag_data_->window, window);
  ResetDragData();
}

bool TabletModeToggleFullscreenEventHandler::ProcessEvent(
    const ui::TouchEvent& event) {
  switch (event.type()) {
    case ui::EventType::kTouchPressed: {
      // Another drag is already underway from another finger.
      if (drag_data_) {
        return false;
      }

      aura::Window* active_window = window_util::GetActiveWindow();
      if (!active_window || !CanToggleFullscreen(active_window))
        return false;

      const int y = event.y();
      // For touch press events only process the ones on the top or bottom
      // lines.
      if (y >= kLeaveFullScreenAreaHeightInPixel &&
          y < (active_window->bounds().height() -
               kLeaveFullScreenAreaHeightInPixel)) {
        return false;
      }

      drag_data_ = DragData{.start_y_location = y, .window = active_window};
      active_window->AddObserver(this);
      return true;
    }
    case ui::EventType::kTouchReleased: {
      if (!drag_data_)
        return false;

      // Toggle fullscreen if dragged enough and the window can still be
      // fullscreened.
      const int drag_threshold =
          ShelfConfig::Get()->shelf_size() *
          ShelfConfig::Get()->drag_hide_ratio_threshold();
      if (abs(event.y() - drag_data_->start_y_location) > drag_threshold &&
          CanToggleFullscreen(drag_data_->window)) {
        WMEvent toggle_fullscreen(WM_EVENT_TOGGLE_FULLSCREEN);
        WindowState::Get(drag_data_->window)->OnWMEvent(&toggle_fullscreen);
      }

      ResetDragData();
      return true;
    }
    case ui::EventType::kTouchMoved:
      return drag_data_.has_value();
    case ui::EventType::kTouchCancelled: {
      const bool drag_in_progress = drag_data_.has_value();
      ResetDragData();
      return drag_in_progress;
    }
    default:
      break;
  }

  NOTREACHED();
}

bool TabletModeToggleFullscreenEventHandler::CanToggleFullscreen(
    const aura::Window* window) {
  DCHECK(window);

  const SessionControllerImpl* controller = Shell::Get()->session_controller();
  if (controller->IsScreenLocked() ||
      controller->GetSessionState() != session_manager::SessionState::ACTIVE) {
    return false;
  }

  // Find the active window (from the primary screen) to un-fullscreen.
  aura::Window* active_window = window_util::GetActiveWindow();
  if (window != active_window)
    return false;

  const WindowState* window_state = WindowState::Get(window);
  if (!window_state->IsFullscreen() || window_state->IsInImmersiveFullscreen())
    return false;

  // Do not exit fullscreen in kiosk app mode.
  if (Shell::Get()->session_controller()->IsRunningInAppMode())
    return false;

  return true;
}

void TabletModeToggleFullscreenEventHandler::ResetDragData() {
  if (drag_data_)
    drag_data_->window->RemoveObserver(this);
  drag_data_.reset();
}

}  // namespace ash
