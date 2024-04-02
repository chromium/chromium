// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MULTI_DISPLAY_PERSISTENT_WINDOW_CONTROLLER_H_
#define ASH_WM_MULTI_DISPLAY_PERSISTENT_WINDOW_CONTROLLER_H_

#include <unordered_map>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Observes display changes and saves/restores window bounds persistently in
// multi-displays scenario. It will observe and restore window bounds
// persistently on screen rotation as well.
class ASH_EXPORT PersistentWindowController
    : public display::DisplayObserver,
      public SessionObserver,
      public display::DisplayManagerObserver {
 public:
  // This class is used to track a list of windows with their restore bounds in
  // parent. The restore bounds in parent will be empty if it does not exist.
  // When a window is destroyed, it is removed from the list.
  class WindowTracker : public aura::WindowObserver {
   public:
    WindowTracker();

    WindowTracker(const WindowTracker&) = delete;
    WindowTracker& operator=(const WindowTracker&) = delete;

    ~WindowTracker() override;

    const base::flat_map<aura::Window*, gfx::Rect>& window_restore_bounds_map()
        const {
      return window_restore_bounds_map_;
    }

    // Adds {window, restore_bounds_in_parent} as a pair to the map. If the
    // `window` is already tracked, it will do nothing.
    void Add(aura::Window* window, const gfx::Rect& restore_bounds_in_parent);

    void RemoveAll();

    // Removes `window` from the map of windows with restore bounds.
    void Remove(aura::Window* window);

    // aura::WindowObserver:
    void OnWindowDestroying(aura::Window* window) override;

   private:
    base::flat_map<aura::Window*, gfx::Rect> window_restore_bounds_map_;
  };

  // Public so it can be used by unit tests.
  constexpr static char kNumOfWindowsRestoredOnDisplayAdded[] =
      "Ash.PersistentWindow.NumOfWindowsRestoredOnDisplayAdded";
  constexpr static char kNumOfWindowsRestoredOnScreenRotation[] =
      "Ash.PersistentWindow.NumOfWindowsRestoredOnScreenRotation";

  PersistentWindowController();
  PersistentWindowController(const PersistentWindowController&) = delete;
  PersistentWindowController& operator=(const PersistentWindowController&) =
      delete;
  ~PersistentWindowController() override;

 private:
  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnWillRemoveDisplays(const display::Displays& removed_displays) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // display::DisplayManagerObserver:
  void OnWillProcessDisplayChanges() override;
  void OnDidProcessDisplayChanges(
      const DisplayConfigurationChange& configuration_change) override;

  // SessionObserver:
  void OnFirstSessionStarted() override;

  // Called when restoring persistent window placement on display added.
  void MaybeRestorePersistentWindowBoundsOnDisplayAdded();

  // Called when restoring persistent window placement on screen rotation.
  void MaybeRestorePersistentWindowBoundsOnScreenRotation();

  // Callback binded on display added and run on display changes are processed.
  base::OnceClosure display_added_restore_callback_;

  // Callback binded on display rotation happens and run on display changes are
  // processed.
  base::OnceClosure screen_rotation_restore_callback_;

  // Temporary storage that stores windows that may need persistent info
  // stored on display removal. Cleared when display changes are processed.
  WindowTracker need_persistent_info_windows_;

  // Tracking the screen orientation of each display before screen rotation
  // take effect. Key is the display id, value is true if the display is in
  // the landscape orientation, otherwise false. This is used to help restore
  // windows' bounds on screen rotation. It is needed since the target rotation
  // already changed even inside OnWillProcessDisplayChanges, which means the
  // screen orientation checked there will be the updated orientation when
  // screen rotation happens. So we get the initial screen orientation
  // OnFirstSessionStarted and store the updated ones inside
  // OnDidProcessDisplayChanges.
  std::unordered_map<int64_t, bool> is_landscape_orientation_map_;

  // Register for DisplayObserver callbacks.
  display::ScopedDisplayObserver display_observer_{this};

  // Register for display configuration changes.
  base::ScopedObservation<display::DisplayManager,
                          display::DisplayManagerObserver>
      display_manager_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_MULTI_DISPLAY_PERSISTENT_WINDOW_CONTROLLER_H_
