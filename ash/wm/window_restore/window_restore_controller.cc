// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/window_restore_controller.h"

#include <cstdint>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

WindowRestoreController* g_instance = nullptr;

// Callback for testing which is run when `SaveWindowImpl()` triggers a write to
// file.
WindowRestoreController::SaveWindowCallback g_save_window_callback_for_testing;

// The list of possible app window parents.
constexpr ShellWindowId kAppParentContainers[19] = {
    kShellWindowId_DeskContainerA,       kShellWindowId_DeskContainerB,
    kShellWindowId_DeskContainerC,       kShellWindowId_DeskContainerD,
    kShellWindowId_DeskContainerE,       kShellWindowId_DeskContainerF,
    kShellWindowId_DeskContainerG,       kShellWindowId_DeskContainerH,
    kShellWindowId_DeskContainerI,       kShellWindowId_DeskContainerJ,
    kShellWindowId_DeskContainerK,       kShellWindowId_DeskContainerL,
    kShellWindowId_DeskContainerM,       kShellWindowId_DeskContainerN,
    kShellWindowId_DeskContainerO,       kShellWindowId_DeskContainerP,
    kShellWindowId_AlwaysOnTopContainer, kShellWindowId_FloatContainer,
    kShellWindowId_UnparentedContainer,
};

// The types of apps currently supported by window restore.
// TODO(crbug.com/40163553): Checking app type is temporary solution until we
// can get windows which are allowed to window restore from the
// FullRestoreService.
constexpr chromeos::AppType kSupportedAppTypes[5] = {
    chromeos::AppType::BROWSER, chromeos::AppType::CHROME_APP,
    chromeos::AppType::ARC_APP, chromeos::AppType::SYSTEM_APP,
    chromeos::AppType::LACROS};

// Delay for certain app types before activation is allowed. This is because
// some apps' client request activation after creation, which can break user
// flow.
constexpr base::TimeDelta kAllowActivationDelay = base::Seconds(2);

app_restore::WindowInfo* GetWindowInfo(aura::Window* window) {
  return window->GetProperty(app_restore::kWindowInfoKey);
}

// If `window`'s saved window info makes the `window` out-of-bounds for the
// display, manually restore its bounds. Also ensures that at least 30% of the
// window is visible to handle the case where the display a window is restored
// to is drastically smaller than the pre-restore display.
void MaybeRestoreOutOfBoundsWindows(aura::Window* window) {
  app_restore::WindowInfo* window_info = GetWindowInfo(window);
  if (!window_info)
    return;

  gfx::Rect current_bounds =
      window_info->current_bounds.value_or(gfx::Rect(0, 0));
  if (current_bounds.IsEmpty())
    return;

  const auto& closest_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  const gfx::Rect display_area = closest_display.work_area();
  if (display_area.Contains(current_bounds))
    return;

  AdjustBoundsToEnsureMinimumWindowVisibility(
      display_area, /*client_controlled=*/false, &current_bounds);

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

// Self deleting class which watches a unparented window and deletes itself once
// the window has a parent.
class ParentChangeObserver : public aura::WindowObserver {
 public:
  ParentChangeObserver(aura::Window* window) {
    DCHECK(!window->parent());
    window_observation_.Observe(window);
  }
  ParentChangeObserver(const ParentChangeObserver&) = delete;
  ParentChangeObserver& operator=(const ParentChangeObserver&) = delete;
  ~ParentChangeObserver() override = default;

  // aura::WindowObserver:
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override {
    if (!parent)
      return;
    WindowRestoreController::Get()->SaveAllWindows();
    delete this;
  }
  void OnWindowDestroying(aura::Window* window) override { delete this; }

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace

WindowRestoreController::WindowRestoreController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;

  app_restore_info_observation_.Observe(
      app_restore::AppRestoreInfo::GetInstance());
}

WindowRestoreController::~WindowRestoreController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
WindowRestoreController* WindowRestoreController::Get() {
  return g_instance;
}

// static
bool WindowRestoreController::CanActivateRestoredWindow(
    const aura::Window* window) {
  if (!window->GetProperty(app_restore::kLaunchedFromAppRestoreKey))
    return true;

  // Only windows on the active desk should be activatable.
  if (!desks_util::BelongsToActiveDesk(const_cast<aura::Window*>(window)))
    return false;

  // Ghost windows can be activated.
  const chromeos::AppType app_type = window->GetProperty(chromeos::kAppTypeKey);
  const bool is_real_arc_window =
      window->GetProperty(app_restore::kRealArcTaskWindow);
  if (app_type == chromeos::AppType::ARC_APP && !is_real_arc_window) {
    return true;
  }

  auto* desk_container = window->parent();
  if (!desk_container || !desks_util::IsDeskContainer(desk_container))
    return true;

  // Only the topmost unminimize restored window can be activated.
  auto siblings = desk_container->children();
  for (aura::Window* const sibling : base::Reversed(siblings)) {
    if (WindowState::Get(sibling)->IsMinimized())
      continue;

    return window == sibling;
  }

  return false;
}

// static
bool WindowRestoreController::CanActivateAppList(const aura::Window* window) {
  if (!display::Screen::GetScreen()->InTabletMode()) {
    return true;
  }

  auto* app_list_controller = Shell::Get()->app_list_controller();
  if (!app_list_controller || app_list_controller->GetWindow() != window)
    return true;

  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
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
            ->GetProperty(app_restore::kLaunchedFromAppRestoreKey)) {
      return false;
    }
  }

  return true;
}

