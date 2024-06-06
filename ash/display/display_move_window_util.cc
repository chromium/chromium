// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_move_window_util.h"

#include <stdint.h>
#include <algorithm>
#include <array>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_util.h"
#include "base/containers/contains.h"
#include "base/metrics/user_metrics.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/util/display_util.h"

namespace ash {

namespace display_move_window_util {

namespace {

aura::Window* GetTargetWindow() {
  aura::Window* window = window_util::GetActiveWindow();
  if (!window)
    return nullptr;

  // If |window| is transient window, move its first non-transient
  // transient-parent window instead.
  if (::wm::GetTransientParent(window)) {
    while (::wm::GetTransientParent(window))
      window = ::wm::GetTransientParent(window);
    if (window == window->GetRootWindow())
      return nullptr;
  }
  return window;
}

}  // namespace

bool CanHandleMoveActiveWindowBetweenDisplays() {
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  // Accelerators to move window between displays on unified desktop mode and
  // mirror mode is disabled.
  if (display_manager->IsInUnifiedMode() || display_manager->IsInMirrorMode())
    return false;

  if (display::Screen::GetScreen()->GetNumDisplays() < 2)
    return false;

  // The movement target window must be in window cycle list.
  return base::Contains(
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk),
      GetTargetWindow());
}

void HandleMoveActiveWindowBetweenDisplays() {
  DCHECK(CanHandleMoveActiveWindowBetweenDisplays());
  aura::Window* window = GetTargetWindow();
  DCHECK(window);

  int64_t origin_display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
  auto displays = display::Screen::GetScreen()->GetAllDisplays();
  display::DisplayIdList display_id_list =
      display::CreateDisplayIdList(displays);
  // Find target display id in sorted display id list in a cycling way.
  auto itr = std::upper_bound(display_id_list.begin(), display_id_list.end(),
                              origin_display_id, display::CompareDisplayIds);
  int64_t target_display_id =
      itr == display_id_list.end() ? display_id_list[0] : *itr;
  window_util::MoveWindowToDisplay(window, target_display_id);
  Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(
      AccessibilityAlert::WINDOW_MOVED_TO_ANOTHER_DISPLAY);
  base::RecordAction(
      base::UserMetricsAction("Accel_Move_Active_Window_Between_Displays"));
}

}  // namespace display_move_window_util

}  // namespace ash
