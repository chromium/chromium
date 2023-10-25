// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CLIENT_CONTROLLED_STATE_H_
#define ASH_WM_CLIENT_CONTROLLED_STATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/base_state.h"

namespace ash {

class WMEvent;

// ClientControlledState delegates the window state transition and
// bounds control to the client. Its window state and bounds are
// determined by the delegate. ARC++ window's state is controlled by
// Android framework, for example.
class ASH_EXPORT ClientControlledState : public BaseState {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Handles the state change of |window_state| to |requested_state|.
    // Delegate may decide to ignore the state change, proceed with the state
    // change, or can move to a different state.
    virtual void HandleWindowStateRequest(
        WindowState* window_state,
        chromeos::WindowStateType requested_state) = 0;
    // Handles the bounds change request for |window_state|. The bounds change
    // might come from a state change request |requested_state| (currently it
    // should only be a snapped window state). Delegate may choose to ignore the
    // request, set the given bounds, or set the different bounds.
    virtual void HandleBoundsRequest(
        WindowState* window_state,
        chromeos::WindowStateType requested_state,
        const gfx::Rect& requested_bounds_in_display,
        int64_t display_id) = 0;
  };

  // Adjust bounds to ensure window visibility, which is used for window added
  // to a new workspace.
  static void AdjustBoundsForMinimumWindowVisibility(
      const gfx::Rect& display_bounds,
      gfx::Rect* bounds);

  explicit ClientControlledState(std::unique_ptr<Delegate> delegate);

  ClientControlledState(const ClientControlledState&) = delete;
  ClientControlledState& operator=(const ClientControlledState&) = delete;

  ~ClientControlledState() override;

  // Resets |delegate_|.
  void ResetDelegate();

  // A flag used to update the window's bounds directly, instead of
  // delegating to |Delegate|. The Delegate should use this to
  // apply the bounds change to the window.
  void set_bounds_locally(bool set) { set_bounds_locally_ = set; }
  bool set_bounds_locally() const { return set_bounds_locally_; }

  // Sets the type of animation for the next bounds change
  // applied locally.
  void set_next_bounds_change_animation_type(
      WindowState::BoundsChangeAnimationType animation_type) {
    next_bounds_change_animation_type_ = animation_type;
  }

  // WindowState::State:
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
  void OnWindowDestroying(WindowState* window_state) override;

  // Enters next state. This is used when the state moves from one to another
  // within the same desktop mode. Returns true if the state has changed, or
  // false otherwise.
  bool EnterNextState(WindowState* window_state,
                      chromeos::WindowStateType next_state_type);

 private:
  chromeos::WindowStateType GetResolvedNextWindowStateType(
      WindowState* window_state,
      const WMEvent* event);

  void UpdateWindowForTransitionEvents(
      WindowState* window_state,
      chromeos::WindowStateType next_state_type,
      const WMEvent* event);

  std::unique_ptr<Delegate> delegate_;

  bool set_bounds_locally_ = false;
  base::TimeDelta bounds_change_animation_duration_ =
      WindowState::kBoundsChangeSlideDuration;

  WindowState::BoundsChangeAnimationType next_bounds_change_animation_type_ =
      WindowState::BoundsChangeAnimationType::kNone;
};

}  // namespace ash

#endif  // ASH_WM_CLIENT_CONTROLLED_STATE_H_
