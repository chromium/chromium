// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_LOCK_WINDOW_STATE_H_
#define ASH_WM_LOCK_WINDOW_STATE_H_

#include "ash/wm/window_state.h"

namespace ash {

// The LockWindowState implementation which reduces all possible window
// states to maximized (or normal if can't be maximized)/minimized/full-screen
// and is applied only on lock (login) window container and window containers
// associated with apps handling lock screen tray actions.
// LockWindowState implements Ash behavior without state machine.
class LockWindowState : public WindowState::State {
 public:
  // The |window|'s state object will be modified to use this new window mode
  // state handler.
  // |exclude_shelf| - if set, the maximized window size will be
  // restricted to work area defined by ash shelf, rather than taking only
  // virtual keyboard window into consideration when calculating the window
  // size.
  LockWindowState(aura::Window* window, bool exclude_shelf);

  LockWindowState(const LockWindowState&) = delete;
  LockWindowState& operator=(const LockWindowState&) = delete;

  ~LockWindowState() override;

  // WindowState::State:
  void OnWMEvent(WindowState* window_state, const WMEvent* event) override;
  chromeos::WindowStateType GetType() const override;
  void AttachState(WindowState* window_state,
                   WindowState::State* previous_state) override;
  void DetachState(WindowState* window_state) override;

  // Creates new LockWindowState instance and attaches it to `window`. See
  // constructor comment for more info about the `shelf_excluded` parameter.
  static WindowState* SetLockWindowState(aura::Window* window,
                                         bool shelf_excluded);

 private:
  // Updates the window to |new_state_type| and resulting bounds:
  // Either full screen, maximized centered or minimized. If the state does not
  // change, only the bounds will be changed.
  void UpdateWindow(WindowState* window_state,
                    chromeos::WindowStateType new_state_type);

  // Used in locked screen to get window state type depends on whether the
  // window is maximizable.
  chromeos::WindowStateType GetWindowTypeOnMaximizable(
      WindowState* window_state) const;

  // Returns boudns to be used for the provided window.
  gfx::Rect GetWindowBounds(aura::Window* window);

  // Updates the bounds taking virtual keyboard bounds into consideration.
  void UpdateBounds(WindowState* window_state);

  // The current state type. Due to the nature of this state, this can only be
  // WM_STATE_TYPE{NORMAL, MINIMIZED, MAXIMIZED}.
  chromeos::WindowStateType current_state_type_;

  // Restrict window size to the work area defined by the shelf - i.e. window
  // bounds exclude system shelf bounds.
  bool exclude_shelf_ = false;
};

}  // namespace ash

#endif  // ASH_WM_LOCK_WINDOW_STATE_H_
