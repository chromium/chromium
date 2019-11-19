// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands.h"

#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"

namespace ash {
namespace accelerators {

bool ZoomDisplay(bool up) {
  if (up)
    base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Up"));
  else
    base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Down"));

  display::DisplayManager* display_manager = Shell::Get()->display_manager();

  gfx::Point point = display::Screen::GetScreen()->GetCursorScreenPoint();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(point);
  return display_manager->ZoomDisplay(display.id(), up);
}

void ResetDisplayZoom() {
  base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Reset"));
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  gfx::Point point = display::Screen::GetScreen()->GetCursorScreenPoint();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(point);
  display_manager->ResetDisplayZoom(display.id());
}

bool ToggleMinimized() {
  aura::Window* window = window_util::GetActiveWindow();
  // Attempt to restore the window that would be cycled through next from
  // the launcher when there is no active window.
  if (!window) {
    // Do not unminimize a window on an inactive desk, since this will cause
    // desks to switch and that will be unintentional for the user.
    MruWindowTracker::WindowList mru_windows(
        Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk));
    if (!mru_windows.empty())
      WindowState::Get(mru_windows.front())->Activate();
    return true;
  }
  WindowState* window_state = WindowState::Get(window);
  if (!window_state->CanMinimize())
    return false;
  window_state->Minimize();
  return true;
}

void ToggleMaximized() {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return;
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Maximized"));
  WMEvent event(WM_EVENT_TOGGLE_MAXIMIZE);
  WindowState::Get(active_window)->OnWMEvent(&event);
}

void ToggleFullscreen() {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return;
  const WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(active_window)->OnWMEvent(&event);
}

bool CanUnpinWindow() {
  // WindowStateType::kTrustedPinned does not allow the user to press a key to
  // exit pinned mode.
  WindowState* window_state = WindowState::ForActiveWindow();
  return window_state &&
         window_state->GetStateType() == WindowStateType::kPinned;
}

void UnpinWindow() {
  aura::Window* pinned_window =
      Shell::Get()->screen_pinning_controller()->pinned_window();
  if (pinned_window)
    WindowState::Get(pinned_window)->Restore();
}

}  // namespace accelerators
}  // namespace ash
