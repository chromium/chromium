// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/scoped_skip_user_session_blocked_check.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_toggle_fullscreen_event_handler.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/screen.h"
#include "ui/wm/core/scoped_animation_disabler.h"

namespace ash {

namespace {

using ::chromeos::WindowStateType;

// This function is called to check if window[i] is eligible to be carried over
// to split view mode during clamshell <-> tablet mode transition or multi-user
// switch transition. Returns true if windows[i] exists, is on |root_window|,
// and can snap in split view on |root_window|.
bool IsCarryOverCandidateForSplitView(
    const MruWindowTracker::WindowList& windows,
    size_t i,
    aura::Window* root_window) {
  return windows.size() > i && windows[i]->GetRootWindow() == root_window &&
         SplitViewController::Get(root_window)
             ->CanKeepCurrentSnapRatio(windows[i]);
}

// When switching to clamshell mode if all the following
// conditions are met:
// 1. `InClamshellSplitViewMode()` returns true;
// 2. Overview is either not active or empty;
// 3. Two windows are not in a snap group.
// This state will be out of the scope of the `SplitViewController` in clamshell
// mode and we should end split view and end overview if any. For more details,
// please refer to `split_view_controller.h`.
void MaybeEndSplitViewAndOverview() {
  Shell* shell = Shell::Get();
  OverviewController* overview_controller = shell->overview_controller();
  const bool empty_or_inactive_overview =
      !overview_controller->InOverviewSession() ||
      overview_controller->overview_session()->IsEmpty();
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  auto* primary_window = split_view_controller->primary_window();
  auto* secondary_window = split_view_controller->secondary_window();
  const bool windows_in_snap_group =
      snap_group_controller && primary_window && secondary_window &&
      snap_group_controller->AreWindowsInSnapGroup(primary_window,
                                                   secondary_window);

  if (split_view_controller->InClamshellSplitViewMode() &&
      empty_or_inactive_overview && !windows_in_snap_group) {
    split_view_controller->EndSplitView(
        SplitViewController::EndReason::kExitTabletMode);
    overview_controller->EndOverview(OverviewEndAction::kSplitView);
  }
}

// Snap the carry over windows into splitview mode at |divider_position|.
// TODO(b/327269057): Refactor split view transition. Also determine whether we
// should snap the windows in mru order, since it can cause
// `SplitViewDivider::observed_windows()` to get out of order.
void DoSplitViewTransition(
    std::vector<std::pair<aura::Window*, WindowStateType>> windows,
    int divider_position,
    WindowSnapActionSource snap_action_source) {
  if (windows.empty())
    return;

  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  for (auto& iter : windows) {
    // Preserve the current snap ratio before transition, since
    // `SplitViewController::SnapWindow()` will send a new snap event with
    // `snap_ratio`.
    std::optional<float> snap_ratio =
        WindowState::Get(iter.first)->snap_ratio();
    split_view_controller->SnapWindow(
        /*window=*/iter.first,
        /*snap_position=*/iter.second == WindowStateType::kPrimarySnapped
            ? SnapPosition::kPrimary
            : SnapPosition::kSecondary,
        snap_action_source,
        /*activate_window=*/false,
        /*snap_ratio=*/snap_ratio ? *snap_ratio : chromeos::kDefaultSnapRatio);
  }

  // For clamshell split view mode, end splitview mode if we're in single
  // split mode or both snapped mode (in both cases overview is not active)
  // except for the case when two windows are in a snap group.
  // TODO(xdai): Refactoring SplitViewController to make SplitViewController to
  // handle this case.
  MaybeEndSplitViewAndOverview();
}

void UpdateDeskContainersBackdrops() {
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    for (auto* desk_container : desks_util::GetDesksContainers(root)) {
      WorkspaceController* controller = GetWorkspaceController(desk_container);
      WorkspaceLayoutManager* layout_manager = controller->layout_manager();
      BackdropController* backdrop_controller =
          layout_manager->backdrop_controller();
      backdrop_controller->UpdateBackdrop();
    }
  }
}

}  // namespace

