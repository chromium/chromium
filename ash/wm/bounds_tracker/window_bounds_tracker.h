// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_BOUNDS_TRACKER_WINDOW_BOUNDS_TRACKER_H_
#define ASH_WM_BOUNDS_TRACKER_WINDOW_BOUNDS_TRACKER_H_

#include <unordered_map>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/scoped_multi_source_observation.h"
#include "chromeos/ui/base/display_util.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Tracks the scenarios that need window bounds remapping and restoration.
// Window bounds remapping will be needed if the window being moved to a target
// display configuration without user assigned bounds. While restoration will be
// applied if the window is being moved back to its original display
// configuration. E.g., remapping the window if its host display being removed
// and restoring it if reconnecting the display.
// Note: `PersistentWindowController` will be disabled with this one enabled.
class WindowBoundsTracker : public aura::WindowObserver,
                            public wm::ActivationChangeObserver {
 public:
  WindowBoundsTracker();
  WindowBoundsTracker(const WindowBoundsTracker&) = delete;
  WindowBoundsTracker& operator=(const WindowBoundsTracker&) = delete;
  ~WindowBoundsTracker() override;

  void set_moving_window_between_displays(aura::Window* window) {
    moving_window_between_displays_ = window;
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // Called before swapping the root windows of the two given displays. This
  // will iterate the windows observed by `window_observations` and also inside
  // these two displays, to calculate and store the window's remapping bounds in
  // another display before swapping their root windows. The remapping bounds
  // will be used to set the window's bounds inside another display after
  // swapping the root windows of the two displays.
  void OnWillSwapDisplayRootWindows(int64_t first_display_id,
                                    int64_t second_display_id);

  // Called after swapping the root windows of the two given displays. This will
  // iterate the observed windows inside these two displays, and set their
  // bounds to the remapping bounds calculated inside
  // `OnWillSwapDisplayRootWindows` before swapping the root windows.
  void OnDisplayRootWindowsSwapped(int64_t first_display_id,
                                   int64_t second_display_id);

  // Adds `window` and its host display id to `window_to_display_map_` before
  // removing its host display.
  void AddWindowDisplayIdOnDisplayRemoval(aura::Window* window);

  // Checks `window_to_display_map_` to restore the windows if their previous
  // host display is the display that was just added.
  void MaybeRestoreWindowsOnDisplayAdded();

  // Updates the window's `displays_with_window_user_assigned_bounds` on the
  // given `bounds_changed_by_user`.
  void SetWindowBoundsChangedByUser(aura::Window* window,
                                    bool bounds_changed_by_user);

 private:
  // This defines the key of the window bounds database that stores the window's
  // bounds in each display configuration. It tracks the display's change,
  // rotation changes and work area changes so far.
  struct WindowDisplayInfo {
    WindowDisplayInfo(int64_t given_display_id,
                      display::Display::Rotation given_rotation,
                      const gfx::Rect& given_local_work_area);
    WindowDisplayInfo(const WindowDisplayInfo&) = default;
    WindowDisplayInfo(WindowDisplayInfo&&) = default;
    WindowDisplayInfo& operator=(const WindowDisplayInfo&) = default;
    WindowDisplayInfo& operator=(WindowDisplayInfo&&) = default;
    ~WindowDisplayInfo() = default;

    bool operator==(const WindowDisplayInfo& rhs) const {
      return display_id == rhs.display_id && rotation == rhs.rotation &&
             local_work_area == rhs.local_work_area;
    }
    bool operator!=(const WindowDisplayInfo& rhs) const {
      return !(*this == rhs);
    }

    bool operator<(const WindowDisplayInfo& rhs) const;

    int64_t display_id;
    display::Display::Rotation rotation;
    // Work area relative to the display's origin.
    gfx::Rect local_work_area;
  };

  struct WindowBoundsInfo {
    WindowBoundsInfo(const gfx::Rect& given_bounds_in_parent,
                     bool given_is_restore_bounds);
    WindowBoundsInfo(const WindowBoundsInfo&) = default;
    WindowBoundsInfo& operator=(const WindowBoundsInfo&) = default;
    ~WindowBoundsInfo() = default;

    gfx::Rect bounds_in_parent;
    // True if the stored `bounds_in_parent` is a restore bounds, which means a
    // user-assigned bounds. And the window will be put back to this restore
    // bounds directly instead of recalculation when being moved back to a
    // display with the same `WindowDisplayInfo`.
    bool is_restore_bounds;
  };

  // This defines all the info of a window stored in `bounds_database_`.
  struct WindowBoundsEntry {
    WindowBoundsEntry();
    WindowBoundsEntry(WindowBoundsEntry&&);
    WindowBoundsEntry& operator=(WindowBoundsEntry&&);
    ~WindowBoundsEntry();

    // Returns true if the window's current bounds should be stored as
    // `is_restore_bounds` inside `WindowBoundsInfo`.
    bool ShouldUseCurrentBoundsAsRestoreBounds(int64_t display_id) const;

    // Includes the displays that the window's bounds there is a user-assigned
    // bounds, which means the bounds is set by the user. See
    // `WindowState::bounds_changed_by_user_` for more details.
    base::flat_set<int64_t> displays_with_window_user_assigned_bounds;

    // A map stores the window's bounds in each `WindowDisplayInfo` it has been
    // there, which will be updated by `RemapOrRestore` on the window.
    base::flat_map<WindowDisplayInfo, WindowBoundsInfo> window_bounds_map;
  };

  // Stores the window's bounds in its current display for restoring the window
  // back to this display later. Calculates and stores the window's remapping
  // bounds inside the target display configuration. There are three mechanisms
  // of calculating the remapping bounds 1) keep the window's physical position
  // on screen rotation 2) keep the same relative position to the center point
  // of the work area 3) offscreen protection.
  //
  // Remapping will be applied to a window if it is moved to a display that it
  // has never been there before, and no user-assigned bounds is assigned. And
  // restoring will be applied if the window has been moved back to a display
  // configuration that it has been there before.
  //
  // Note: This function should be called before `window` being moved to the
  // target display.
  void RemapOrRestore(aura::Window* window, int64_t target_display_id);

  // Updates the window's bounds stored in `bounds_database_` on the key
  // `window_display_info`. `is_current_bounds` is true if the given `bounds` is
  // the window's current bounds, only update it to the database if it is a
  // restore bounds. If the given bounds is not window's current bounds, which
  // means it is calculated by the system for future use, always update it to
  // the database.
  WindowBoundsEntry& UpdateBoundsDatabaseOfWindow(
      aura::Window* window,
      const WindowDisplayInfo& window_display_info,
      const gfx::Rect& bounds,
      bool is_current_bounds);

  // Restores the given `window` back to the stored bounds inside
  // `bounds_database_` on its current `DisplayWindowInfo`.
  void RestoreWindowToCachedBounds(aura::Window* window);

  // Stores the window's host display id when removing its host display, which
  // will be used to restore the window when its host display being reconnected
  // later.
  base::flat_map<aura::Window*, int64_t> window_to_display_map_;

  // The window that is being moved between displays through the shortcut
  // `kMoveActiveWindowBetweenDisplays`.
  raw_ptr<aura::Window> moving_window_between_displays_ = nullptr;

  // True if restoring a window back to its host display on display
  // reconnection. Will be used to see whether remapping or restoring should be
  // triggered on the window on this root window changes.
  bool is_restoring_window_on_display_added_ = false;

  // TODO: Figure out how we can redesign this data structure, then extra data
  // structures like `window_to_display_map_` above can be removed.
  // The database that stores the window's bounds in each display configuration.
  // `WindowDisplayInfo` defines the display configuration changes that we are
  // tracking. Note: stored window bounds are in parent coordinates.
  std::unordered_map<aura::Window*, WindowBoundsEntry> bounds_database_;

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
};

}  // namespace ash

#endif  // ASH_WM_BOUNDS_TRACKER_WINDOW_BOUNDS_TRACKER_H_
