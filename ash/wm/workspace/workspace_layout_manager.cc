// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include <algorithm>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/notification_center/ash_message_popup_collection.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/fullscreen_window_finder.h"
#include "ash/wm/pip/pip_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_tracker.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

using ::chromeos::WindowStateType;

WorkspaceLayoutManager::FloatingWindowObserver::FloatingWindowObserver(
    WorkspaceLayoutManager* workspace_layout_manager)
    : workspace_layout_manager_(workspace_layout_manager) {}

WorkspaceLayoutManager::FloatingWindowObserver::~FloatingWindowObserver() =
    default;

void WorkspaceLayoutManager::FloatingWindowObserver::MaybeObserveWindow(
    aura::Window* window) {
  if (window_observations_.IsObservingSource(window)) {
    return;
  }

  aura::Window* root = window->GetRootWindow();
  aura::Window* parent = window->parent();
  if (parent == root->GetChildById(kShellWindowId_SettingBubbleContainer) ||
      parent ==
          root->GetChildById(kShellWindowId_AccessibilityBubbleContainer) ||
      (parent == root->GetChildById(kShellWindowId_ShelfContainer) &&
       window->GetName() ==
           AshMessagePopupCollection::kMessagePopupWidgetName)) {
    window_observations_.AddObservation(window);
  }
}

void WorkspaceLayoutManager::FloatingWindowObserver::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  if (window_observations_.IsObservingSource(params.target) &&
      (!params.new_parent || params.new_parent != params.old_parent)) {
    window_observations_.RemoveObservation(params.target);
  }
}

void WorkspaceLayoutManager::FloatingWindowObserver::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  if (window_observations_.IsObservingSource(window)) {
    workspace_layout_manager_->MaybeUpdateA11yFloatingPanelOrPipBounds();
  }
}

void WorkspaceLayoutManager::FloatingWindowObserver::OnWindowDestroying(
    aura::Window* window) {
  workspace_layout_manager_->MaybeUpdateA11yFloatingPanelOrPipBounds();
  window_observations_.RemoveObservation(window);
}

void WorkspaceLayoutManager::FloatingWindowObserver::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  workspace_layout_manager_->MaybeUpdateA11yFloatingPanelOrPipBounds();
}

WorkspaceLayoutManager::WorkspaceLayoutManager(aura::Window* window)
    : window_(window),
      root_window_(window->GetRootWindow()),
      root_window_controller_(RootWindowController::ForWindow(root_window_)),
      is_fullscreen_(GetWindowForFullscreenModeForContext(window) != nullptr),
      floating_window_observer_(
          std::make_unique<FloatingWindowObserver>(this)) {
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->activation_client()->AddObserver(this);
  root_window_->AddObserver(this);
  backdrop_controller_ = std::make_unique<BackdropController>(window_);
  keyboard::KeyboardUIController::Get()->AddObserver(this);
  root_window_controller_->shelf()->AddObserver(this);
  Shell::Get()->app_list_controller()->AddObserver(this);
}

WorkspaceLayoutManager::~WorkspaceLayoutManager() {
  // WorkspaceLayoutManagers for the primary display are destroyed after
  // AppListControllerImpl. Their observers are removed in OnShellDestroying().
  if (Shell::Get()->app_list_controller())
    Shell::Get()->app_list_controller()->RemoveObserver(this);
  root_window_controller_->shelf()->RemoveObserver(this);
  if (root_window_)
    root_window_->RemoveObserver(this);
  for (aura::Window* window : windows_) {
    WindowState* window_state = WindowState::Get(window);
    window_state->RemoveObserver(this);
    window->RemoveObserver(this);
  }
  Shell::Get()->activation_client()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  keyboard::KeyboardUIController::Get()->RemoveObserver(this);
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, aura::LayoutManager implementation:

void WorkspaceLayoutManager::OnWindowResized() {}

void WorkspaceLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  DCHECK_NE(aura::client::WINDOW_TYPE_CONTROL, child->GetType());
  WindowState* window_state = WindowState::Get(child);
  WMEvent event(WM_EVENT_ADDED_TO_WORKSPACE);
  window_state->OnWMEvent(&event);
  windows_.insert(child);
  child->AddObserver(this);
  window_state->AddObserver(this);
  UpdateShelfVisibility();
  UpdateFullscreenState();
  UpdateWindowWorkspace(child);

  backdrop_controller_->OnWindowAddedToLayout(child);
  window_positioner::RearrangeVisibleWindowOnShow(child);
  if (Shell::Get()->screen_pinning_controller()->IsPinned())
    WindowState::Get(child)->DisableZOrdering(nullptr);
}

void WorkspaceLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
  windows_.erase(child);
  child->RemoveObserver(this);
  WindowState* window_state = WindowState::Get(child);
  window_state->RemoveObserver(this);

  // When a window is removing from a workspace layout, it is going to be added
  // to a new workspace layout or destroyed.
  if (!window_state->pre_added_to_workspace_window_bounds()) {
    if (window_state->pre_auto_manage_window_bounds()) {
      window_state->set_pre_added_to_workspace_window_bounds(
          *window_state->pre_auto_manage_window_bounds());
    } else {
      window_state->set_pre_added_to_workspace_window_bounds(child->bounds());
    }
  }

  if (child->layer()->GetTargetVisibility())
    window_positioner::RearrangeVisibleWindowOnHideOrRemove(child);
}

void WorkspaceLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
  UpdateShelfVisibility();
  UpdateFullscreenState();
  backdrop_controller_->OnWindowRemovedFromLayout(child);
}

void WorkspaceLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                            bool visible) {
  WindowState* window_state = WindowState::Get(child);
  // Attempting to show a minimized window. Unminimize it.
  if (visible && window_state->IsMinimized())
    window_state->Unminimize();

  if (child->layer()->GetTargetVisibility())
    window_positioner::RearrangeVisibleWindowOnShow(child);
  else
    window_positioner::RearrangeVisibleWindowOnHideOrRemove(child);
  UpdateFullscreenState();
  UpdateShelfVisibility();
  backdrop_controller_->OnChildWindowVisibilityChanged(child);
}

void WorkspaceLayoutManager::SetChildBounds(aura::Window* child,
                                            const gfx::Rect& requested_bounds) {
  WindowState* window_state = WindowState::Get(child);
  SetBoundsWMEvent event(requested_bounds);
  window_state->OnWMEvent(&event);
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, aura::WindowObserver implementation:

void WorkspaceLayoutManager::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  if (params.new_parent) {
    if (floating_window_observer_) {
      floating_window_observer_->MaybeObserveWindow(params.target);
    }

    // The window should have a parent (unless it's being removed), so we can
    // create WindowState, which requires its parent. (crbug.com/924305)
    WindowState::Get(params.target);
  }

  if (!wm::IsActiveWindow(params.target)) {
    return;
  }
  // If the window is already tracked by the workspace this update would be
  // redundant as the fullscreen and shelf state would have been handled in
  // OnWindowAddedToLayout.
  if (base::Contains(windows_, params.target)) {
    return;
  }

  // If the active window has moved to this root window then update the
  // fullscreen state.
  // TODO(flackr): Track the active window leaving this root window and update
  // the fullscreen state accordingly.
  if (params.new_parent && params.new_parent->GetRootWindow() == root_window_) {
    UpdateFullscreenState();
    UpdateShelfVisibility();
  }
}

void WorkspaceLayoutManager::OnWindowAdded(aura::Window* window) {
  if (floating_window_observer_) {
    floating_window_observer_->MaybeObserveWindow(window);
  }
}

void WorkspaceLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  if (key == aura::client::kZOrderingKey) {
    if (window->GetProperty(aura::client::kZOrderingKey) !=
        ui::ZOrderLevel::kNormal) {
      aura::Window* container =
          root_window_controller_->always_on_top_controller()->GetContainer(
              window);
      if (window->parent() != container)
        container->AddChild(window);
    }
  } else if (key == kWindowBackdropKey) {
    // kWindowBackdropKey is not supposed to be cleared.
    DCHECK(window->GetProperty(kWindowBackdropKey));
  } else if (key == aura::client::kWindowWorkspaceKey) {
    UpdateWindowWorkspace(window);
  }
}

