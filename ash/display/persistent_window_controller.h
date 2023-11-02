// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_PERSISTENT_WINDOW_CONTROLLER_H_
#define ASH_DISPLAY_PERSISTENT_WINDOW_CONTROLLER_H_

#include <unordered_map>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/callback.h"
#include "ui/aura/window_tracker.h"
#include "ui/display/display_observer.h"

namespace ash {

// Observes display changes and saves/restores window bounds persistently in
// multi-displays scenario. It will observe and restore window bounds
// persistently on screen rotation as well.
class ASH_EXPORT PersistentWindowController : public display::DisplayObserver,
                                              public SessionObserver {
 public:
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
  void OnWillProcessDisplayChanges() override;
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;
  void OnDidProcessDisplayChanges() override;

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
  aura::WindowTracker need_persistent_info_windows_;

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
};

}  // namespace ash

#endif  // ASH_DISPLAY_PERSISTENT_WINDOW_CONTROLLER_H_