// Class which tells tablet mode controller to observe a given window for UMA
// logging purposes. Created before the window animations start. When this goes
// out of scope and the given window is not actually animating, tells tablet
// mode controller to stop observing.
class ScopedObserveWindowAnimation {
 public:
  ScopedObserveWindowAnimation(aura::Window* window,
                               TabletModeWindowManager* manager,
                               bool exiting_tablet_mode)
      : window_(window),
        manager_(manager),
        exiting_tablet_mode_(exiting_tablet_mode) {
    if (Shell::Get()->tablet_mode_controller() && window_) {
      Shell::Get()->tablet_mode_controller()->MaybeObserveBoundsAnimation(
          window_);
    }
  }

  ScopedObserveWindowAnimation(const ScopedObserveWindowAnimation&) = delete;
  ScopedObserveWindowAnimation& operator=(const ScopedObserveWindowAnimation&) =
      delete;

  ~ScopedObserveWindowAnimation() {
    // May be null on shutdown.
    if (!Shell::Get()->tablet_mode_controller())
      return;

    if (!window_)
      return;

    const bool is_animating =
        window_->layer()->GetAnimator()->IsAnimatingProperty(
            TabletModeController::GetObservedTabletTransitionProperty());
    // Stops observing if |window_| is not animating the property we care about,
    // or if it is not tracked by TabletModeWindowManager. When this object is
    // destroyed while exiting tablet mode, |window_| is no longer tracked, so
    // skip that check.
    if (is_animating &&
        (exiting_tablet_mode_ || manager_->IsTrackingWindow(window_))) {
      return;
    }

    Shell::Get()->tablet_mode_controller()->StopObservingAnimation(
        /*record_stats=*/false, /*delete_screenshot=*/true);
  }

 private:
  raw_ptr<aura::Window> window_;
  raw_ptr<TabletModeWindowManager> manager_;
  bool exiting_tablet_mode_;
};

TabletModeWindowManager::TabletModeWindowManager() = default;

TabletModeWindowManager::~TabletModeWindowManager() = default;

void TabletModeWindowManager::Init() {
  {
    ScopedObserveWindowAnimation scoped_observe(
        window_util::GetTopNonFloatedWindow(), this,
        /*exiting_tablet_mode=*/false);
    ArrangeWindowsForTabletMode();
  }
  AddWindowCreationObservers();
  display_observer_.emplace(this);
  SplitViewController::Get(Shell::GetPrimaryRootWindow())->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->overview_controller()->AddObserver(this);
  accounts_since_entering_tablet_.insert(
      Shell::Get()->session_controller()->GetActiveAccountId());
  event_handler_ = std::make_unique<TabletModeToggleFullscreenEventHandler>();
  tablet_mode_multitask_menu_controller_ =
      std::make_unique<TabletModeMultitaskMenuController>();
}

void TabletModeWindowManager::Shutdown(ShutdownReason shutdown_reason) {
  WindowAndStateTypeList carryover_windows_in_splitview;
  const bool was_in_overview =
      Shell::Get()->overview_controller()->InOverviewSession();

  if (shutdown_reason == ShutdownReason::kExitTabletUIMode) {
    // There are 4 cases when exiting tablet mode:
    // 1) overview is active but split view is inactive: keep overview active in
    //    clamshell mode.
    // 2) overview and splitview are both active: keep overview and splitview
    //    both active in clamshell mode, unless if it's single split state,
    //    splitview and overview will both be ended.
    // 3) overview is inactive but split view is active (two snapped windows):
    //    split view is no longer active. But the two snapped windows will still
    //    keep snapped in clamshell mode.
    // 4) overview and splitview are both inactive: keep the current behavior,
    //    i.e., restore all windows to its window state before entering tablet
    //    mode.

    // TODO(xdai): Instead of caching snapped windows and their state here, we
    // should try to see if it can be done in the WindowState::State impl.
    carryover_windows_in_splitview =
        GetCarryOverWindowsInSplitView(/*clamshell_to_tablet=*/false);

    // For case 2 and 3: End splitview mode for two snapped windows case or
    // single split case to match the clamshell split view behavior except for
    // the case when two windows are in a snap group. (there is no both snapped
    // state or single split state in clamshell split view). The windows will
    // still be kept snapped though.
    MaybeEndSplitViewAndOverview();
  }

  for (aura::Window* window : windows_to_track_)
    window->RemoveObserver(this);
  windows_to_track_.clear();
  SplitViewController::Get(Shell::GetPrimaryRootWindow())->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
  Shell::Get()->overview_controller()->RemoveObserver(this);
  display_observer_.reset();
  RemoveWindowCreationObservers();

  if (shutdown_reason == ShutdownReason::kExitTabletUIMode) {
    ScopedObserveWindowAnimation scoped_observe(
        window_util::GetTopNonFloatedWindow(), this,
        /*exiting_tablet_mode=*/true);
    ArrangeWindowsForClamshellMode(carryover_windows_in_splitview,
                                   was_in_overview);
  } else {
    CHECK_EQ(shutdown_reason, ShutdownReason::kSystemShutdown);
    while (window_state_map_.size()) {
      WindowToState::iterator iter = window_state_map_.begin();
      iter->first->RemoveObserver(this);
      window_state_map_.erase(iter);
    }
  }
}