void WorkspaceLayoutManager::OnWindowStackingChanged(aura::Window* window) {
  UpdateShelfVisibility();
  UpdateFullscreenState();
  backdrop_controller_->OnWindowStackingChanged(window);
}

void WorkspaceLayoutManager::OnWindowDestroying(aura::Window* window) {
  if (root_window_ == window) {
    root_window_->RemoveObserver(this);
    root_window_ = nullptr;
  }

  Shell::Get()->desks_controller()->MaybeRemoveVisibleOnAllDesksWindow(window);
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, wm::ActivationChangeObserver implementation:

void WorkspaceLayoutManager::OnWindowActivating(ActivationReason reason,
                                                aura::Window* gaining_active,
                                                aura::Window* losing_active) {
  if (!base::Contains(windows_, gaining_active)) {
    return;
  }

  WindowState* window_state =
      gaining_active ? WindowState::Get(gaining_active) : nullptr;
  if (window_state && window_state->IsMinimized() &&
      !gaining_active->IsVisible()) {
    window_state->Unminimize();
  }
}

void WorkspaceLayoutManager::OnWindowActivated(ActivationReason reason,
                                               aura::Window* gained_active,
                                               aura::Window* lost_active) {
  // This callback may be called multiple times with one activation change
  // because we have one instance of this class for each desk.
  if ((!gained_active || !base::Contains(windows_, gained_active)) &&
      (!lost_active || !base::Contains(windows_, lost_active))) {
    return;
  }

  if (lost_active)
    WindowState::Get(lost_active)->OnActivationLost();

  UpdateFullscreenState();
  UpdateShelfVisibility();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, ash::KeyboardControllerObserver implementation:

void WorkspaceLayoutManager::OnKeyboardVisibleBoundsChanged(
    const gfx::Rect& new_bounds) {
  auto* keyboard_window =
      keyboard::KeyboardUIController::Get()->GetKeyboardWindow();
  if (keyboard_window && keyboard_window->GetRootWindow() == root_window_) {
    MaybeUpdateA11yFloatingPanelOrPipBounds();
  }
}

void WorkspaceLayoutManager::OnKeyboardDisplacingBoundsChanged(
    const gfx::Rect& new_bounds_in_screen) {
  aura::Window* window = window_util::GetActiveWindow();
  if (!window) {
    return;
  }

  window = window->GetToplevelWindow();
  if (!window_->Contains(window)) {
    return;
  }

  WindowState* window_state = WindowState::Get(window);
  if (window_state->ignore_keyboard_bounds_change()) {
    return;
  }

  if (!new_bounds_in_screen.IsEmpty()) {
    // Store existing bounds to be restored before resizing for keyboard if it
    // is not already stored.
    if (!window_state->HasRestoreBounds()) {
      window_state->SaveCurrentBoundsForRestore();
    }

    gfx::Rect window_bounds(window->GetTargetBounds());
    wm::ConvertRectToScreen(window_, &window_bounds);
    const int vertical_displacement =
        std::max(0, window_bounds.bottom() - new_bounds_in_screen.y());
    const int shift = std::min(
        vertical_displacement,
        window_bounds.y() -
            screen_util::GetDisplayWorkAreaBoundsInParent(window_).y());
    if (shift > 0) {
      const gfx::Point origin(window_bounds.x(), window_bounds.y() - shift);
      SetChildBounds(window, gfx::Rect(origin, window_bounds.size()));
    }
  } else if (window_state->IsNormalStateType() &&
             window_state->HasRestoreBounds()) {
    // Keyboard hidden, restore original bounds if they exist. If the user has
    // resized or dragged the window in the meantime, WorkspaceWindowResizer
    // will have cleared the restore bounds and this code will not accidentally
    // override user intent. Only do this for normal window states that use the
    // restore bounds.
    window_state->SetAndClearRestoreBounds();
  }
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, WindowStateObserver implementation:

void WorkspaceLayoutManager::OnPostWindowStateTypeChange(
    WindowState* window_state,
    WindowStateType old_type) {
  // Notify observers that fullscreen state may be changing.
  if (window_state->IsFullscreen() ||
      old_type == WindowStateType::kFullscreen) {
    UpdateFullscreenState();
  }

  UpdateShelfVisibility();
  backdrop_controller_->OnPostWindowStateTypeChange(window_state->window());
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, display::DisplayObserver implementation:

void WorkspaceLayoutManager::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (display::Screen::GetScreen()->GetDisplayNearestWindow(window_).id() !=
      display.id()) {
    return;
  }

  if (changed_metrics & (display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
                         display::DisplayObserver::DISPLAY_METRIC_PRIMARY |
                         display::DisplayObserver::DISPLAY_METRIC_WORK_AREA)) {
    const DisplayMetricsChangedWMEvent wm_event(changed_metrics);
    AdjustAllWindowsBoundsForWorkAreaChange(&wm_event);
  }

  backdrop_controller_->OnDisplayMetricsChanged();
}

void WorkspaceLayoutManager::OnDisplayTabletStateChanged(
    display::TabletState state) {
  if (display::IsTabletStateChanging(state)) {
    // Do nothing if the tablet state is still in the process of transition.
    return;
  }

  backdrop_controller_->OnTabletModeChanged();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, ShellObserver implementation:

void WorkspaceLayoutManager::OnFullscreenStateChanged(bool is_fullscreen,
                                                      aura::Window* container) {
  // Note that only the active desk's container broadcasts this event, but all
  // containers' workspaces (active desk's and inactive desks' as well the
  // always-on-top container) receive it.
  DCHECK(desks_util::IsActiveDeskContainer(container));

  // If |container| is the one associated with this workspace, then fullscreen
  // state must match.
  DCHECK(window_ != container || is_fullscreen == is_fullscreen_);

  // This notification may come from active desk containers on other displays.
  // No need to update the always-on-top states if the fullscreen state change
  // happened on a different root window.
  if (container->GetRootWindow() != root_window_)
    return;

  if (Shell::Get()->screen_pinning_controller()->IsPinned()) {
    // If this is in pinned mode, then this event does not trigger the
    // always-on-top state change, because it is kept disabled regardless of
    // the fullscreen state change.
    return;
  }

  // We need to update the always-on-top states even for inactive desks
  // containers, because inactive desks may have a previously demoted
  // always-on-top windows that we need to promote back to the always-on-top
  // container if there no longer fullscreen windows on this root window.
  UpdateAlwaysOnTop(GetWindowForFullscreenModeInRoot(root_window_));
}

void WorkspaceLayoutManager::OnPinnedStateChanged(aura::Window* pinned_window) {
  const bool is_pinned = Shell::Get()->screen_pinning_controller()->IsPinned();
  if (!is_pinned && is_fullscreen_) {
    // On exiting from pinned mode, if the workspace is still in fullscreen
    // mode, then this event does not trigger the restoring yet. On exiting
    // from fullscreen, the temporarily disabled always-on-top property will be
    // restored.
    return;
  }

  UpdateAlwaysOnTop(is_pinned ? pinned_window : nullptr);
}

void WorkspaceLayoutManager::OnShellDestroying() {
  is_shell_destroying_ = true;
  Shell::Get()->app_list_controller()->RemoveObserver(this);
  floating_window_observer_.reset();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, ShelfObserver implementation:

void WorkspaceLayoutManager::OnAutoHideStateChanged(
    ShelfAutoHideState new_state) {
  MaybeUpdateA11yFloatingPanelOrPipBounds();
}

void WorkspaceLayoutManager::OnHotseatStateChanged(HotseatState old_state,
                                                   HotseatState new_state) {
  MaybeUpdateA11yFloatingPanelOrPipBounds();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, AppListControllerObserver implementation:

void WorkspaceLayoutManager::OnAppListVisibilityChanged(bool shown,
                                                        int64_t display_id) {
  if (display::Screen::GetScreen()->GetDisplayNearestWindow(window_).id() !=
      display_id) {
    return;
  }
  if (!Shell::Get()->IsInTabletMode()) {
    MaybeUpdateA11yFloatingPanelOrPipBounds();
  }
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, private:

void WorkspaceLayoutManager::AdjustAllWindowsBoundsForWorkAreaChange(
    const WMEvent* event) {
  CHECK_EQ(WM_EVENT_DISPLAY_METRICS_CHANGED, event->type());

  const DisplayMetricsChangedWMEvent* display_event =
      event->AsDisplayMetricsChangedWMEvent();
  CHECK(display_event->display_bounds_changed() ||
        display_event->primary_changed() || display_event->work_area_changed());

  MaybeUpdateA11yFloatingPanelOrPipBounds();

  // If a user plugs an external display into a laptop running Aura the
  // display size will change.  Maximized windows need to resize to match.
  // We also do this when developers running Aura on a desktop manually resize
  // the host window.
  // We also need to do this when the work area insets changes.
  // Update the windows from top-most to bottom-most so when windows get bigger
  // they occlude windows below them first.
  auto ordered_windows = window_util::SortWindowsBottomToTop(windows_);
  for (aura::Window* window : base::Reversed(ordered_windows)) {
    WindowState::Get(window)->OnWMEvent(event);
  }
}

void WorkspaceLayoutManager::UpdateShelfVisibility() {
  root_window_controller_->shelf()->UpdateVisibilityState();
}

void WorkspaceLayoutManager::UpdateFullscreenState() {
  // Note that we don't allow always-on-top or PiP containers to have fullscreen
  // windows, and we only update the fullscreen state for the active desk
  // container.
  if (!desks_util::IsActiveDeskContainer(window_))
    return;

  const bool is_fullscreen =
      GetWindowForFullscreenModeForContext(window_) != nullptr;
  if (is_fullscreen == is_fullscreen_)
    return;

  is_fullscreen_ = is_fullscreen;
  Shell::Get()->NotifyFullscreenStateChanged(is_fullscreen, window_);
}

void WorkspaceLayoutManager::UpdateAlwaysOnTop(
    aura::Window* active_desk_fullscreen_window) {
  // Changing always on top state may change window's parent. Iterate on a copy
  // of |windows_| to avoid invalidating an iterator. Since both workspace and
  // always_on_top containers' layouts are managed by this class all the
  // appropriate windows will be included in the iteration.
  // Use an `aura::WindowTracker` since `OnWillRemoveWindowFromLayout()` may
  // remove windows from `windows_`.
  std::vector<raw_ptr<aura::Window, VectorExperimental>> windows(
      windows_.begin(), windows_.end());
  aura::WindowTracker tracker(windows);
  while (!tracker.windows().empty()) {
    aura::Window* window = tracker.Pop();
    if (window == active_desk_fullscreen_window)
      continue;

    WindowState* window_state = WindowState::Get(window);
    if (active_desk_fullscreen_window)
      window_state->DisableZOrdering(active_desk_fullscreen_window);
    else
      window_state->RestoreZOrdering();
  }
}

void WorkspaceLayoutManager::MaybeUpdateA11yFloatingPanelOrPipBounds() const {
  // The PIP avoids the accessibility bubble, so here we update the
  // accessibility bubble position first, so that if the PIP is also being shown
  // the PIPs calculation does not need to take place twice.
  if (!is_shell_destroying_) {
    Shell::Get()
        ->accessibility_controller()
        ->UpdateFloatingPanelBoundsIfNeeded();
  }
  for (aura::Window* window : windows_) {
    WindowState* window_state = WindowState::Get(window);
    if (window_state->IsPip()) {
      Shell::Get()->pip_controller()->UpdatePipBounds();
    }
  }
}

void WorkspaceLayoutManager::UpdateWindowWorkspace(aura::Window* window) {
  if (window->GetType() != aura::client::WindowType::WINDOW_TYPE_NORMAL ||
      window->GetProperty(aura::client::kZOrderingKey) !=
          ui::ZOrderLevel::kNormal) {
    return;
  }

  auto* desks_controller = Shell::Get()->desks_controller();
  if (desks_util::IsWindowVisibleOnAllWorkspaces(window))
    desks_controller->AddVisibleOnAllDesksWindow(window);
  else
    desks_controller->MaybeRemoveVisibleOnAllDesksWindow(window);
}

}  // namespace ash
