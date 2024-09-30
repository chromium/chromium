// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_MANAGER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_MANAGER_H_

#include <stdint.h>

#include <map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/display/display_observer.h"

namespace aura {
class Window;
}

namespace ash {
class TabletModeController;
class TabletModeMultitaskMenuController;
class TabletModeToggleFullscreenEventHandler;
class TabletModeWindowState;

// A window manager which - when created - will force all windows into maximized
// mode. Exception are panels and windows which cannot be maximized.
// Windows which cannot be maximized / resized are centered with a layer placed
// behind the window so that no other windows are visible and/or obscured.
// With the destruction of the manager all windows will be restored to their
// original state.
class ASH_EXPORT TabletModeWindowManager : public aura::WindowObserver,
                                           public display::DisplayObserver,
                                           public OverviewObserver,
                                           public SplitViewObserver,
                                           public SessionObserver {
 public:
  // There are two reasons that we would destroy `this`.
  enum class ShutdownReason {
    kSystemShutdown,
    kExitTabletUIMode,
  };

  // This should only be created or deleted by the creator
  // (TabletModeController).
  TabletModeWindowManager();

  TabletModeWindowManager(const TabletModeWindowManager&) = delete;
  TabletModeWindowManager& operator=(const TabletModeWindowManager&) = delete;

  ~TabletModeWindowManager() override;

  TabletModeMultitaskMenuController* tablet_mode_multitask_menu_controller() {
    return tablet_mode_multitask_menu_controller_.get();
  }

  void Init();

  // Stops tracking windows and returns them to their clamshell mode state. Work
  // is done here instead of the destructor because TabletModeController may
  // still need this object alive during shutdown.
  void Shutdown(ShutdownReason shutdown_reason);

  // True if |window| is in |window_state_map_|.
  bool IsTrackingWindow(aura::Window* window);

  // Returns the number of maximized & tracked windows by this manager.
  int GetNumberOfManagedWindows();

  // Adds a window which needs to be maximized. This is used by other window
  // managers for windows which needs to get tracked due to (upcoming) state
  // changes.
  // The call gets ignored if the window was already or should not be handled.
  void AddWindow(aura::Window* window);

  // Called from a window state object when it gets destroyed.
  void WindowStateDestroyed(aura::Window* window);

  // Tell all managing windows not to handle WM events.
  void SetIgnoreWmEventsForExit();

  // Stops animations on windows managed by this TabletModeWindowManager.
  void StopWindowAnimations();

  // OverviewObserver:
  void OnOverviewModeEndingAnimationComplete(bool canceled) override;

  // SplitViewObserver:
  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state) override;

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
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

 private:
  using WindowToState =
      std::map<aura::Window*, raw_ptr<TabletModeWindowState, CtnExperimental>>;
  using WindowAndStateTypeList =
      std::vector<std::pair<aura::Window*, chromeos::WindowStateType>>;

  // If |from_clamshell| is true, returns the bounds or state type that |window|
  // had before tablet mode started. If |from_clamshell| is false, returns the
  // current bounds or state type of |window|.
  gfx::Rect GetWindowBoundsInScreen(aura::Window* window,
                                    bool from_clamshell) const;
  chromeos::WindowStateType GetWindowStateType(aura::Window* window,
                                               bool from_clamshell) const;

  // Returns the windows that are going to be carried over to split view during
  // clamshell <-> tablet transition or multi-user switch transition.
  std::vector<std::pair<aura::Window*, chromeos::WindowStateType>>
  GetCarryOverWindowsInSplitView(bool clamshell_to_tablet) const;

  // Calculates the split view divider position that will best preserve the
  // bounds of the windows.
  int CalculateCarryOverDividerPosition(
      const WindowAndStateTypeList& windows_in_splitview,
      bool clamshell_to_tablet) const;

  // Maximizes all windows, except that snapped windows shall carry over to
  // split view as determined by GetCarryOverWindowsInSplitView().
  void ArrangeWindowsForTabletMode();

  // Reverts all windows to how they were arranged before tablet mode.
  // |windows_in_splitview| contains the windows that were in splitview before
  // entering clamshell mode, and if clamshell split view is enabled, these
  // windows will be carried over to clamshell split view. |was_in_overview|
  // indicates whether overview is active before entering clamshell mode.
  void ArrangeWindowsForClamshellMode(
      WindowAndStateTypeList windows_in_splitview,
      bool was_in_overview);

  // If the given window should be handled by us, this function will add it to
  // the list of known windows (remembering the initial show state).
  // Note: If the given window cannot be handled by us the function will return
  // immediately.
  void TrackWindow(aura::Window* window,
                   bool entering_tablet_mode = false,
                   bool snap = false,
                   bool animate_bounds_on_attach = true);

  // Removes a window from our tracking list. |was_in_overview| used when
  // |destroyed| is false to help handle leaving tablet mode. If the window is
  // going to be destroyed, do not restore its old previous window state object
  // as it will send unnecessary window state change event.
  void ForgetWindow(aura::Window* window,
                    bool destroyed,
                    bool was_in_overview = false);

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

  // Every window which got touched by our window manager gets added here.
  WindowToState window_state_map_;

  // All container windows which have to be tracked.
  std::unordered_set<raw_ptr<aura::Window, CtnExperimental>>
      observed_container_windows_;

  // Windows added to the container, but not yet shown or tracked. They will be
  // attempted to be tracked when the window is shown.
  std::unordered_set<raw_ptr<aura::Window, CtnExperimental>> windows_to_track_;

  // All accounts that have been active at least once since tablet mode started.
  base::flat_set<AccountId> accounts_since_entering_tablet_;

  std::unique_ptr<TabletModeToggleFullscreenEventHandler> event_handler_;

  // Handles gestures that may show or hide the multitask menu.
  std::unique_ptr<TabletModeMultitaskMenuController>
      tablet_mode_multitask_menu_controller_;

  std::optional<display::ScopedDisplayObserver> display_observer_;

  // True when tablet mode is about to end.
  bool is_exiting_ = false;

  base::WeakPtrFactory<TabletModeWindowManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_MANAGER_H_