// static
std::vector<raw_ptr<aura::Window, VectorExperimental>>::const_iterator
WindowRestoreController::GetWindowToInsertBefore(
    aura::Window* window,
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows) {
  const int32_t* activation_index =
      window->GetProperty(app_restore::kActivationIndexKey);
  DCHECK(activation_index);

  // If this is an admin template window, it should be placed on top of existing
  // windows (but relative to other desk template windows).
  if (saved_desk_util::IsWindowOnTopForTemplate(window)) {
    for (auto it = windows.begin(); it != windows.end(); ++it) {
      const int32_t* next_activation_index =
          (*it)->GetProperty(app_restore::kActivationIndexKey);
      if (next_activation_index && *activation_index > *next_activation_index) {
        return it;
      }
    }
    return windows.end();
  }

  auto it = windows.begin();
  while (it != windows.end()) {
    const int32_t* next_activation_index =
        (*it)->GetProperty(app_restore::kActivationIndexKey);

    if (!next_activation_index || *activation_index > *next_activation_index) {
      // Activation index is saved to match MRU order so lower means more
      // recent/higher in stacking order. Also restored windows should be
      // stacked below non-restored windows.
      return it;
    }
    it = std::next(it);
  }

  return it;
}

void WindowRestoreController::SaveWindow(WindowState* window_state) {
  SaveWindowImpl(window_state, /*activation_index=*/std::nullopt);
}

void WindowRestoreController::SaveAllWindows() {
  auto mru_windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kAllDesks);
  for (int i = 0; i < static_cast<int>(mru_windows.size()); ++i) {
    // Provide the activation index here since we need to loop through `windows`
    // anyhow. Otherwise we need to loop again to get the same value in
    // `SaveWindowImpl()`.
    WindowState* window_state = WindowState::Get(mru_windows[i]);
    SaveWindowImpl(window_state, /*activation_index=*/i);
  }
}

void WindowRestoreController::OnWindowActivated(aura::Window* gained_active) {
  SaveAllWindows();
}

void WindowRestoreController::OnDisplayTabletStateChanged(
    display::TabletState state) {
  if (display::IsTabletStateChanging(state)) {
    // Do nothing if the tablet state is still in the process of transition.
    return;
  }

  SaveAllWindows();
}

void WindowRestoreController::OnRestorePrefChanged(const AccountId& account_id,
                                                   bool could_restore) {
  if (could_restore)
    SaveAllWindows();
}

