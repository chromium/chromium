// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DEFAULT_STATE_H_
#define ASH_WM_DEFAULT_STATE_H_

#include "ash/wm/base_state.h"
#include "ash/wm/window_state.h"
#include "base/macros.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
class SetBoundsWMEvent;

namespace mojom {
enum class WindowStateType;
}

// DefaultState implements Ash behavior without state machine.
class DefaultState : public BaseState {
 public:
  explicit DefaultState(WindowStateType initial_state_type);
  ~DefaultState() override;

  // WindowState::State overrides:
  void AttachState(WindowState* window_state,
                   WindowState::State* previous_state) override;
  void DetachState(WindowState* window_state) override;

  // BaseState:
  void HandleWorkspaceEvents(WindowState* window_state,
                             const WMEvent* event) override;
  void HandleCompoundEvents(WindowState* window_state,
                            const WMEvent* event) override;
  void HandleBoundsEvents(WindowState* window_state,
                          const WMEvent* event) override;
  void HandleTransitionEvents(WindowState* window_state,
                              const WMEvent* event) override;

 private:
  // Set the fullscreen/maximized bounds without animation.
  static bool SetMaximizedOrFullscreenBounds(WindowState* window_state);

  static void SetBounds(WindowState* window_state,
                        const SetBoundsWMEvent* bounds_event);

  // Enters next state. This is used when the state moves from one to another
  // within the same desktop mode.
  void EnterToNextState(WindowState* window_state,
                        WindowStateType next_state_type);

  // Reenters the current state. This is called when migrating from
  // previous desktop mode, and the window's state needs to re-construct the
  // state/bounds for this state.
  void ReenterToCurrentState(WindowState* window_state,
                             WindowState::State* state_in_previous_mode);

  // Animates to new window bounds based on the current and previous state type.
  void UpdateBoundsFromState(WindowState* window_state,
                             WindowStateType old_state_type);

  // The saved window state for the case that the state gets de-/activated.
  gfx::Rect stored_bounds_;
  gfx::Rect stored_restore_bounds_;

  // The display state in which the mode got started.
  display::Display stored_display_state_;

  // The window state only gets remembered for DCHECK reasons.
  WindowState* stored_window_state_;

  DISALLOW_COPY_AND_ASSIGN(DefaultState);
};

}  // namespace ash

#endif  // ASH_WM_DEFAULT_STATE_H_
