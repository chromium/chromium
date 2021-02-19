// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/full_restore/full_restore_controller.h"

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "base/check_op.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/aura_constants.h"

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

  tablet_mode_observeration_.Observe(Shell::Get()->tablet_mode_controller());
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

void FullRestoreController::SaveAllWindows() {
  auto windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kAllDesks);
  for (int i = 0; i < int{windows.size()}; ++i) {
    WindowState* window_state = WindowState::Get(windows[i]);

    // Flip the index so that larger values are associated with more recently
    // used windows. See SaveWindowImpl for more details.
    const int activation_index = windows.size() - 1 - i;
    SaveWindowImpl(window_state, activation_index);
  }
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
  tablet_mode_observeration_.Reset();
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
    // The returned MRU list has the more recently used windows have a smaller
    // index. We want to flip it so that larger values are associated with
    // more recently used windows instead. This is because when creating
    // windows, we will add them into a aura::Window hierarichy, whose
    // children are stacked such that the highest indexed children are stacked
    // closest to the top. We reverse the order here since we have access to
    // the full MRU list, as opposed to on read when we will not have enough
    // info when the first couple of windows get added. Due to there being
    // equal or more windows in the MRU list than app windows, the activation
    // indexes may not be consecutive, but the relative order will be correct.
    auto windows =
        Shell::Get()->mru_window_tracker()->BuildMruWindowList(kAllDesks);
    std::reverse(windows.begin(), windows.end());
    auto it = std::find(windows.begin(), windows.end(), window);
    if (it != windows.end())
      window_activation_index = it - windows.begin();
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
  window_info.restore_bounds = window_state->GetRestoreBoundsInScreen();
  window_info.current_bounds = window->GetBoundsInScreen();
  window_info.window_state_type = window_state->GetStateType();
  full_restore::SaveWindowInfo(window_info);

  if (g_save_window_callback_for_testing)
    g_save_window_callback_for_testing.Run(window_info);
}

void FullRestoreController::SetSaveWindowCallbackForTesting(
    SaveWindowCallback callback) {
  g_save_window_callback_for_testing = std::move(callback);
}

}  // namespace ash