void WindowRestoreController::OnAppLaunched(aura::Window* window) {
  // Non ARC windows will already be saved as this point, as this is for cases
  // where an ARC window is created without a task.
  if (!IsArcWindow(window))
    return;

  // Save the window info once the app launched. If `window` does not have a
  // parent yet, there won't be any window state, so create an observer that
  // will save when `window` gets a parent. Save all windows since we need to
  // update the activation index of the other windows.
  if (window->parent()) {
    SaveAllWindows();
    return;
  }

  new ParentChangeObserver(window);
}

void WindowRestoreController::OnWidgetInitialized(views::Widget* widget) {
  CHECK(widget);

  aura::Window* window = widget->GetNativeWindow();
  if (window->GetProperty(app_restore::kParentToHiddenContainerKey)) {
    return;
  }

  if (!window->GetProperty(app_restore::kLaunchedFromAppRestoreKey)) {
    return;
  }

  // Windows with restore window key less than -1 are launched from desk
  // templates or saved desks; we want to stay in overview for these. Windows
  // with restore window key more than -1 are launched from full restore and we
  // want to end overview for these.
  if (window->GetProperty(app_restore::kRestoreWindowIdKey) > -1) {
    OverviewController::Get()->EndOverview(OverviewEndAction::kFullRestore);
  }

  UpdateAndObserveWindow(window);

  // If the restored bounds are out of the screen, move the window to the bounds
  // manually as most widget types force windows to be within the work area on
  // creation.
  MaybeRestoreOutOfBoundsWindows(window);
}

void WindowRestoreController::OnParentWindowToValidContainer(
    aura::Window* window) {
  DCHECK(window);
  DCHECK(window->GetProperty(app_restore::kParentToHiddenContainerKey));

  app_restore::WindowInfo* window_info = GetWindowInfo(window);
  if (window_info) {
    int desk_id = -1;
    if (window_info->desk_guid.is_valid()) {
      desk_id =
          DesksController::Get()->GetDeskIndexByUuid(window_info->desk_guid);
    }
    // Its possible that the uuid is valid but it is not the uuid of any current
    // desk.
    if (desk_id == -1) {
      desk_id = window_info->desk_id
                    ? static_cast<int>(*window_info->desk_id)
                    : aura::client::kWindowWorkspaceUnassignedWorkspace;
    }
    window->SetProperty(aura::client::kWindowWorkspaceKey, desk_id);
  }

  // Now that the hidden container key is cleared,
  // `aura::client::ParentWindowWithContext` should parent `window` to a valid
  // desk container.
  window->SetProperty(app_restore::kParentToHiddenContainerKey, false);
  aura::client::ParentWindowWithContext(window,
                                        /*context=*/window->GetRootWindow(),
                                        window->GetBoundsInScreen(),
                                        display::kInvalidDisplayId);

  UpdateAndObserveWindow(window);
}

void WindowRestoreController::OnWindowPropertyChanged(aura::Window* window,
                                                      const void* key,
                                                      intptr_t old) {
  // If the ARC ghost window becomes ARC app's window, it should be applied
  // the activation delay.
  if (key == app_restore::kRealArcTaskWindow &&
      window->GetProperty(app_restore::kRealArcTaskWindow)) {
    window->SetProperty(app_restore::kLaunchedFromAppRestoreKey, true);
    restore_property_clear_callbacks_.emplace(
        window, base::BindOnce(&WindowRestoreController::ClearLaunchedKey,
                               weak_ptr_factory_.GetWeakPtr(), window));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, restore_property_clear_callbacks_[window].callback(),
        kAllowActivationDelay);
  }

  if (key != app_restore::kLaunchedFromAppRestoreKey ||
      window->GetProperty(app_restore::kLaunchedFromAppRestoreKey)) {
    return;
  }

  // Once this property is cleared, there is no need to observe `window`
  // anymore.
  DCHECK(windows_observation_.IsObservingSource(window));
  windows_observation_.RemoveObservation(window);
  to_be_shown_windows_.erase(window);

  if (base::Contains(restore_property_clear_callbacks_, window))
    CancelAndRemoveRestorePropertyClearCallback(window);
}