bool TabletModeWindowManager::IsTrackingWindow(aura::Window* window) {
  return base::Contains(window_state_map_, window);
}

int TabletModeWindowManager::GetNumberOfManagedWindows() {
  return window_state_map_.size();
}

void TabletModeWindowManager::AddWindow(aura::Window* window) {
  // Only add the window if it is a direct dependent of a container window
  // and not yet tracked.
  if (IsTrackingWindow(window) || !IsContainerWindow(window->parent()))
    return;

  TrackWindow(window);
}

void TabletModeWindowManager::WindowStateDestroyed(aura::Window* window) {
  // We come here because the tablet window state object was destroyed. It was
  // destroyed either because ForgetWindow() was called, or because its
  // associated window was destroyed. In both cases, the window must has removed
  // TabletModeWindowManager as an observer.
  DCHECK(!window->HasObserver(this));

  // The window state object might have been removed in OnWindowDestroying().
  auto it = window_state_map_.find(window);
  if (it != window_state_map_.end())
    window_state_map_.erase(it);
}

void TabletModeWindowManager::SetIgnoreWmEventsForExit() {
  is_exiting_ = true;
  for (auto& pair : window_state_map_)
    pair.second->set_ignore_wm_events(true);
}

void TabletModeWindowManager::StopWindowAnimations() {
  for (auto& pair : window_state_map_)
    pair.first->layer()->GetAnimator()->StopAnimating();
}

void TabletModeWindowManager::OnOverviewModeEndingAnimationComplete(
    bool canceled) {
  if (canceled)
    return;

  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());

  // Maximize all snapped windows upon exiting overview mode except snapped
  // windows in splitview mode. Note the snapped window might not be tracked in
  // our |window_state_map_|.
  // Leave snapped windows on inactive desks unchanged.
  const MruWindowTracker::WindowList windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
          kActiveDesk);
  for (aura::Window* window : windows) {
    if (split_view_controller->primary_window() != window &&
        split_view_controller->secondary_window() != window) {
      MaximizeIfSnapped(window);
    }
  }
}

void TabletModeWindowManager::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  // All TabletModeWindowState will ignore further WMEvents, but we still have
  // to manually prevent sending maximizing events to ClientControlledState ARC
  // windows e.g. ARC apps.
  if (is_exiting_)
    return;

  if (state != SplitViewController::State::kNoSnap)
    return;

  aura::Window* primary_root = Shell::GetPrimaryRootWindow();
  switch (SplitViewController::Get(primary_root)->end_reason()) {
    case SplitViewController::EndReason::kNormal:
    case SplitViewController::EndReason::kUnsnappableWindowActivated:
    case SplitViewController::EndReason::kRootWindowDestroyed:
      break;
    case SplitViewController::EndReason::kHomeLauncherPressed:
    case SplitViewController::EndReason::kActiveUserChanged:
    case SplitViewController::EndReason::kWindowDragStarted:
    case SplitViewController::EndReason::kExitTabletMode:
    case SplitViewController::EndReason::kDesksChange:
    case SplitViewController::EndReason::kSnapGroups:
      // For the case of kHomeLauncherPressed, the home launcher will minimize
      // the snapped windows after ending splitview, so avoid maximizing them
      // here. For the case of kActiveUserChanged, the snapped windows will be
      // used to restore the splitview layout when switching back, and it is
      // already too late to maximize them anyway (the for loop below would
      // iterate over windows in the newly activated user session).
      return;
  }

  // Maximize all snapped windows upon exiting split view mode. Note the snapped
  // window might not be tracked in our |window_state_map_|.
  // Leave snapped windows on inactive desks unchanged.
  const MruWindowTracker::WindowList windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
          kActiveDesk);
  for (aura::Window* window : windows) {
    // Please notice, if there're multi displays in tablet mode, we should just
    // maximize snapped `window` which belongs to the primary root window.
    // Maximizing snapped `window` on the second display can trigger
    // `EndSplitView` which can trigger activating the overview focus widget,
    // but the pending activable window could be the window on the primary
    // display.
    if (window->GetRootWindow() != primary_root)
      continue;
    MaximizeIfSnapped(window);
  }
}

