// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_STATE_OBSERVER_H_
#define ASH_WM_WINDOW_STATE_OBSERVER_H_

#include "ash/ash_export.h"

namespace chromeos {
enum class WindowStateType;
}

namespace ash {

class WindowState;

class ASH_EXPORT WindowStateObserver {
 public:
  virtual ~WindowStateObserver() {}

  // Following observer methods are different from kWindowShowStatekey
  // property change as they will be invoked when the window
  // gets left/right maximized, and auto positioned. |old_type| is the value
  // before the change.

  // Called after the window's state type is set to new type, but before
  // the window's bounds has been updated for the new type.
  // This is used to update the shell state such as work area so
  // that the window can use the correct environment to update its bounds.
  virtual void OnPreWindowStateTypeChange(WindowState* window_state,
                                          chromeos::WindowStateType old_type) {}

  // Called after the window's state has been updated.
  // This is used to update the shell state that depends on the updated
  // window bounds, such as shelf visibility.
  virtual void OnPostWindowStateTypeChange(WindowState* window_state,
                                           chromeos::WindowStateType old_type) {
  }
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_STATE_OBSERVER_H_
