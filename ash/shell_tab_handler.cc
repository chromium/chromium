// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_tab_handler.h"

#include "ash/capture_mode/capture_mode_util.h"
#include "ash/focus_cycler.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_util.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

void ShellTabHandler::OnKeyEvent(ui::KeyEvent* key_event) {
  // Only focus the shelf if the device is in clamshell mode, and the user
  // pressed tab.
  if (key_event->key_code() != ui::KeyboardCode::VKEY_TAB ||
      key_event->type() != ui::EventType::kKeyPressed ||
      key_event->IsAltDown() || key_event->IsControlDown() ||
      key_event->IsCommandDown() ||
      display::Screen::GetScreen()->InTabletMode()) {
    return;
  }

  // Capture session will process their own tab events.
  if (capture_mode_util::IsCaptureModeActive())
    return;

  aura::Window* root_window_for_new_windows =
      Shell::GetRootWindowForNewWindows();

  if (!root_window_for_new_windows || window_util::GetActiveWindow())
    return;

  // If there is no active window, focus the HomeButton or StatusWidget,
  // depending on whether this is Tab or Shift + Tab. This will allow the
  // users focus to traverse the shelf.
  auto* shelf = Shelf::ForWindow(root_window_for_new_windows);
  views::Widget* status_area_widget = shelf->status_area_widget();
  views::Widget* navigation_widget = shelf->navigation_widget();
  shell_->focus_cycler()->FocusWidget(
      key_event->IsShiftDown() ? status_area_widget : navigation_widget);
  key_event->SetHandled();
  key_event->StopPropagation();
}

}  // namespace ash