void TabletModeWindowManager::OnWindowDestroying(aura::Window* window) {
  if (IsContainerWindow(window)) {
    // container window can be removed on display destruction.
    window->RemoveObserver(this);
    observed_container_windows_.erase(window);
  } else if (base::Contains(windows_to_track_, window)) {
    // Added window was destroyed before being shown.
    windows_to_track_.erase(window);
    window->RemoveObserver(this);
  } else {
    // If a known window gets destroyed we need to remove all knowledge about
    // it.
    ForgetWindow(window, /*destroyed=*/true);
  }
}

void TabletModeWindowManager::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  // A window can get removed and then re-added by a drag and drop operation.
  if (params.new_parent && IsContainerWindow(params.new_parent) &&
      !IsTrackingWindow(params.target)) {
    // Don't register the window if the window is invisible. Instead,
    // wait until it becomes visible because the client may update the
    // flag to control if the window should be added.
    if (!params.target->IsVisible()) {
      if (!base::Contains(windows_to_track_, params.target)) {
        windows_to_track_.insert(params.target);
        params.target->AddObserver(this);
      }
      return;
    }
    TrackWindow(params.target);
    // When the state got added, the "WM_EVENT_ADDED_TO_WORKSPACE" event got
    // already sent and we have to notify our state again.
    if (IsTrackingWindow(params.target)) {
      WMEvent event(WM_EVENT_ADDED_TO_WORKSPACE);
      WindowState::Get(params.target)->OnWMEvent(&event);
    }
  }
}

void TabletModeWindowManager::OnWindowPropertyChanged(aura::Window* window,
                                                      const void* key,
                                                      intptr_t old) {
  // Stop managing |window| if it is moved to have a non-normal z-order.
  if (key == aura::client::kZOrderingKey &&
      window->GetProperty(aura::client::kZOrderingKey) !=
          ui::ZOrderLevel::kNormal) {
    ForgetWindow(window, false /* destroyed */);
  }
}

void TabletModeWindowManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (!IsContainerWindow(window))
    return;

  auto* session = Shell::Get()->overview_controller()->overview_session();
  if (session)
    session->SuspendReposition();

  // Reposition all non maximizeable windows.
  for (auto& pair : window_state_map_) {
    TabletModeWindowState::UpdateWindowPosition(
        WindowState::Get(pair.first),
        WindowState::BoundsChangeAnimationType::kNone);
  }
  if (session)
    session->ResumeReposition();
}

void TabletModeWindowManager::OnWindowVisibilityChanged(aura::Window* window,
                                                        bool visible) {
  // Skip if it's already managed.
  if (IsTrackingWindow(window))
    return;

  if (IsContainerWindow(window->parent()) &&
      base::Contains(windows_to_track_, window) && visible) {
    TrackWindow(window);
    // When the state got added, the "WM_EVENT_ADDED_TO_WORKSPACE" event got
    // already sent and we have to notify our state again.
    if (IsTrackingWindow(window)) {
      WMEvent event(WM_EVENT_ADDED_TO_WORKSPACE);
      WindowState::Get(window)->OnWMEvent(&event);
    }
  }
}

void TabletModeWindowManager::OnDisplayAdded(const display::Display& display) {
  DisplayConfigurationChanged();
}

void TabletModeWindowManager::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  DisplayConfigurationChanged();
}

