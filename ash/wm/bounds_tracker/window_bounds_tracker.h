// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_BOUNDS_TRACKER_WINDOW_BOUNDS_TRACKER_H_
#define ASH_WM_BOUNDS_TRACKER_WINDOW_BOUNDS_TRACKER_H_

#include <unordered_map>

#include "base/containers/flat_map.h"
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

  // Stops observing `window` and removes it from the `bounds_database_`.
  void RemoveWindowFromBoundsDatabase(aura::Window* window);

  // Updates the window's bounds stored in `bounds_database_` on the key
  // `window_display_info` to the given `bounds`. Returns the bounds database of
  // `window` stored in `bounds_database_`.
  WindowBoundsMap& UpdateBoundsDatabaseOfWindow(
      aura::Window* window,
      const WindowDisplayInfo& window_display_info,
      const gfx::Rect& bounds);

  // Stores the window's host display id when removing its host display, which
  // will be used to restore the window when its host display being reconnected
  // later.
  base::flat_map<aura::Window*, int64_t> window_to_display_map_;

  // The window that is being moved between displays through the shortcut
  // `kMoveActiveWindowBetweenDisplays`.
  raw_ptr<aura::Window, ExperimentalAsh> moving_window_between_displays_ =
      nullptr;

  // TODO: Figure out how we can redesign this data structure, then extra data
  // structures like `window_to_display_map_` above can be removed.
  // The database that stores the window's bounds in each display configuration.
  // `WindowDisplayInfo` defines the display configuration changes that we are
  // tracking. Note: stored window bounds are in parent coordinates.
  std::unordered_map<aura::Window*, WindowBoundsMap> bounds_database_;

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
};

}  // namespace ash

#endif  // ASH_WM_BOUNDS_TRACKER_WINDOW_BOUNDS_TRACKER_H_
