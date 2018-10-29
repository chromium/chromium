// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_MANAGER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_MANAGER_H_

#include <stdint.h>

#include <map>
#include <unordered_set>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_state.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"

namespace aura {
class Window;
}

namespace ash {
class TabletModeController;
class TabletModeWindowState;

namespace wm {
class TabletModeEventHandler;
}

// A window manager which - when created - will force all windows into maximized
// mode. Exception are panels and windows which cannot be maximized.
// Windows which cannot be maximized / resized are centered with a layer placed
// behind the window so that no other windows are visible and/or obscured.
// With the destruction of the manager all windows will be restored to their
// original state.
class ASH_EXPORT TabletModeWindowManager
    : public aura::WindowObserver,
      public display::DisplayObserver,
      public ShellObserver,
      public SplitViewController::Observer {
 public:
  // This should only be deleted by the creator (ash::Shell).
  ~TabletModeWindowManager() override;

  // Returns the number of maximized & tracked windows by this manager.
  int GetNumberOfManagedWindows();

  // Adds a window which needs to be maximized. This is used by other window
  // managers for windows which needs to get tracked due to (upcoming) state
  // changes.
  // The call gets ignored if the window was already or should not be handled.
  void AddWindow(aura::Window* window);

  // Called from a window state object when it gets destroyed.
  void WindowStateDestroyed(aura::Window* window);

  // ShellObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnding() override;
  void OnOverviewModeEnded() override;
  void OnSplitViewModeEnded() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& display) override;
  void OnDisplayRemoved(const display::Display& display) override;

  // SplitViewController::Observer:
  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state) override;

  // Tell all managing windows not to handle WM events.
  void SetIgnoreWmEventsForExit();

 protected:
  friend class TabletModeController;

  // The object should only be created by the ash::Shell.
  TabletModeWindowManager();

 private:
  using WindowToState = std::map<aura::Window*, TabletModeWindowState*>;

  // Maximize all windows and restore their current state.
  void MaximizeAllWindows();

  // Restore all windows to their previous state.
  void RestoreAllWindows();

  // Set whether to defer bounds updates for |window|. When set to false bounds
  // will be updated as they may be stale.
  void SetDeferBoundsUpdates(aura::Window* window, bool defer_bounds_updates);

  // If the given window should be handled by us, this function will maximize it
  // and add it to the list of known windows (remembering the initial show
  // state).
  // Note: If the given window cannot be handled by us the function will return
  // immediately.
  void MaximizeAndTrackWindow(aura::Window* window);

  // Remove a window from our tracking list. If the window is going to be
  // destroyed, do not restore its old previous window state object as it will
  // send unneccessary window state change event.
  void ForgetWindow(aura::Window* window, bool destroyed);

  // Returns true when the given window should be modified in any way by us.
  bool ShouldHandleWindow(aura::Window* window);

  // Add window creation observers to track creation of new windows.
  void AddWindowCreationObservers();

  // Remove Window creation observers.
  void RemoveWindowCreationObservers();

  // Change the internal state (e.g. observers) when the display configuration
  // changes.
  void DisplayConfigurationChanged();

  // Returns true when the |window| is a container window.
  bool IsContainerWindow(aura::Window* window);

  // Add a backdrop behind the currently active window on each desktop.
  void EnableBackdropBehindTopWindowOnEachDisplay(bool enable);

  // Every window which got touched by our window manager gets added here.
  WindowToState window_state_map_;

  // All container windows which have to be tracked.
  std::unordered_set<aura::Window*> observed_container_windows_;

  // Windows added to the container, but not yet shown.
  std::unordered_set<aura::Window*> added_windows_;

  std::unique_ptr<wm::TabletModeEventHandler> event_handler_;

  // True if overview exit type is |kWindowDragged|.
  bool exit_overview_by_window_drag_ = false;

  DISALLOW_COPY_AND_ASSIGN(TabletModeWindowManager);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_MANAGER_H_