void TabletModeWindowManager::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());

  // There is only one SplitViewController object for all user sessions, but
  // functionally, each user session independently can be in split view or not.
  // Here, a new user session has just been switched to, and if split view mode
  // is active then it was for the previous user session.
  // SplitViewController::EndSplitView() will perform some cleanup, including
  // setting |SplitViewController::left_window_| and
  // |SplitViewController::right_window_| to null, but the aura::Window objects
  // will be left unchanged to facilitate switching back.
  split_view_controller->EndSplitView(
      SplitViewController::EndReason::kActiveUserChanged);

  // If a user session is now active for the first time since clamshell mode,
  // then do the logic for carrying over snapped windows. Else recreate the
  // split view layout from the last time the current user session was active.
  bool refresh_snapped_windows = false;
  if (accounts_since_entering_tablet_.count(account_id) == 0u) {
    WindowAndStateTypeList windows_in_splitview =
        GetCarryOverWindowsInSplitView(/*clamshell_to_tablet=*/true);
    const int divider_position = CalculateCarryOverDividerPosition(
        windows_in_splitview, /*clamshell_to_tablet=*/true);
    DoSplitViewTransition(windows_in_splitview, divider_position,
                          WindowSnapActionSource::kSnapByDeskOrSessionChange);
    accounts_since_entering_tablet_.insert(account_id);
  } else {
    refresh_snapped_windows = true;
  }

  MaybeRestoreSplitView(refresh_snapped_windows);
}

gfx::Rect TabletModeWindowManager::GetWindowBoundsInScreen(
    aura::Window* window,
    bool from_clamshell) const {
  auto iter = window_state_map_.find(window);
  return !from_clamshell || iter == window_state_map_.end()
             ? window->GetBoundsInScreen()
             : iter->second->old_window_bounds_in_screen();
}

WindowStateType TabletModeWindowManager::GetWindowStateType(
    aura::Window* window,
    bool from_clamshell) const {
  auto iter = window_state_map_.find(window);
  return !from_clamshell || iter == window_state_map_.end()
             ? WindowState::Get(window)->GetStateType()
             : iter->second->old_state()->GetType();
}

TabletModeWindowManager::WindowAndStateTypeList
TabletModeWindowManager::GetCarryOverWindowsInSplitView(
    bool clamshell_to_tablet) const {
  // Use vector here to get eligible windows, so that the most recently used
  // window gets carried over to the split view first to prevent overview from
  // interfering with the window activation order.
  WindowAndStateTypeList windows;

  // Check the states of the topmost two non-overview windows to see if they are
  // eligible to be carried over to splitscreen. A window must meet
  // IsCarryOverCandidateForSplitView() to be carried over to splitscreen.
  MruWindowTracker::WindowList mru_windows =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);
  std::erase_if(mru_windows, [](aura::Window* window) {
    return window->GetProperty(chromeos::kIsShowingInOverviewKey);
  });
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  if (IsCarryOverCandidateForSplitView(mru_windows, 0u, root_window)) {
    if (GetWindowStateType(mru_windows[0], clamshell_to_tablet) ==
        WindowStateType::kPrimarySnapped) {
      windows.emplace_back(
          std::make_pair(mru_windows[0], WindowStateType::kPrimarySnapped));
      if (IsCarryOverCandidateForSplitView(mru_windows, 1u, root_window) &&
          GetWindowStateType(mru_windows[1], clamshell_to_tablet) ==
              WindowStateType::kSecondarySnapped) {
        windows.emplace_back(
            std::make_pair(mru_windows[1], WindowStateType::kSecondarySnapped));
      }
    } else if (GetWindowStateType(mru_windows[0], clamshell_to_tablet) ==
               WindowStateType::kSecondarySnapped) {
      windows.emplace_back(
          std::make_pair(mru_windows[0], WindowStateType::kSecondarySnapped));
      if (IsCarryOverCandidateForSplitView(mru_windows, 1u, root_window) &&
          GetWindowStateType(mru_windows[1], clamshell_to_tablet) ==
              WindowStateType::kPrimarySnapped) {
        windows.emplace_back(
            std::make_pair(mru_windows[1], WindowStateType::kPrimarySnapped));
      }
    }
  }
  return windows;
}

