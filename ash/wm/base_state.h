// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_BASE_STATE_H_
#define ASH_WM_BASE_STATE_H_

#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/macros.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// BaseState implements the common framework for WindowState::State.
class BaseState : public WindowState::State {
 public:
  explicit BaseState(WindowStateType initial_state_type);
  ~BaseState() override;

  // WindowState::State:
  void OnWMEvent(WindowState* window_state, const WMEvent* event) override;
  WindowStateType GetType() const override;

 protected:
  // Returns the WindowStateType corresponds to the WMEvent type.
  static WindowStateType GetStateForTransitionEvent(const WMEvent* event);

  static void CenterWindow(WindowState* window_state);
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
                            WindowStateType previous_state_type);

  // Returns the window bounds for snapped window state.
  gfx::Rect GetSnappedWindowBoundsInParent(aura::Window* window,
                                           const WindowStateType state_type);

  // The current type of the window.
  WindowStateType state_type_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BaseState);
};

}  // namespace ash

#endif  // ASH_WM_BASE_STATE_H_
