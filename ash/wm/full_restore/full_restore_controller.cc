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
#include "ash/wm/wm_event.h"
#include "base/check_op.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

FullRestoreController* g_instance = nullptr;

// Callback for testing which is run when `OnWidgetInitialized()` triggers a
// read from file.
FullRestoreController::ReadWindowCallback g_read_window_callback_for_testing;

// Callback for testing which is run when `SaveWindowImpl()` triggers a write to
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
// TODO(crbug.com/1164472): Checking app type is temporary solution until we
// can get windows which are allowed to full restore from the
// FullRestoreService.
constexpr AppType kSupportedAppTypes[3] = {
    AppType::BROWSER, AppType::CHROME_APP, AppType::ARC_APP};

// Returns the sibling of `window` that `window` should be stacked below based
// on restored activation indices. Returns nullptr if `window` does not need
// to be moved in the z-ordering. Should be called after `window` is added as
// a child of its parent.
aura::Window* GetSiblingToStackBelow(aura::Window* window) {
  DCHECK(window->parent());
  auto siblings = window->parent()->children();
#if DCHECK_IS_ON()
  // Verify that the activation keys are descending and that non-restored
  // windows are all at the end.
  for (int i = 0; i < int{siblings.size()} - 2; ++i) {
    int32_t* current_activation_key =
        siblings[i]->GetProperty(full_restore::kActivationIndexKey);
    size_t next_index = i + 1;
    int32_t* next_activation_key =
        siblings[next_index]->GetProperty(full_restore::kActivationIndexKey);

    const bool descending_order =
        current_activation_key &&
        (!next_activation_key ||
         *current_activation_key > *next_activation_key);
    const bool both_null = !current_activation_key && !next_activation_key;

    DCHECK(descending_order || both_null);
  }

  DCHECK_EQ(siblings.back(), window);
#endif

  int32_t* restore_activation_key =
      window->GetProperty(full_restore::kActivationIndexKey);
  DCHECK(restore_activation_key);

  for (int i = 0; i < int{siblings.size()} - 1; ++i) {
    int32_t* sibling_restore_activation_key =
        siblings[i]->GetProperty(full_restore::kActivationIndexKey);

    if (!sibling_restore_activation_key ||
        *restore_activation_key > *sibling_restore_activation_key) {
      // Activation index is saved to match MRU order so lower means more
      // recent/higher in stacking order. Also restored windows should be
      // stacked below non-restored windows.
      return siblings[i];
    }
  }

  return nullptr;
}

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

void FullRestoreController::OnWidgetInitialized(views::Widget* widget) {
  DCHECK(widget);

  aura::Window* window = widget->GetNativeWindow();
  DCHECK(window->parent());

  std::unique_ptr<full_restore::WindowInfo> window_info =
      g_read_window_callback_for_testing
          ? g_read_window_callback_for_testing.Run(window)
          : full_restore::GetWindowInfo(window);
  if (window_info) {
    // Snap the window if necessary.
    auto state_type = window_info->window_state_type;
    if (state_type) {
      if (*state_type == chromeos::WindowStateType::kLeftSnapped ||
          *state_type == chromeos::WindowStateType::kRightSnapped) {
        const WMEvent snap_event(*state_type ==
                                         chromeos::WindowStateType::kLeftSnapped
                                     ? WM_EVENT_SNAP_LEFT
                                     : WM_EVENT_SNAP_RIGHT);
        WindowState::Get(window)->OnWMEvent(&snap_event);
      }
    }
  }

  int32_t* activation_index =
      window->GetProperty(full_restore::kActivationIndexKey);
  if (!activation_index)
    return;

  // Window that are launched from full restore are not activatable initially to
  // prevent them from taking activation when Widget::Show() is called. Make
  // these windows activatable once they are launched. Use a post task since it
  // is quite common for some widgets to explicitly call Show() after
  // initialized.
  // TODO(sammiequon): Instead of disabling activation when creating the widget
  // and enabling it here, use ShowInactive() instead of Show() when the widget
  // is created.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](aura::Window* window) {
                       views::Widget* widget =
                           views::Widget::GetWidgetForNativeView(window);
                       DCHECK(widget);
                       widget->widget_delegate()->SetCanActivate(true);
                     },
                     window));

  // Stack the window.
  auto* target_sibling = GetSiblingToStackBelow(window);
  if (target_sibling)
    window->parent()->StackChildBelow(window, target_sibling);
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

  // Only apps whose parent is a certain container can be saved.
  if (!window->parent() ||
      !base::Contains(kAppParentContainers, window->parent()->id())) {
    return;
  }

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

void FullRestoreController::SetReadWindowCallbackForTesting(
    ReadWindowCallback callback) {
  g_read_window_callback_for_testing = std::move(callback);
}

void FullRestoreController::SetSaveWindowCallbackForTesting(
    SaveWindowCallback callback) {
  g_save_window_callback_for_testing = std::move(callback);
}

}  // namespace ash