int TabletModeWindowManager::CalculateCarryOverDividerPosition(
    const WindowAndStateTypeList& windows_in_splitview,
    bool clamshell_to_tablet) const {
  aura::Window* left_window = nullptr;
  aura::Window* right_window = nullptr;
  for (auto& iter : windows_in_splitview) {
    if (iter.second == WindowStateType::kPrimarySnapped)
      left_window = iter.first;
    else if (iter.second == WindowStateType::kSecondarySnapped)
      right_window = iter.first;
  }
  if (!left_window && !right_window)
    return -1;

  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          left_window ? left_window : right_window);
  gfx::Rect work_area = display.work_area();
  gfx::Rect left_window_bounds =
      left_window ? GetWindowBoundsInScreen(left_window, clamshell_to_tablet)
                  : gfx::Rect();
  gfx::Rect right_window_bounds =
      right_window ? GetWindowBoundsInScreen(right_window, clamshell_to_tablet)
                   : gfx::Rect();

  const bool horizontal = IsLayoutHorizontal(display);
  const bool primary = IsLayoutPrimary(display);

  // We need to expand (or shrink) the width of the snapped windows by the half
  // of the divider width when to-clamshell (or to-tablet) transition happens
  // accordingly, because in tablet mode the "center" of the split view should
  // be the center of the divider.
  const int divider_padding =
      (clamshell_to_tablet ? -1 : 1) * kSplitviewDividerShortSideLength / 2;
  if (horizontal) {
    if (primary) {
      return left_window ? left_window_bounds.width() + divider_padding
                         : work_area.width() - right_window_bounds.width() -
                               divider_padding;
    } else {
      return left_window ? work_area.width() - left_window_bounds.width() -
                               divider_padding
                         : right_window_bounds.width() + divider_padding;
    }
  } else {
    if (primary) {
      return left_window ? left_window_bounds.height() + divider_padding
                         : work_area.height() - right_window_bounds.height() -
                               divider_padding;
    } else {
      return left_window ? work_area.height() - left_window_bounds.height() -
                               divider_padding
                         : right_window_bounds.height() + divider_padding;
    }
  }
}

void TabletModeWindowManager::ArrangeWindowsForTabletMode() {
  // There are 3 cases when entering tablet mode:
  // 1) overview is active but split view is inactive: keep overview active in
  //    tablet mode.
  // 2) overview and splitview are both active (splitview can only be active
  //    when overview is active in clamshell mode): keep overview and splitview
  //    both active in tablet mode.
  // 3) overview is inactive: keep the current behavior, i.e.,
  //    a. if the top window is a snapped window, put it in splitview
  //    b. if the second top window is also a snapped window and snapped to
  //       the other side, put it in split view as well. Otherwise, open
  //       overview on the other side of the screen
  //    c. if the top window is not a snapped window, maximize all windows
  //       when entering tablet mode.

  // |activatable_windows| includes all windows to be tracked, and that includes
  // windows on the lock screen via |scoped_skip_user_session_blocked_check|.
  ScopedSkipUserSessionBlockedCheck scoped_skip_user_session_blocked_check;
  MruWindowTracker::WindowList activatable_windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(kAllDesks);

  // Determine which windows are to be carried over to splitview from clamshell
  // mode to tablet mode.
  WindowAndStateTypeList windows_in_splitview =
      GetCarryOverWindowsInSplitView(/*clamshell_to_tablet=*/true);
  const int divider_position = CalculateCarryOverDividerPosition(
      windows_in_splitview, /*clamshell_to_tablet=*/true);

  // If split view is not appropriate, then maximize all windows and bail out.
  if (windows_in_splitview.empty()) {
    for (aura::Window* window : activatable_windows) {
      TrackWindow(window, /*entering_tablet_mode=*/true);
    }
    return;
  }

  // Carry over the state types of the windows that shall be in split view.
  // Maximize all other windows. Do not animate any window bounds updates.
  for (aura::Window* window : activatable_windows) {
    bool snap = false;
    for (auto& iter : windows_in_splitview) {
      if (window == iter.first) {
        snap = true;
        break;
      }
    }
    TrackWindow(window, /*entering_tablet_mode=*/true, snap,
                /*animate_bounds_on_attach=*/false);
  }

  // Do split view mode transition.
  DoSplitViewTransition(
      windows_in_splitview, divider_position,
      WindowSnapActionSource::kSnapByClamshellTabletTransition);
}

