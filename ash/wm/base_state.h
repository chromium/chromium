// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_BASE_STATE_H_
#define ASH_WM_BASE_STATE_H_

#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// BaseState implements the common framework for WindowState::State.
class BaseState : public WindowState::State {
 public:
  explicit BaseState(chromeos::WindowStateType initial_state_type);

  BaseState(const BaseState&) = delete;
  BaseState& operator=(const BaseState&) = delete;

  ~BaseState() override;

  // WindowState::State:
  void OnWMEvent(WindowState* window_state, const WMEvent* event) override;
  chromeos::WindowStateType GetType() const override;

 protected:
  // Returns the chromeos::WindowStateType corresponds to the WMEvent type.
  static chromeos::WindowStateType GetStateForTransitionEvent(
      WindowState* window_state,
      const WMEvent* event);

  static void CycleSnap(WindowState* window_state, WMEventType event);

  // Handles workspace related events, such as DISPLAY_BOUNDS_CHANGED.
  virtual void HandleWorkspaceEvents(WindowState* window_state,
                                     const WMEvent* event) = 0;

  // Handles state dependent events, such as TOGGLE_MAXIMIZED,
  // TOGGLE_FULLSCREEN.
  virtual void HandleCompoundEvents(WindowState* window_state,
                                    const WMEvent* event) = 0;

  // Handles bounds change events: SET_BOUNDS and CENTER.
  virtual void HandleBoundsEvents(WindowState* window_state,
                                  const WMEvent* event) = 0;

  // Handles state transition events, such as MAXIMZIED, MINIMIZED.
  virtual void HandleTransitionEvents(WindowState* window_state,
                                      const WMEvent* event) = 0;

  // Shows/Hides window when minimized state changes.
  void UpdateMinimizedState(WindowState* window_state,
                            chromeos::WindowStateType previous_state_type);

  // Returns the window bounds for snapped window state for given `snap_ratio`.
  // Note that even when `snap_ratio` is provided, it might get ignored to meet
  // the window's minimum size requirement.
  gfx::Rect GetSnappedWindowBoundsInParent(
      aura::Window* window,
      const chromeos::WindowStateType state_type,
      float snap_ratio);

  // Prepares for the window snap event. Check if the window can be snapped in
  // split screen and if so, SplitViewController will start observe this window.
  // This needs to be done before the window's state and bounds change to its
  // snapped window state and bounds to make sure split screen can be properly
  // set up. `snap_action_source` specifies the source for this snap event.
  void HandleWindowSnapping(WindowState* window_state,
                            WMEventType event_type,
                            WindowSnapActionSource snap_action_source);

  // The current type of the window.
  chromeos::WindowStateType state_type_;
};

}  // namespace ash

#endif  // ASH_WM_BASE_STATE_H_
