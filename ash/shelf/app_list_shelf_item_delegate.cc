// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/app_list_shelf_item_delegate.h"

#include <algorithm>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/home_launcher_gesture_handler.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ui/display/manager/display_manager.h"

namespace ash {

AppListShelfItemDelegate::AppListShelfItemDelegate()
    : ShelfItemDelegate(ShelfID(kAppListId)) {}

AppListShelfItemDelegate::~AppListShelfItemDelegate() = default;

void AppListShelfItemDelegate::ItemSelected(std::unique_ptr<ui::Event> event,
                                            int64_t display_id,
                                            ShelfLaunchSource source,
                                            ItemSelectedCallback callback) {
  if (!Shell::Get()
           ->app_list_controller()
           ->IsHomeLauncherEnabledInTabletMode()) {
    Shell::Get()->app_list_controller()->ToggleAppList(
        display_id, app_list::kShelfButton, event->time_stamp());
    std::move(callback).Run(SHELF_ACTION_APP_LIST_SHOWN, base::nullopt);
    return;
  }

  // Whether the this action is handled.
  bool handled = false;

  HomeLauncherGestureHandler* home_launcher_gesture_handler =
      Shell::Get()->app_list_controller()->home_launcher_gesture_handler();
  if (home_launcher_gesture_handler) {
    handled = home_launcher_gesture_handler->ShowHomeLauncher(
        Shell::Get()->display_manager()->GetDisplayForId(display_id));
  }

  if (!handled) {
    if (Shell::Get()->window_selector_controller()->IsSelecting()) {
      // End overview mode.
      Shell::Get()->window_selector_controller()->ToggleOverview(
          WindowSelector::EnterExitOverviewType::kWindowsMinimized);
      handled = true;
    }
    if (Shell::Get()->split_view_controller()->IsSplitViewModeActive()) {
      // End split view mode.
      Shell::Get()->split_view_controller()->EndSplitView(
          SplitViewController::EndReason::kHomeLauncherPressed);
      handled = true;
    }
  }

  if (!handled) {
    // Minimize all windows that aren't the app list in reverse order to
    // preserve the mru ordering.
    aura::Window* app_list_container =
        Shell::Get()->GetPrimaryRootWindow()->GetChildById(
            kShellWindowId_AppListTabletModeContainer);
    aura::Window::Windows windows =
        Shell::Get()->mru_window_tracker()->BuildWindowForCycleList();
    std::reverse(windows.begin(), windows.end());
    for (auto* window : windows) {
      if (!app_list_container->Contains(window) &&
          !wm::GetWindowState(window)->IsMinimized()) {
        wm::GetWindowState(window)->Minimize();
        handled = true;
      }
    }
  }

  // Perform the "back" action for the app list.
  if (!handled)
    Shell::Get()->app_list_controller()->Back();

  std::move(callback).Run(SHELF_ACTION_APP_LIST_SHOWN, base::nullopt);
}

void AppListShelfItemDelegate::ExecuteCommand(bool from_context_menu,
                                              int64_t command_id,
                                              int32_t event_flags,
                                              int64_t display_id) {
  // This delegate does not show custom context or application menu items.
  NOTIMPLEMENTED();
}

void AppListShelfItemDelegate::Close() {}

}  // namespace ash