void WindowRestoreController::OnWindowVisibilityChanged(aura::Window* window,
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
  if (!Shell::Get()->IsInTabletMode() || !app_list_window) {
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

void WindowRestoreController::OnWindowDestroying(aura::Window* window) {
  DCHECK(windows_observation_.IsObservingSource(window));
  windows_observation_.RemoveObservation(window);

  if (base::Contains(restore_property_clear_callbacks_, window))
    ClearLaunchedKey(window);
}

void WindowRestoreController::UpdateAndObserveWindow(aura::Window* window) {
  DCHECK(window);
  DCHECK(window->parent());

  windows_observation_.AddObservation(window);

  // Unless minimized, snap state and activation unblock are done when the
  // window is first shown, which will be async for exo apps.
  if (WindowState::Get(window)->IsMinimized() || window->IsVisible()) {
    // If the window is already visible, do not wait until it is next visible to
    // restore the state type and clear the launched key.
    RestoreStateTypeAndClearLaunchedKey(window);
  } else {
    to_be_shown_windows_.insert(window);

    // Clear the restore show state key in case for any reason the window
    // did not restore its minimized state.
    window->ClearProperty(aura::client::kRestoreShowStateKey);
  }

  StackWindow(window);
}

void WindowRestoreController::StackWindow(aura::Window* window) {
  int32_t* activation_index =
      window->GetProperty(app_restore::kActivationIndexKey);
  if (!activation_index)
    return;

  Shell::Get()->mru_window_tracker()->OnWindowAlteredByWindowRestore(window);

  // Stack the window.
  auto siblings = window->parent()->children();
  auto insertion_point = GetWindowToInsertBefore(window, siblings);
  if (insertion_point != siblings.end())
    window->parent()->StackChildBelow(window, *insertion_point);
}

bool WindowRestoreController::IsRestoringWindow(aura::Window* window) const {
  return windows_observation_.IsObservingSource(window);
}

void WindowRestoreController::SaveWindowImpl(
    WindowState* window_state,
    std::optional<int> activation_index) {
  DCHECK(window_state);
  aura::Window* window = window_state->window();

  // Skip saving ARC PIP window.
  if (window_state->IsPip() && IsArcWindow(window))
    return;

  // Only apps whose parent is a certain container can be saved.
  if (!window->parent() ||
      !base::Contains(kAppParentContainers, window->parent()->GetId())) {
    return;
  }

  // Only some app types can be saved.
  if (!base::Contains(kSupportedAppTypes,
                      window->GetProperty(chromeos::kAppTypeKey))) {
    return;
  }

  // Do not save window data if the setting is turned off by active user.
  if (!app_restore::AppRestoreInfo::GetInstance()->CanPerformRestore(
          Shell::Get()->session_controller()->GetActiveAccountId())) {
    return;
  }

  aura::Window::Windows mru_windows;
  // We only need |mru_windows| if |activation_index| is nullopt as
  // |mru_windows| will be used to calculated the window's activation index when
  // it's not provided by |activation_index|.
  if (!activation_index.has_value()) {
    mru_windows =
        Shell::Get()->mru_window_tracker()->BuildMruWindowList(kAllDesks);
  }
  std::unique_ptr<app_restore::WindowInfo> window_info =
      BuildWindowInfo(window, activation_index, mru_windows);
  ::full_restore::SaveWindowInfo(*window_info);

  if (g_save_window_callback_for_testing)
    g_save_window_callback_for_testing.Run(*window_info);
}

void WindowRestoreController::RestoreStateTypeAndClearLaunchedKey(
    aura::Window* window) {
  app_restore::WindowInfo* window_info = GetWindowInfo(window);
  if (window_info) {
    // Snap the window if necessary.
    auto state_type = window_info->window_state_type;
    if (state_type) {
      auto* window_state = WindowState::Get(window);
      // Add the window to be tracked by the tablet mode window manager
      // manually. It is normally tracked when it becomes visible, but in snap
      // case we want to track it before it becomes visible. This will allow us
      // to snap the window before it is shown and skip first showing the window
      // in normal or maximized state.
      // TODO(crbug.com/40163553): Investigate splitview for ARC apps, which
      // are not managed by TabletModeWindowManager.
      Shell::Get()->tablet_mode_controller()->AddWindow(window);

      if (chromeos::IsSnappedWindowStateType(*state_type)) {
        base::AutoReset<raw_ptr<aura::Window>> auto_reset_to_be_snapped(
            &to_be_snapped_window_, window);
        // Use the window restore info snap percentage as the target snap ratio.
        const float snap_ratio = window_info->snap_percentage.value_or(
                                     chromeos::kDefaultSnapRatio * 100) *
                                 0.01f;
        const WindowSnapWMEvent snap_event(
            *state_type == chromeos::WindowStateType::kPrimarySnapped
                ? WM_EVENT_SNAP_PRIMARY
                : WM_EVENT_SNAP_SECONDARY,
            snap_ratio,
            WindowSnapActionSource::
                kSnapByFullRestoreOrDeskTemplateOrSavedDesk);
        window_state->OnWMEvent(&snap_event);
      }
      if (*state_type == chromeos::WindowStateType::kFloated) {
        const WindowFloatWMEvent float_event(
            chromeos::FloatStartLocation::kBottomRight);
        window_state->OnWMEvent(&float_event);
      }
    }
  }

  // Window that are launched from window restore are not activatable initially
  // to prevent them from taking activation when Widget::Show() is called. Make
  // these windows activatable once they are launched. Use a post task since it
  // is quite common for some widgets to explicitly call Show() after
  // initialized.
  // TODO(sammiequon): Instead of disabling activation when creating the widget
  // and enabling it here, use `ShowInactive()` instead of `Show()` when the
  // widget is created.
  restore_property_clear_callbacks_.emplace(
      window, base::BindOnce(&WindowRestoreController::ClearLaunchedKey,
                             weak_ptr_factory_.GetWeakPtr(), window));

  // Also, for some ARC and chrome apps, the client can request activation after
  // showing. We cannot detect this, so we use a timeout to keep the window not
  // activatable for a while longer. Classic browser and lacros windows are
  // expected to call `ShowInactive()` where the browser is created.
  const chromeos::AppType app_type = window->GetProperty(chromeos::kAppTypeKey);
  // Prevent apply activation delay on ARC ghost window. It should be only apply
  // on real ARC window. Only ARC ghost window use this property.
  const bool is_real_arc_window =
      window->GetProperty(app_restore::kRealArcTaskWindow);
  const base::TimeDelta delay =
      app_type == chromeos::AppType::CHROME_APP ||
              (app_type == chromeos::AppType::ARC_APP && is_real_arc_window)
          ? kAllowActivationDelay
          : base::TimeDelta();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, restore_property_clear_callbacks_[window].callback(), delay);
}

void WindowRestoreController::ClearLaunchedKey(aura::Window* window) {
  CancelAndRemoveRestorePropertyClearCallback(window);

  // If the window is destroying then prevent extra work by not clearing the
  // property.
  if (!window->is_destroying())
    window->SetProperty(app_restore::kLaunchedFromAppRestoreKey, false);
}

void WindowRestoreController::CancelAndRemoveRestorePropertyClearCallback(
    aura::Window* window) {
  DCHECK(window);
  DCHECK(base::Contains(restore_property_clear_callbacks_, window));

  restore_property_clear_callbacks_[window].Cancel();
  restore_property_clear_callbacks_.erase(window);
}

void WindowRestoreController::SetSaveWindowCallbackForTesting(
    SaveWindowCallback callback) {
  g_save_window_callback_for_testing = std::move(callback);
}

}  // namespace ash
