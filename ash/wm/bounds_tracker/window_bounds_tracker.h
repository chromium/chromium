// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_BOUNDS_TRACKER_WINDOW_BOUNDS_TRACKER_H_
#define ASH_WM_BOUNDS_TRACKER_WINDOW_BOUNDS_TRACKER_H_

#include <unordered_map>

#include "base/containers/flat_map.h"
#include "chromeos/ui/base/display_util.h"
#include "ui/aura/window_observer.h"

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
class WindowBoundsTracker : public aura::WindowObserver {
 public:
  WindowBoundsTracker();
  WindowBoundsTracker(const WindowBoundsTracker&) = delete;
  WindowBoundsTracker& operator=(const WindowBoundsTracker&) = delete;
  ~WindowBoundsTracker() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // Gets the window's restoring or remapping bounds in parent coordinates.
  // Restoring bounds is from `bounds_database_` with the key
  // `DisplayWindowInfo(target_display_id, target_rotation,
  // target_work_area)` if it exists. Otherwise, calculating the window's
  // remapping bounds in this target display configuration. There are three
  // mechanisms of the calculation: 1) keep the window's physical position on
  // screen rotation 2) keep the same relative position to the center point of
  // the work area 3) offscreen protection.
  gfx::Rect RemapOrRestore(aura::Window* window, int64_t target_display_id);

  // Adds `window` and its host display id to `window_to_display_map_` before
  // removing its host display.
  void AddWindowDisplayIdOnDisplayRemoval(aura::Window* window);

  // Checks `window_to_display_map_` to restore the windows if their previous
  // host display is the display that was just added.
  void MaybeRestoreWindowsOnDisplayAdded();

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

  using WindowBoundsMap = base::flat_map<WindowDisplayInfo, gfx::Rect>;

  // Resets `bounds_database_`.
  void ResetBoundsDatabase();

  // Stops observing `window` and removes it from the `bounds_database_`.
  void RemoveWindowFromBoundsDatabase(aura::Window* window);

  // Updates the window's bounds stored in `bounds_database_` to the key
  // `DisplayWindowInfo(display_id, rotation, work_area)`. Returns the bounds
  // database of `window` stored in `bounds_database_`.
  WindowBoundsMap& UpdateBoundsDatabaseOfWindow(
      aura::Window* window,
      int64_t display_id,
      display::Display::Rotation rotation,
      const gfx::Rect& work_area);

  // Stores the window's host display id when removing its host display, which
  // will be used to restore the window when its host display being reconnected
  // later.
  base::flat_map<aura::Window*, int64_t> window_to_display_map_;

  // TODO: Figure out how we can redesign this data structure, then extra data
  // structures like `window_to_display_map_` above can be removed.
  // The database that stores the window's bounds in each display configuration.
  // `WindowDisplayInfo` defines the display configuration changes that we are
  // tracking. Note: stored window bounds are in parent coordinates.
  std::unordered_map<aura::Window*, WindowBoundsMap> bounds_database_;
};

}  // namespace ash

#endif  // ASH_WM_BOUNDS_TRACKER_WINDOW_BOUNDS_TRACKER_H_
