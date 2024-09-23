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
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/public/activation_change_observer.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace ash {

class BackdropController;
class RootWindowController;
class WMEvent;

// LayoutManager used on the window created for a workspace.
class ASH_EXPORT WorkspaceLayoutManager : public aura::LayoutManager,
                                          public aura::WindowObserver,
                                          public wm::ActivationChangeObserver,
                                          public KeyboardControllerObserver,
                                          public WindowStateObserver,
                                          public display::DisplayObserver,
                                          public ShellObserver,
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

  bool is_fullscreen() const { return is_fullscreen_; }

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

  // wm::ActivationChangeObserver:
  void OnWindowActivating(ActivationReason reason,
                          aura::Window* gaining_active,
                          aura::Window* losing_active) override;
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
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
  void OnDisplayTabletStateChanged(display::TabletState state) override;

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

  // Observes changes in windows in the FloatingWindowObserver, and
  // notifies WorkspaceLayoutManager to update the accessibility panels and pip
  // window bounds if needed. Observes windows in the settings bubble,
  // accessibility bubble and shelf containers.
  class FloatingWindowObserver : public aura::WindowObserver {
   public:
    explicit FloatingWindowObserver(
        WorkspaceLayoutManager* workspace_layout_manager);
    FloatingWindowObserver(const FloatingWindowObserver&) = delete;
    FloatingWindowObserver& operator=(const FloatingWindowObserver&) = delete;
    ~FloatingWindowObserver() override;

    void MaybeObserveWindow(aura::Window* window);

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
    raw_ptr<const WorkspaceLayoutManager> workspace_layout_manager_;

    base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
        window_observations_{this};
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

  // Updates the bounds of the a11y floating panels (including autoclick menu
  // and stick keys) and pip window when needed. E.g, work area changes,
  // visibility of the windows observed by `FloatingWindowObserver` changes.
  void MaybeUpdateA11yFloatingPanelOrPipBounds() const;

  // Updates the window workspace.
  void UpdateWindowWorkspace(aura::Window* window);

  raw_ptr<aura::Window> window_;
  raw_ptr<aura::Window> root_window_;
  raw_ptr<RootWindowController> root_window_controller_;

  display::ScopedDisplayObserver display_observer_{this};

  // Set of windows we're listening to.
  std::set<raw_ptr<aura::Window, SetExperimental>> windows_;

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

  std::unique_ptr<FloatingWindowObserver> floating_window_observer_;

  // Indicator that the `Shell` is being destroyed and we should not
  // `NotifyAccessibilityWorkspaceChanged` in this case.
  bool is_shell_destroying_ = false;
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_
