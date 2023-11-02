// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_

#include <map>
#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shell_observer.h"
#include "ash/wm/window_state_observer.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

class RootWindowController;
class BackdropController;
class WMEvent;

// LayoutManager used on the window created for a workspace.
class ASH_EXPORT WorkspaceLayoutManager : public aura::LayoutManager,
                                          public aura::WindowObserver,
                                          public ::wm::ActivationChangeObserver,
                                          public KeyboardControllerObserver,
                                          public display::DisplayObserver,
                                          public ShellObserver,
                                          public WindowStateObserver,
                                          public ShelfObserver,
                                          public AppListControllerObserver {
 public:
  // |window| is the container for this layout manager.
  explicit WorkspaceLayoutManager(aura::Window* window);

  WorkspaceLayoutManager(const WorkspaceLayoutManager&) = delete;
  WorkspaceLayoutManager& operator=(const WorkspaceLayoutManager&) = delete;

  ~WorkspaceLayoutManager() override;

  BackdropController* backdrop_controller() {
    return backdrop_controller_.get();
  }

  bool is_fullscreen() { return is_fullscreen_; }

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

  // KeyboardControllerObserver:
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& new_bounds) override;
  void OnKeyboardDisplacingBoundsChanged(const gfx::Rect& new_bounds) override;

  // WindowStateObserver:
  void OnPostWindowStateTypeChange(WindowState* window_state,
                                   chromeos::WindowStateType old_type) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // ShellObserver:
  void OnFullscreenStateChanged(bool is_fullscreen,
                                aura::Window* container) override;
  void OnPinnedStateChanged(aura::Window* pinned_window) override;
  void OnShellDestroying() override;

  // ShelfObserver:
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;
  void OnHotseatStateChanged(HotseatState old_state,
                             HotseatState new_state) override;

  // AppListControllerObserver:
  void OnAppListVisibilityChanged(bool shown, int64_t display_id) override;

 private:
  friend class WorkspaceControllerTestApi;
  typedef std::set<aura::Window*> WindowSet;

  // Observes changes in windows in the FloatingWindowObserver, and
  // notifies WorkspaceLayoutManager to send out system ui area change events.
  // This class currently observes windows in |settings_bubble_container_|,
  // |accessibility_bubble_container_|, and |shelf_container_|.
  class FloatingWindowObserver : public aura::WindowObserver {
   public:
    explicit FloatingWindowObserver(
        WorkspaceLayoutManager* workspace_layout_manager);

    FloatingWindowObserver(const FloatingWindowObserver&) = delete;
    FloatingWindowObserver& operator=(const FloatingWindowObserver&) = delete;

    ~FloatingWindowObserver() override;

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
    const WorkspaceLayoutManager* workspace_layout_manager_;
    // The key is the window to be observed, and the value is the parent of the
    // window.
    std::map<aura::Window*, aura::Window*> observed_windows_;

    void StopOberservingWindow(aura::Window* window);
  };

  // Adjusts the bounds of all managed windows when the display area changes.
  // This happens when the display size, work area insets has changed.
  // If this is called for a display size change (i.e. |event|
  // is DISPLAY_RESIZED), the non-maximized/non-fullscreen
  // windows are readjusted to make sure the window is completely within the
  // display region. Otherwise, it makes sure at least some parts of the window
  // is on display.
  void AdjustAllWindowsBoundsForWorkAreaChange(const WMEvent* event);

  // Updates the visibility state of the shelf.
  void UpdateShelfVisibility();

  // Updates the fullscreen state of the workspace and notifies Shell if it
  // has changed.
  void UpdateFullscreenState();

  // Updates the always-on-top state for windows managed by this layout
  // manager.
  void UpdateAlwaysOnTop(aura::Window* active_desk_fullscreen_window);

  // Notifies windows about a change in a system ui area. This could be
  // the keyboard or any window in the SettingsBubbleContainer or
  // |accessibility_bubble_container_|. Windows will only be notified about
  // changes to system ui areas on the display they are on.
  void NotifySystemUiAreaChanged() const;

  // Notifies the accessibility controller about a workspace event. If autoclick
  // or stick keys is enabled, the autoclick bubble or sticky keys overlay may
  // need to move in response to that event.
  void NotifyAccessibilityWorkspaceChanged() const;

  // Updates the window workspace.
  void UpdateWindowWorkspace(aura::Window* window);

  bool IsPopupNotificationWindow(aura::Window* window) const;

  aura::Window* window_;
  aura::Window* root_window_;
  RootWindowController* root_window_controller_;
  FloatingWindowObserver floating_window_observer_;
  aura::Window* settings_bubble_container_;
  aura::Window* accessibility_bubble_container_;
  aura::Window* shelf_container_;

  display::ScopedDisplayObserver display_observer_{this};

  // Set of windows we're listening to.
  WindowSet windows_;

  // The work area in the coordinates of |window_|.
  gfx::Rect work_area_in_parent_;

  // True if this workspace is currently in fullscreen mode. Tracks the
  // fullscreen state of the container |window_| associated with this workspace
  // rather than the root window.
  // Note that in the case of a workspace of a PiP or always-on-top containers,
  // |is_fullscreen_| doesn't make sense since we don't allow windows on those
  // containers to go fullscreen. Hence, |is_fullscreen_| is always false on
  // those workspaces.
  bool is_fullscreen_;

  // A window which covers the full container and which gets inserted behind the
  // topmost visible window.
  std::unique_ptr<BackdropController> backdrop_controller_;
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_
