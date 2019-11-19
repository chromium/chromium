// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_CONTROLLER_H_
#define ASH_WM_WINDOW_CYCLE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/time/time.h"

namespace aura {
class Window;
}

namespace ash {

class WindowCycleEventFilter;
class WindowCycleList;

// Controls cycling through windows with the keyboard via alt-tab.
// Windows are sorted primarily by most recently used, and then by screen order.
// We activate windows as you cycle through them, so the order on the screen
// may change during the gesture, but the most recently used list isn't updated
// until the cycling ends.  Thus we maintain the state of the windows
// at the beginning of the gesture so you can cycle through in a consistent
// order.
class ASH_EXPORT WindowCycleController {
 public:
  enum Direction { FORWARD, BACKWARD };

  WindowCycleController();
  virtual ~WindowCycleController();

  // Returns true if cycling through windows is enabled. This is false at
  // certain times, such as when the lock screen is visible.
  static bool CanCycle();

  // Cycles between windows in the given |direction|.
  void HandleCycleWindow(Direction direction);

  // Returns true if we are in the middle of a window cycling gesture.
  bool IsCycling() const { return window_cycle_list_.get() != NULL; }

  // Call to start cycling windows. This function adds a pre-target handler to
  // listen to the alt key release.
  void StartCycling();

  // Both of these functions stop the current window cycle and removes the event
  // filter. The former indicates success (i.e. the new window should be
  // activated) and the latter indicates that the interaction was cancelled (and
  // the originally active window should remain active).
  void CompleteCycling();
  void CancelCycling();

  // Returns the WindowCycleList.
  const WindowCycleList* window_cycle_list() const {
    return window_cycle_list_.get();
  }

 private:
  // Cycles to the next or previous window based on |direction|.
  void Step(Direction direction);

  void StopCycling();

  std::unique_ptr<WindowCycleList> window_cycle_list_;

  // Tracks what Window was active when starting to cycle and used to determine
  // if the active Window changed in when ending cycling.
  aura::Window* active_window_before_window_cycle_ = nullptr;

  // Non-null while actively cycling.
  std::unique_ptr<WindowCycleEventFilter> event_filter_;

  DISALLOW_COPY_AND_ASSIGN(WindowCycleController);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_CONTROLLER_H_
