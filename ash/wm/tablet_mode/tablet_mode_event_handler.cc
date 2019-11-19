// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_event_handler.h"

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
const int kLeaveFullScreenAreaHeightInPixel = 2;

}  // namespace

TabletModeEventHandler::TabletModeEventHandler() {
  Shell::Get()->AddPreTargetHandler(this);
}

TabletModeEventHandler::~TabletModeEventHandler() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void TabletModeEventHandler::OnTouchEvent(ui::TouchEvent* event) {
  if (ToggleFullscreen(*event))
    event->StopPropagation();
}

bool TabletModeEventHandler::ToggleFullscreen(const ui::TouchEvent& event) {
  if (event.type() != ui::ET_TOUCH_PRESSED)
    return false;

  const SessionControllerImpl* controller = Shell::Get()->session_controller();

  if (controller->IsScreenLocked() ||
      controller->GetSessionState() != session_manager::SessionState::ACTIVE) {
    return false;
  }

  // Find the active window (from the primary screen) to un-fullscreen.
  aura::Window* window = window_util::GetActiveWindow();
  if (!window)
    return false;

  WindowState* window_state = WindowState::Get(window);
  if (!window_state->IsFullscreen() || window_state->IsInImmersiveFullscreen())
    return false;

  // Test that the touch happened in the top or bottom lines.
  int y = event.y();
  if (y >= kLeaveFullScreenAreaHeightInPixel &&
      y < (window->bounds().height() - kLeaveFullScreenAreaHeightInPixel)) {
    return false;
  }

  // Do not exit fullscreen in kiosk app mode.
  if (Shell::Get()->session_controller()->IsRunningInAppMode())
    return false;

  WMEvent toggle_fullscreen(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(window)->OnWMEvent(&toggle_fullscreen);
  return true;
}

}  // namespace ash
