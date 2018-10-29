// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_

#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ash/wm/window_state_observer.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

class RootWindowController;
class BackdropDelegate;
class BackdropController;

namespace wm {
class WMEvent;
}

// LayoutManager used on the window created for a workspace.
class ASH_EXPORT WorkspaceLayoutManager
    : public aura::LayoutManager,
      public aura::WindowObserver,
      public ::wm::ActivationChangeObserver,
      public keyboard::KeyboardControllerObserver,
      public display::DisplayObserver,
      public ShellObserver,
      public wm::WindowStateObserver {
 public:
  // |window| is the container for this layout manager.
  explicit WorkspaceLayoutManager(aura::Window* window);
  ~WorkspaceLayoutManager() override;

  // A delegate which can be set to add a backdrop behind the top most visible
  // window. With the call the ownership of the delegate will be transferred to
  // the WorkspaceLayoutManager.
  void SetBackdropDelegate(std::unique_ptr<BackdropDelegate> delegate);

  BackdropController* backdrop_controller() {
    return backdrop_controller_.get();
  }

  // aura::LayoutManager:
  void OnWindowResized() override;
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override;
  void OnWindowRemovedFromLayout(aura::Window* child) override;
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override;
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

  // aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowAdded(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivating(ActivationReason reason,
                          aura::Window* gaining_active,
                          aura::Window* losing_active) override;
  void OnWindowActivated(
      ::wm::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // keyboard::KeyboardControllerObserver:
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& new_bounds) override;
  void OnKeyboardWorkspaceDisplacingBoundsChanged(
      const gfx::Rect& new_bounds) override;
  void OnStateChanged(keyboard::KeyboardControllerState state) override;

  // WindowStateObserver:
  void OnPostWindowStateTypeChange(wm::WindowState* window_state,
                                   mojom::WindowStateType old_type) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // ShellObserver:
  void OnFullscreenStateChanged(bool is_fullscreen,
                                aura::Window* root_window) override;
  void OnPinnedStateChanged(aura::Window* pinned_window) override;

 private:
  friend class WorkspaceControllerTestApi;
  typedef std::set<aura::Window*> WindowSet;

  // Observes changes in windows in the SettingsBubbleWindowObserver, and
  // notifies WorkspaceLayoutManager to send out system ui area change events.
  class SettingsBubbleWindowObserver : public aura::WindowObserver {
   public:
    SettingsBubbleWindowObserver(
        WorkspaceLayoutManager* workspace_layout_manager);
    ~SettingsBubbleWindowObserver() override;

    void ObserveWindow(aura::Window* window);

    // aura::WindowObserver:
    void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
    void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
    void OnWindowDestroying(aura::Window* window) override;
    void OnWindowBoundsChanged(aura::Window* window,
                               const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds,
                               ui::PropertyChangeReason reason) override;

   private:
    // WorkspaceLayoutManager has at least as long a lifetime as this class.
    WorkspaceLayoutManager* workspace_layout_manager_;
    WindowSet windows_;

    void StopOberservingWindow(aura::Window* window);

    DISALLOW_COPY_AND_ASSIGN(SettingsBubbleWindowObserver);
  };

  // Adjusts the bounds of all managed windows when the display area changes.
  // This happens when the display size, work area insets has changed.
  // If this is called for a display size change (i.e. |event|
  // is DISPLAY_RESIZED), the non-maximized/non-fullscreen
  // windows are readjusted to make sure the window is completely within the
  // display region. Otherwise, it makes sure at least some parts of the window
  // is on display.
  void AdjustAllWindowsBoundsForWorkAreaChange(const wm::WMEvent* event);

  // Updates the visibility state of the shelf.
  void UpdateShelfVisibility();

  // Updates the fullscreen state of the workspace and notifies Shell if it
  // has changed.
  void UpdateFullscreenState();

  // Updates the always-on-top state for windows managed by this layout
  // manager.
  void UpdateAlwaysOnTop(aura::Window* window_on_top);

  // Notifies windows about a change in a system ui area. This could be
  // the keyboard or any window in the SettingsBubbleContainer. Windows will
  // only be notified about changes to system ui areas on the display they are
  // on.
  void NotifySystemUiAreaChanged();

  aura::Window* window_;
  aura::Window* root_window_;
  RootWindowController* root_window_controller_;
  aura::Window* settings_bubble_container_;
  SettingsBubbleWindowObserver settings_bubble_window_observer_;

  // Set of windows we're listening to.
  WindowSet windows_;

  // The work area in the coordinates of |window_|.
  gfx::Rect work_area_in_parent_;

  // True if this workspace is currently in fullscreen mode.
  bool is_fullscreen_;

  // A window which covers the full container and which gets inserted behind the
  // topmost visible window.
  std::unique_ptr<BackdropController> backdrop_controller_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceLayoutManager);
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_
