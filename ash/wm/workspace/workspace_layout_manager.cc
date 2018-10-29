// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include <algorithm>
#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/fullscreen_window_finder.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/backdrop_delegate.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_properties.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

WorkspaceLayoutManager::SettingsBubbleWindowObserver::
    SettingsBubbleWindowObserver(
        WorkspaceLayoutManager* workspace_layout_manager)
    : workspace_layout_manager_(workspace_layout_manager) {}

WorkspaceLayoutManager::SettingsBubbleWindowObserver::
    ~SettingsBubbleWindowObserver() {
  for (auto* window : windows_)
    window->RemoveObserver(this);
}

void WorkspaceLayoutManager::SettingsBubbleWindowObserver::ObserveWindow(
    aura::Window* window) {
  if (!windows_.count(window)) {
    windows_.insert(window);
    window->AddObserver(this);
  }
}

void WorkspaceLayoutManager::SettingsBubbleWindowObserver::
    OnWindowHierarchyChanged(const HierarchyChangeParams& params) {
  if (params.new_parent &&
      params.new_parent !=
          workspace_layout_manager_->settings_bubble_container_) {
    StopOberservingWindow(params.target);
  }
}

void WorkspaceLayoutManager::SettingsBubbleWindowObserver::
    OnWindowVisibilityChanged(aura::Window* window, bool visible) {
  workspace_layout_manager_->NotifySystemUiAreaChanged();
}

void WorkspaceLayoutManager::SettingsBubbleWindowObserver::OnWindowDestroying(
    aura::Window* window) {
  StopOberservingWindow(window);
}

void WorkspaceLayoutManager::SettingsBubbleWindowObserver::
    OnWindowBoundsChanged(aura::Window* window,
                          const gfx::Rect& old_bounds,
                          const gfx::Rect& new_bounds,
                          ui::PropertyChangeReason reason) {
  workspace_layout_manager_->NotifySystemUiAreaChanged();
}

void WorkspaceLayoutManager::SettingsBubbleWindowObserver::
    StopOberservingWindow(aura::Window* window) {
  windows_.erase(window);
  window->RemoveObserver(this);
}

WorkspaceLayoutManager::WorkspaceLayoutManager(aura::Window* window)
    : window_(window),
      root_window_(window->GetRootWindow()),
      root_window_controller_(RootWindowController::ForWindow(root_window_)),
      settings_bubble_window_observer_(this),
      work_area_in_parent_(
          screen_util::GetDisplayWorkAreaBoundsInParent(window_)),
      is_fullscreen_(wm::GetWindowForFullscreenMode(window) != nullptr) {
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->activation_client()->AddObserver(this);
  root_window_->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
  DCHECK(window->GetProperty(::wm::kSnapChildrenToPixelBoundary));
  backdrop_controller_ = std::make_unique<BackdropController>(window_);
  keyboard::KeyboardController::Get()->AddObserver(this);
  settings_bubble_container_ = window->GetRootWindow()->GetChildById(
      kShellWindowId_SettingBubbleContainer);
}

WorkspaceLayoutManager::~WorkspaceLayoutManager() {
  if (root_window_)
    root_window_->RemoveObserver(this);
  if (settings_bubble_container_)
    settings_bubble_container_->RemoveObserver(this);
  for (aura::Window* window : windows_) {
    wm::WindowState* window_state = wm::GetWindowState(window);
    window_state->RemoveObserver(this);
    window->RemoveObserver(this);
  }
  display::Screen::GetScreen()->RemoveObserver(this);
  Shell::Get()->activation_client()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  keyboard::KeyboardController::Get()->RemoveObserver(this);
}

void WorkspaceLayoutManager::SetBackdropDelegate(
    std::unique_ptr<BackdropDelegate> delegate) {
  backdrop_controller_->SetBackdropDelegate(std::move(delegate));
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, aura::LayoutManager implementation:

void WorkspaceLayoutManager::OnWindowResized() {}

void WorkspaceLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  wm::WindowState* window_state = wm::GetWindowState(child);
  wm::WMEvent event(wm::WM_EVENT_ADDED_TO_WORKSPACE);
  window_state->OnWMEvent(&event);
  windows_.insert(child);
  child->AddObserver(this);
  window_state->AddObserver(this);
  UpdateShelfVisibility();
  UpdateFullscreenState();

  backdrop_controller_->OnWindowAddedToLayout(child);
  WindowPositioner::RearrangeVisibleWindowOnShow(child);
  if (Shell::Get()->screen_pinning_controller()->IsPinned())
    wm::GetWindowState(child)->DisableAlwaysOnTop(nullptr);
}

void WorkspaceLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
  windows_.erase(child);
  child->RemoveObserver(this);
  wm::WindowState* window_state = wm::GetWindowState(child);
  window_state->RemoveObserver(this);

  // When a window is removing from a workspace layout, it is going to be added
  // to a new workspace layout or destroyed.
  if (!window_state->pre_added_to_workspace_window_bounds()) {
    if (window_state->pre_auto_manage_window_bounds()) {
      window_state->SetPreAddedToWorkspaceWindowBounds(
          *window_state->pre_auto_manage_window_bounds());
    } else {
      window_state->SetPreAddedToWorkspaceWindowBounds(child->bounds());
    }
  }

  if (child->layer()->GetTargetVisibility())
    WindowPositioner::RearrangeVisibleWindowOnHideOrRemove(child);
}

void WorkspaceLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
  UpdateShelfVisibility();
  UpdateFullscreenState();
  backdrop_controller_->OnWindowRemovedFromLayout(child);
}

void WorkspaceLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                            bool visible) {
  wm::WindowState* window_state = wm::GetWindowState(child);
  // Attempting to show a minimized window. Unminimize it.
  if (visible && window_state->IsMinimized())
    window_state->Unminimize();

  if (child->layer()->GetTargetVisibility())
    WindowPositioner::RearrangeVisibleWindowOnShow(child);
  else
    WindowPositioner::RearrangeVisibleWindowOnHideOrRemove(child);
  UpdateFullscreenState();
  UpdateShelfVisibility();
  backdrop_controller_->OnChildWindowVisibilityChanged(child, visible);
}

void WorkspaceLayoutManager::SetChildBounds(aura::Window* child,
                                            const gfx::Rect& requested_bounds) {
  wm::SetBoundsEvent event(wm::WM_EVENT_SET_BOUNDS, requested_bounds);
  wm::GetWindowState(child)->OnWMEvent(&event);
  UpdateShelfVisibility();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, keyboard::KeyboardControllerObserver implementation:

void WorkspaceLayoutManager::OnKeyboardVisibleBoundsChanged(
    const gfx::Rect& new_bounds) {
  auto* keyboard_window =
      keyboard::KeyboardController::Get()->GetKeyboardWindow();
  if (keyboard_window && keyboard_window->GetRootWindow() == root_window_)
    NotifySystemUiAreaChanged();
}

void WorkspaceLayoutManager::OnKeyboardWorkspaceDisplacingBoundsChanged(
    const gfx::Rect& new_bounds) {
  aura::Window* window = wm::GetActiveWindow();
  if (!window)
    return;

  window = window->GetToplevelWindow();
  if (!window_->Contains(window))
    return;

  wm::WindowState* window_state = wm::GetWindowState(window);
  if (window_state->ignore_keyboard_bounds_change())
    return;

  if (!new_bounds.IsEmpty()) {
    // Store existing bounds to be restored before resizing for keyboard if it
    // is not already stored.
    if (!window_state->HasRestoreBounds())
      window_state->SaveCurrentBoundsForRestore();

    gfx::Rect window_bounds(window->GetTargetBounds());
    ::wm::ConvertRectToScreen(window_, &window_bounds);
    int vertical_displacement =
        std::max(0, window_bounds.bottom() - new_bounds.y());
    int shift = std::min(vertical_displacement,
                         window_bounds.y() - work_area_in_parent_.y());
    if (shift > 0) {
      gfx::Point origin(window_bounds.x(), window_bounds.y() - shift);
      SetChildBounds(window, gfx::Rect(origin, window_bounds.size()));
    }
  } else if (window_state->HasRestoreBounds()) {
    // Keyboard hidden, restore original bounds if they exist. If the user has
    // resized or dragged the window in the meantime, WorkspaceWindowResizer
    // will have cleared the restore bounds and this code will not accidentally
    // override user intent.
    window_state->SetAndClearRestoreBounds();
  }
}

void WorkspaceLayoutManager::OnStateChanged(
    keyboard::KeyboardControllerState state) {
  auto* keyboard_window =
      keyboard::KeyboardController::Get()->GetKeyboardWindow();
  if (keyboard_window && keyboard_window->GetRootWindow() == root_window_)
    NotifySystemUiAreaChanged();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, aura::WindowObserver implementation:

void WorkspaceLayoutManager::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  if (params.new_parent && params.new_parent == settings_bubble_container_)
    settings_bubble_window_observer_.ObserveWindow(params.target);

  if (!wm::GetWindowState(params.target)->IsActive())
    return;
  // If the window is already tracked by the workspace this update would be
  // redundant as the fullscreen and shelf state would have been handled in
  // OnWindowAddedToLayout.
  if (windows_.find(params.target) != windows_.end())
    return;

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
  if (window->parent() == settings_bubble_container_)
    settings_bubble_window_observer_.ObserveWindow(window);
}

void WorkspaceLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  if (key == aura::client::kAlwaysOnTopKey) {
    if (window->GetProperty(aura::client::kAlwaysOnTopKey)) {
      aura::Window* container =
          root_window_controller_->always_on_top_controller()->GetContainer(
              window);
      if (window->parent() != container)
        container->AddChild(window);
    }
  } else if (key == kBackdropWindowMode) {
    backdrop_controller_->UpdateBackdrop();
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
  if (settings_bubble_container_ == window)
    settings_bubble_container_ = nullptr;
}

void WorkspaceLayoutManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (root_window_ == window) {
    const wm::WMEvent wm_event(wm::WM_EVENT_DISPLAY_BOUNDS_CHANGED);
    AdjustAllWindowsBoundsForWorkAreaChange(&wm_event);
  }
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, wm::ActivationChangeObserver implementation:

void WorkspaceLayoutManager::OnWindowActivating(ActivationReason reason,
                                                aura::Window* gaining_active,
                                                aura::Window* losing_active) {
  wm::WindowState* window_state =
      gaining_active ? wm::GetWindowState(gaining_active) : nullptr;
  if (window_state && window_state->IsMinimized() &&
      !gaining_active->IsVisible()) {
    window_state->Unminimize();
    DCHECK(!window_state->IsMinimized());
  }
}

void WorkspaceLayoutManager::OnWindowActivated(ActivationReason reason,
                                               aura::Window* gained_active,
                                               aura::Window* lost_active) {
  UpdateFullscreenState();
  UpdateShelfVisibility();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, wm::WindowStateObserver implementation:

void WorkspaceLayoutManager::OnPostWindowStateTypeChange(
    wm::WindowState* window_state,
    mojom::WindowStateType old_type) {
  // Notify observers that fullscreen state may be changing.
  if (window_state->IsFullscreen() ||
      old_type == mojom::WindowStateType::FULLSCREEN) {
    UpdateFullscreenState();
  }

  UpdateShelfVisibility();
  backdrop_controller_->OnPostWindowStateTypeChange(window_state, old_type);
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

  const gfx::Rect work_area(
      screen_util::GetDisplayWorkAreaBoundsInParent(window_));
  if (work_area != work_area_in_parent_) {
    const wm::WMEvent event(wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED);
    AdjustAllWindowsBoundsForWorkAreaChange(&event);
  }
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, ShellObserver implementation:

void WorkspaceLayoutManager::OnFullscreenStateChanged(
    bool is_fullscreen,
    aura::Window* root_window) {
  if (root_window != root_window_ || is_fullscreen_ == is_fullscreen)
    return;

  is_fullscreen_ = is_fullscreen;
  if (Shell::Get()->screen_pinning_controller()->IsPinned()) {
    // If this is in pinned mode, then this event does not trigger the
    // always-on-top state change, because it is kept disabled regardless of
    // the fullscreen state change.
    return;
  }

  UpdateAlwaysOnTop(is_fullscreen_ ? wm::GetWindowForFullscreenMode(window_)
                                   : nullptr);
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

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, private:

void WorkspaceLayoutManager::AdjustAllWindowsBoundsForWorkAreaChange(
    const wm::WMEvent* event) {
  DCHECK(event->type() == wm::WM_EVENT_DISPLAY_BOUNDS_CHANGED ||
         event->type() == wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED);

  work_area_in_parent_ = screen_util::GetDisplayWorkAreaBoundsInParent(window_);

  // Don't do any adjustments of the insets while we are in screen locked mode.
  // This would happen if the launcher was auto hidden before the login screen
  // was shown and then gets shown when the login screen gets presented.
  if (event->type() == wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED &&
      Shell::Get()->session_controller()->IsScreenLocked())
    return;

  // If a user plugs an external display into a laptop running Aura the
  // display size will change.  Maximized windows need to resize to match.
  // We also do this when developers running Aura on a desktop manually resize
  // the host window.
  // We also need to do this when the work area insets changes.
  for (aura::Window* window : windows_)
    wm::GetWindowState(window)->OnWMEvent(event);
}

void WorkspaceLayoutManager::UpdateShelfVisibility() {
  root_window_controller_->shelf()->UpdateVisibilityState();
}

void WorkspaceLayoutManager::UpdateFullscreenState() {
  // TODO(flackr): The fullscreen state is currently tracked per workspace
  // but the shell notification implies a per root window state. Currently
  // only windows in the default workspace container will go fullscreen but
  // this should really be tracked by the RootWindowController since
  // technically any container could get a fullscreen window.
  if (window_->id() != kShellWindowId_DefaultContainer)
    return;
  bool is_fullscreen = wm::GetWindowForFullscreenMode(window_) != nullptr;
  if (is_fullscreen != is_fullscreen_) {
    Shell::Get()->NotifyFullscreenStateChanged(is_fullscreen, root_window_);
    is_fullscreen_ = is_fullscreen;
  }
}

void WorkspaceLayoutManager::UpdateAlwaysOnTop(aura::Window* window_on_top) {
  // Changing always on top state may change window's parent. Iterate on a copy
  // of |windows_| to avoid invalidating an iterator. Since both workspace and
  // always_on_top containers' layouts are managed by this class all the
  // appropriate windows will be included in the iteration.
  WindowSet windows(windows_);
  for (aura::Window* window : windows) {
    wm::WindowState* window_state = wm::GetWindowState(window);
    if (window_on_top)
      window_state->DisableAlwaysOnTop(window_on_top);
    else
      window_state->RestoreAlwaysOnTop();
  }
}

void WorkspaceLayoutManager::NotifySystemUiAreaChanged() {
  for (auto* window : windows_) {
    wm::WMEvent event(wm::WM_EVENT_SYSTEM_UI_AREA_CHANGED);
    wm::GetWindowState(window)->OnWMEvent(&event);
  }
}

}  // namespace ash
