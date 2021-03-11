// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/full_restore/full_restore_controller.h"

#include <cstdint>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "base/check_op.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

FullRestoreController* g_instance = nullptr;

// Callback for testing which is run when SaveWindowImpl triggers a write to
// file.
FullRestoreController::SaveWindowCallback g_save_window_callback_for_testing;

// The list of possible app window parents.
// TODO(crbug.com/1164472): Support the rest of the desk containers which
// are currently not always created depending on whether the bento feature
// is enabled.
constexpr ShellWindowId kAppParentContainers[5] = {
    kShellWindowId_DefaultContainerDeprecated,
    kShellWindowId_DeskContainerB,
    kShellWindowId_DeskContainerC,
    kShellWindowId_DeskContainerD,
    kShellWindowId_AlwaysOnTopContainer,
};

// The types of apps currently supported by full restore.
// TODO(crbug.com/1164472): Add ARC apps when they are supported.
// TODO(crbug.com/1164472): Checking app type is temporary solution until we
// can get windows which are allowed to full restore from the
// FullRestoreService.
constexpr AppType kSupportedAppTypes[2] = {AppType::BROWSER,
                                           AppType::CHROME_APP};

}  // namespace

FullRestoreController::FullRestoreController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;

  tablet_mode_observation_.Observe(Shell::Get()->tablet_mode_controller());
  full_restore_info_observation_.Observe(
      full_restore::FullRestoreInfo::GetInstance());
}

FullRestoreController::~FullRestoreController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FullRestoreController* FullRestoreController::Get() {
  return g_instance;
}

void FullRestoreController::SaveWindow(WindowState* window_state) {
  SaveWindowImpl(window_state, /*activation_index=*/base::nullopt);
}

void FullRestoreController::OnWindowActivated(aura::Window* gained_active) {
  DCHECK(gained_active);

  // Once a window gains activation, it can be cleared of its activation index
  // key since it is no longer used in the stacking algorithm.
  gained_active->ClearProperty(full_restore::kActivationIndexKey);

  SaveAllWindows();
}

void FullRestoreController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  // TODO(crbug.com/1164472): Register and the check the pref service.
}

void FullRestoreController::OnTabletModeStarted() {
  SaveAllWindows();
}

void FullRestoreController::OnTabletModeEnded() {
  SaveAllWindows();
}

void FullRestoreController::OnTabletControllerDestroyed() {
  tablet_mode_observation_.Reset();
}

void FullRestoreController::OnAppLaunched(aura::Window* window) {}

void FullRestoreController::OnWindowInitialized(aura::Window* window) {
  DCHECK(window);

  int32_t* activation_index =
      window->GetProperty(full_restore::kActivationIndexKey);
  if (!activation_index)
    return;

  // Window that are launched from full restore are not activatable initially to
  // prevent them from taking activation when Widget::Show() is called. Make
  // these windows activatable once they are launched. Use a post task since it
  // is quite common for some widgets to explicitly call Show() after
  // initialized.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](aura::Window* window) {
                       views::Widget* widget =
                           views::Widget::GetWidgetForNativeView(window);
                       DCHECK(widget);
                       widget->widget_delegate()->SetCanActivate(true);
                     },
                     window));
}

void FullRestoreController::SaveAllWindows() {
  auto mru_windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kAllDesks);
  for (int i = 0; i < int{mru_windows.size()}; ++i) {
    // Provide the activation index here since we need to loop through |windows|
    // anyhow. Otherwise we need to loop again to get the same value in
    // SaveWindowImpl().
    WindowState* window_state = WindowState::Get(mru_windows[i]);
    SaveWindowImpl(window_state, /*activation_index=*/i);
  }
}

void FullRestoreController::SaveWindowImpl(
    WindowState* window_state,
    base::Optional<int> activation_index) {
  DCHECK(window_state);
  aura::Window* window = window_state->window();

  if (!base::Contains(kAppParentContainers, window->parent()->id()))
    return;

  // Only some app types can be saved.
  if (!base::Contains(
          kSupportedAppTypes,
          static_cast<AppType>(window->GetProperty(aura::client::kAppType)))) {
    return;
  }

  int window_activation_index;
  if (activation_index) {
    window_activation_index = *activation_index;
  } else {
    auto mru_windows =
        Shell::Get()->mru_window_tracker()->BuildMruWindowList(kAllDesks);
    auto it = std::find(mru_windows.begin(), mru_windows.end(), window);
    if (it != mru_windows.end())
      window_activation_index = it - mru_windows.begin();
  }

  full_restore::WindowInfo window_info;
  window_info.activation_index = window_activation_index;
  window_info.window = window;
  window_info.desk_id = window->GetProperty(aura::client::kWindowWorkspaceKey);
  if (window->GetProperty(aura::client::kVisibleOnAllWorkspacesKey)) {
    // Only save |visible_on_all_workspaces| field if it's true to reduce file
    // storage size.
    window_info.visible_on_all_workspaces = true;
  }
  // If there are restore bounds, use those as current bounds. On restore, for
  // states with restore bounds (maximized, minimized, snapped, etc), they will
  // take the current bounds as their restore bounds and have the current bounds
  // determined by the system.
  window_info.current_bounds = window_state->HasRestoreBounds()
                                   ? window_state->GetRestoreBoundsInScreen()
                                   : window->GetBoundsInScreen();
  window_info.window_state_type = window_state->GetStateType();
  window_info.display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
  full_restore::SaveWindowInfo(window_info);

  if (g_save_window_callback_for_testing)
    g_save_window_callback_for_testing.Run(window_info);
}

void FullRestoreController::SetSaveWindowCallbackForTesting(
    SaveWindowCallback callback) {
  g_save_window_callback_for_testing = std::move(callback);
}

}  // namespace ash