void TabletModeWindowManager::ArrangeWindowsForClamshellMode(
    WindowAndStateTypeList windows_in_splitview,
    bool was_in_overview) {
  const int divider_position = CalculateCarryOverDividerPosition(
      windows_in_splitview, /*clamshell_to_tablet=*/false);

  while (window_state_map_.size()) {
    aura::Window* window = window_state_map_.begin()->first;
    ForgetWindow(window, /*destroyed=*/false, was_in_overview);
  }

  // Arriving here the window state has changed to its clamshell window state.
  // Since we need to keep the windows that were in splitview still be snapped
  // in clamshell mode, change its window state to the corresponding snapped
  // window state.
  DoSplitViewTransition(
      windows_in_splitview, divider_position,
      WindowSnapActionSource::kSnapByClamshellTabletTransition);
}

void TabletModeWindowManager::TrackWindow(aura::Window* window,
                                          bool entering_tablet_mode,
                                          bool snap,
                                          bool animate_bounds_on_attach) {
  // Now that we are tracking it (or finding out it cannot be tracked), remove
  // it from `windows_to_track_`.
  if (base::Contains(windows_to_track_, window)) {
    windows_to_track_.erase(window);
    window->RemoveObserver(this);
  }

  if (!ShouldHandleWindow(window))
    return;

  DCHECK(!IsTrackingWindow(window));
  window->AddObserver(this);

  // Create and remember a tablet mode state which will attach itself to the
  // provided state object.
  window_state_map_.emplace(
      window, new TabletModeWindowState(window, weak_ptr_factory_.GetWeakPtr(),
                                        snap, animate_bounds_on_attach,
                                        entering_tablet_mode));
}

void TabletModeWindowManager::ForgetWindow(aura::Window* window,
                                           bool destroyed,
                                           bool was_in_overview) {
  windows_to_track_.erase(window);
  window->RemoveObserver(this);

  WindowToState::iterator it = window_state_map_.find(window);
  // A window may not be registered yet if the observer was
  // registered in OnWindowHierarchyChanged.
  if (it == window_state_map_.end())
    return;

  if (destroyed) {
    // If the window is to-be-destroyed, remove it from |window_state_map_|
    // immidietely. Otherwise it's possible to send a WMEvent to the to-be-
    // destroyed window.  Note we should not restore its old previous window
    // state object here since it will send unnecessary window state change
    // events. The tablet window state object and the old window state object
    // will be both deleted when the window is destroyed.
    window_state_map_.erase(it);
  } else {
    // By telling the state object to revert, it will switch back the old
    // State object and destroy itself, calling WindowStateDestroyed().
    it->second->LeaveTabletMode(WindowState::Get(it->first), was_in_overview);
    DCHECK(!IsTrackingWindow(window));
  }
}

bool TabletModeWindowManager::ShouldHandleWindow(aura::Window* window) {
  DCHECK(window);

  // Windows that don't have normal z-ordering should be free-floating and thus
  // not managed by us.
  if (window->GetProperty(aura::client::kZOrderingKey) !=
      ui::ZOrderLevel::kNormal) {
    return false;
  }

  // If the changing bounds in the maximized/fullscreen is allowed, then
  // let the client manage it even in tablet mode.
  if (!WindowState::Get(window) ||
      WindowState::Get(window)->allow_set_bounds_direct()) {
    return false;
  }

  return window->GetType() == aura::client::WINDOW_TYPE_NORMAL;
}

void TabletModeWindowManager::AddWindowCreationObservers() {
  DCHECK(observed_container_windows_.empty());
  // Observe window activations/creations in the default containers on all root
  // windows.
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    for (auto* desk_container : desks_util::GetDesksContainers(root)) {
      DCHECK(!base::Contains(observed_container_windows_, desk_container));
      desk_container->AddObserver(this);
      observed_container_windows_.insert(desk_container);
    }
  }
}

void TabletModeWindowManager::RemoveWindowCreationObservers() {
  for (aura::Window* window : observed_container_windows_)
    window->RemoveObserver(this);
  observed_container_windows_.clear();
}

void TabletModeWindowManager::DisplayConfigurationChanged() {
  RemoveWindowCreationObservers();
  AddWindowCreationObservers();
  UpdateDeskContainersBackdrops();
}

bool TabletModeWindowManager::IsContainerWindow(aura::Window* window) {
  return base::Contains(observed_container_windows_, window);
}

}  // namespace ash
