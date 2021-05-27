// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/full_restore/full_restore_controller.h"

#include <cstdint>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

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
constexpr ShellWindowId kAppParentContainers[9] = {
    kShellWindowId_DefaultContainerDeprecated,
    kShellWindowId_DeskContainerB,
    kShellWindowId_DeskContainerC,
    kShellWindowId_DeskContainerD,
    kShellWindowId_DeskContainerE,
    kShellWindowId_DeskContainerF,
    kShellWindowId_DeskContainerG,
    kShellWindowId_DeskContainerH,
    kShellWindowId_AlwaysOnTopContainer,
};

// The types of apps currently supported by full restore.
// TODO(crbug.com/1164472): Checking app type is temporary solution until we
// can get windows which are allowed to full restore from the
// FullRestoreService.
constexpr AppType kSupportedAppTypes[3] = {
    AppType::BROWSER, AppType::CHROME_APP, AppType::ARC_APP};

std::unique_ptr<full_restore::WindowInfo> GetWindowInfo(aura::Window* window) {
  return g_read_window_callback_for_testing
             ? g_read_window_callback_for_testing.Run(window)
             : full_restore::GetWindowInfo(window);
}

// Returns the sibling of `window` that `window` should be stacked below based
// on restored activation indices. Returns nullptr if `window` does not need
// to be moved in the z-ordering. Should be called after `window` is added as
// a child of its parent.
aura::Window* GetSiblingToStackBelow(aura::Window* window) {
  DCHECK(window->parent());
  auto siblings = window->parent()->children();
#if DCHECK_IS_ON()
  // Verify that the activation keys are descending. Non-restored windows may be
  // stacked in certain ways by other window manager features so there may be
  // non-restored windows at any point but the windows that have the
  // `full_restore::kActivationIndexKey` should be in relative descending order.
  absl::optional<int32_t> last_activation_key;
  for (size_t i = 0; i < siblings.size(); ++i) {
    // The current window needs to be stacked, so there is a chance it is
    // initially out of order.
    if (window == siblings[i])
      continue;

    int32_t* current_activation_key =
        siblings[i]->GetProperty(full_restore::kActivationIndexKey);
    if (!current_activation_key)
      continue;

    if (last_activation_key)
      DCHECK_LT(*current_activation_key, *last_activation_key);
    last_activation_key = *current_activation_key;
  }
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

// If `window`'s saved window info makes the `window` out-of-bounds for the
// display, manually restore its bounds. Also ensures that at least 30% of the
// window is visible to handle the case where the display a window is restored
// to is drastically smaller than the pre-restore display.
void MaybeRestoreOutOfBoundsWindows(aura::Window* window) {
  std::unique_ptr<full_restore::WindowInfo> window_info = GetWindowInfo(window);
  if (!window_info)
    return;

  gfx::Rect current_bounds =
      window_info->current_bounds.value_or(gfx::Rect(0, 0));
  if (current_bounds.IsEmpty())
    return;

  const auto& closest_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  gfx::Rect display_area = closest_display.work_area();
  if (display_area.Contains(current_bounds))
    return;

  AdjustBoundsToEnsureMinimumWindowVisibility(display_area, &current_bounds);

  auto* window_state = WindowState::Get(window);
  if (window_state->HasRestoreBounds()) {
    // When a `window` is in maximized, minimized, or snapped its restore bounds
    // are saved in `WindowInfo.current_bounds` and its
    // maximized/minimized/snapped bounds are determined by the system, so apply
    // this adjustment to `window`'s restore bounds instead.
    window_state->SetRestoreBoundsInScreen(current_bounds);
  } else {
    window->SetBoundsInScreen(current_bounds, closest_display);
  }
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

// static
bool FullRestoreController::CanActivateAppList(const aura::Window* window) {
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  if (!tablet_mode_controller || !tablet_mode_controller->InTabletMode())
    return true;

  auto* app_list_controller = Shell::Get()->app_list_controller();
  if (!app_list_controller || app_list_controller->GetWindow() != window)
    return true;

  for (auto* root_window : Shell::GetAllRootWindows()) {
    auto active_desk_children =
        desks_util::GetActiveDeskContainerForRoot(root_window)->children();

    // Find the topmost unminimized window.
    auto topmost_visible_iter = active_desk_children.rbegin();
    while (topmost_visible_iter != active_desk_children.rend() &&
           WindowState::Get(*topmost_visible_iter)->IsMinimized()) {
      topmost_visible_iter = std::next(topmost_visible_iter);
    }

    if (topmost_visible_iter != active_desk_children.rend() &&
        (*topmost_visible_iter)
            ->GetProperty(full_restore::kLaunchedFromFullRestoreKey)) {
      DCHECK(features::IsFullRestoreEnabled());
      return false;
    }
  }

  return true;
}

void FullRestoreController::SaveWindow(WindowState* window_state) {
  SaveWindowImpl(window_state, /*activation_index=*/absl::nullopt);
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

void FullRestoreController::OnWidgetInitialized(views::Widget* widget) {
  DCHECK(widget);

  aura::Window* window = widget->GetNativeWindow();
  if (window->GetProperty(full_restore::kParentToHiddenContainerKey))
    return;

  UpdateAndObserveWindow(window);

  // If the restored bounds are out of the screen, move the window to the bounds
  // manually as most widget types force windows to be within the work area on
  // creation.
  // TODO(chinsenj|sammiequon): The Files app uses async Mojo calls to activate
  // and set its bounds, making this approach not work. In the future, we'll
  // need to address the Files app.
  MaybeRestoreOutOfBoundsWindows(window);
}

void FullRestoreController::OnARCTaskReadyForUnparentedWindow(
    aura::Window* window) {
  DCHECK(window);
  DCHECK(window->GetProperty(full_restore::kParentToHiddenContainerKey));

  std::unique_ptr<full_restore::WindowInfo> window_info = GetWindowInfo(window);
  if (window_info) {
    const int desk_id = window_info->desk_id
                            ? int{*window_info->desk_id}
                            : aura::client::kUnassignedWorkspace;
    window->SetProperty(aura::client::kWindowWorkspaceKey, desk_id);
    window->SetProperty(aura::client::kVisibleOnAllWorkspacesKey,
                        window_info->visible_on_all_workspaces.has_value());
  }

  // Now that the hidden container key is cleared,
  // `aura::client::ParentWindowWithContext` should parent `window` to a valid
  // desk container.
  window->SetProperty(full_restore::kParentToHiddenContainerKey, false);
  aura::client::ParentWindowWithContext(window,
                                        /*context=*/window->GetRootWindow(),
                                        window->GetBoundsInScreen());

  UpdateAndObserveWindow(window);
}

void FullRestoreController::OnWindowStackingChanged(aura::Window* window) {
  DCHECK(windows_observation_.IsObservingSource(window));

  // Do nothing if stacking was triggered by us.
  if (is_stacking_)
    return;

  // Once a window has its stacking changed, possibly by another window
  // management feature, it can be cleared of its activation index
  // key since it is no longer used in the stacking algorithm.
  window->ClearProperty(full_restore::kActivationIndexKey);
}

void FullRestoreController::OnWindowVisibilityChanged(aura::Window* window,
                                                      bool visible) {
  // `OnWindowVisibilityChanged` fires for children of a window as well, but we
  // are only interested in the window we originally observed.
  if (!windows_observation_.IsObservingSource(window))
    return;

  if (!visible || !to_be_shown_windows_.contains(window))
    return;

  to_be_shown_windows_.erase(window);

  RestoreStateTypeAndClearLaunchedKey(window);

  // Early return if we're not in tablet mode, or the app list is null.
  aura::Window* app_list_window =
      Shell::Get()->app_list_controller()->GetWindow();
  if (!Shell::Get()->tablet_mode_controller()->InTabletMode() ||
      !app_list_window) {
    return;
  }

  // Because windows are shown inactive, they don't take focus/activation. This
  // can lead to situations in tablet mode where the app list is active and
  // visibly below restored windows. This causes the hotseat widget to not be
  // hidden, so deactivate the app list. See crbug.com/1202923.
  auto* app_list_widget =
      views::Widget::GetWidgetForNativeWindow(app_list_window);
  if (app_list_widget->IsActive() && WindowState::Get(window)->IsMaximized())
    app_list_widget->Deactivate();
}

void FullRestoreController::UpdateAndObserveWindow(aura::Window* window) {
  DCHECK(window);
  DCHECK(window->parent());
  windows_observation_.AddObservation(window);

  // Unless minimized, snap state and activation unblock are done when the
  // window is first shown, which will be async for exo apps.
  if (WindowState::Get(window)->IsMinimized())
    window->SetProperty(full_restore::kLaunchedFromFullRestoreKey, false);
  else
    to_be_shown_windows_.insert(window);

  int32_t* activation_index =
      window->GetProperty(full_restore::kActivationIndexKey);
  if (!activation_index)
    return;

  // Stack the window.
  auto* target_sibling = GetSiblingToStackBelow(window);
  if (target_sibling) {
    base::AutoReset<bool> auto_reset_is_stacking(&is_stacking_, true);
    window->parent()->StackChildBelow(window, target_sibling);
  }
}

void FullRestoreController::OnWindowDestroying(aura::Window* window) {
  DCHECK(windows_observation_.IsObservingSource(window));
  windows_observation_.RemoveObservation(window);

  if (base::Contains(restore_property_clear_callbacks_, window))
    ClearLaunchedKey(window, /*is_destroying=*/true);
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
    absl::optional<int> activation_index) {
  DCHECK(window_state);
  aura::Window* window = window_state->window();

  // Only apps whose parent is a certain container can be saved.
  if (!window->parent() ||
      !base::Contains(kAppParentContainers, window->parent()->GetId())) {
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

  // If override bounds and window state are available (in tablet mode), save
  // those bounds.
  gfx::Rect* override_bounds = window->GetProperty(kRestoreBoundsOverrideKey);
  if (override_bounds) {
    window_info.current_bounds = *override_bounds;
    // Snapped state can be restored from tablet onto clamshell, so we do not
    // use the restore override state here.
    window_info.window_state_type =
        window_state->IsSnapped()
            ? window_state->GetStateType()
            : window->GetProperty(kRestoreWindowStateTypeOverrideKey);
  } else {
    // If there are restore bounds, use those as current bounds. On restore, for
    // states with restore bounds (maximized, minimized, snapped, etc), they
    // will take the current bounds as their restore bounds and have the current
    // bounds determined by the system.
    window_info.current_bounds = window_state->HasRestoreBounds()
                                     ? window_state->GetRestoreBoundsInScreen()
                                     : window->GetBoundsInScreen();
    // Full restore does not support restoring fullscreen windows. If a window
    // is fullscreen save the pre-fullscreen window state instead.
    window_info.window_state_type =
        window_state->IsFullscreen()
            ? chromeos::ToWindowStateType(
                  window->GetProperty(aura::client::kPreFullscreenShowStateKey))
            : window_state->GetStateType();
  }

  window_info.display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();

  // Save window size restriction of ARC app window.
  if (IsArcWindow(window)) {
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    if (widget) {
      auto extra = full_restore::WindowInfo::ArcExtraInfo();
      extra.maximum_size = widget->GetMaximumSize();
      extra.minimum_size = widget->GetMinimumSize();
      window_info.arc_extra_info = extra;
    }
  }

  full_restore::SaveWindowInfo(window_info);

  if (g_save_window_callback_for_testing)
    g_save_window_callback_for_testing.Run(window_info);
}

void FullRestoreController::RestoreStateTypeAndClearLaunchedKey(
    aura::Window* window) {
  std::unique_ptr<full_restore::WindowInfo> window_info =
      g_read_window_callback_for_testing
          ? g_read_window_callback_for_testing.Run(window)
          : full_restore::GetWindowInfo(window);
  if (window_info) {
    // Snap the window if necessary.
    auto state_type = window_info->window_state_type;
    if (state_type) {
      // Add the window to be tracked by the tablet mode window manager
      // manually. It is normally tracked when it becomes visible, but in snap
      // case we want to track it before it becomes visible. This will allow us
      // to snap the window before it is shown and skip first showing the window
      // in normal or maximized state.
      // TODO(crbug.com/1164472): Investigate splitview for ARC apps, which
      // are not managed by TabletModeWindowManager.
      if (Shell::Get()->tablet_mode_controller()->InTabletMode())
        Shell::Get()->tablet_mode_controller()->AddWindow(window);

      if (*state_type == chromeos::WindowStateType::kLeftSnapped ||
          *state_type == chromeos::WindowStateType::kRightSnapped) {
        base::AutoReset<bool> auto_reset_is_restoring_snap_state(
            &is_restoring_snap_state_, true);
        const WMEvent snap_event(*state_type ==
                                         chromeos::WindowStateType::kLeftSnapped
                                     ? WM_EVENT_SNAP_LEFT
                                     : WM_EVENT_SNAP_RIGHT);
        WindowState::Get(window)->OnWMEvent(&snap_event);
      }
    }
  }

  // Window that are launched from full restore are not activatable initially to
  // prevent them from taking activation when Widget::Show() is called. Make
  // these windows activatable once they are launched. Use a post task since it
  // is quite common for some widgets to explicitly call Show() after
  // initialized.
  // TODO(sammiequon): Instead of disabling activation when creating the widget
  // and enabling it here, use ShowInactive() instead of Show() when the widget
  // is created.
  restore_property_clear_callbacks_.emplace(
      window, base::BindOnce(&FullRestoreController::ClearLaunchedKey,
                             weak_ptr_factory_.GetWeakPtr(), window,
                             /*is_destroying=*/false));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, restore_property_clear_callbacks_[window].callback());
}

void FullRestoreController::ClearLaunchedKey(aura::Window* window,
                                             bool is_destroying) {
  DCHECK(window);
  DCHECK(base::Contains(restore_property_clear_callbacks_, window));

  restore_property_clear_callbacks_[window].Cancel();
  restore_property_clear_callbacks_.erase(window);

  // If the window is destroying then prevent extra work by not clearing the
  // property.
  if (!is_destroying)
    window->SetProperty(full_restore::kLaunchedFromFullRestoreKey, false);
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
