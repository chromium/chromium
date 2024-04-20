// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_STATE_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_STATE_H_

#include <memory>

#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/window_state.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {
class TabletModeWindowManager;

// The TabletModeWindowState implementation which reduces all possible window
// states to minimized and maximized. If a window cannot be maximized it will be
// set to normal. If a window cannot fill the entire workspace it will be
// centered within the workspace.
class TabletModeWindowState : public WindowState::State {
 public:
  // The |window|'s state object will be modified to use this new window mode
  // state handler. |snap| is for carrying over a snapped state from clamshell
  // mode to tablet mode. If |snap| is false, then the window will be maximized,
  // unless the original state was MAXIMIZED, MINIMIZED, FULLSCREEN, PINNED, or
  // TRUSTED_PINNED. Use |animate_bounds_on_attach| to specify whether to
  // animate the corresponding bounds update. Call LeaveTabletMode() to restore
  // the previous state handler, whereupon ~TabletModeWindowState() will call
  // |creator::WindowStateDestroyed()| to inform that the window mode was
  // reverted to the old window manager.
  TabletModeWindowState(aura::Window* window,
                        base::WeakPtr<TabletModeWindowManager> creator,
                        bool snap,
                        bool animate_bounds_on_attach,
                        bool entering_tablet_mode);

  TabletModeWindowState(const TabletModeWindowState&) = delete;
  TabletModeWindowState& operator=(const TabletModeWindowState&) = delete;

  ~TabletModeWindowState() override;

  // Called when the window position might need to be updated. Note that this
  // method is not supposed to be called for client-controlled windows (e.g.
  // ARC++) as the bounds change with `SetBoundsDirect` is not ack'ed by the
  // client. (b/264962634)
  // TODO(sammiequon): Consolidate with `UpdateBounds`.
  static void UpdateWindowPosition(
      WindowState* window_state,
      WindowState::BoundsChangeAnimationType animation_type);

  // Returns the maximized/full screen and/or centered bounds of a window.
  static gfx::Rect GetBoundsInTabletMode(WindowState* state_object);

  // Leaves the tablet mode by reverting to previous state object.
  void LeaveTabletMode(WindowState* window_state, bool was_in_overview);

  // WindowState::State:
  void OnWMEvent(WindowState* window_state, const WMEvent* event) override;
  chromeos::WindowStateType GetType() const override;
  void AttachState(WindowState* window_state,
                   WindowState::State* previous_state) override;
  void DetachState(WindowState* window_state) override;

  gfx::Rect old_window_bounds_in_screen() const {
    return old_window_bounds_in_screen_;
  }
  WindowState::State* old_state() { return old_state_.get(); }
  void set_ignore_wm_events(bool ignore) { ignore_wm_events_ = ignore; }

 private:
  // Updates the window to `new_state_type` and resulting bounds:
  // Either full screen, maximized centered or minimized. If the state does not
  // change, only the bounds will be changed. If `animate` is set, the bound
  // change get animated.
  void UpdateWindow(WindowState* window_state,
                    chromeos::WindowStateType new_state_type,
                    bool animate);

  // If `target_state` is PRIMARY/SECONDARY_SNAPPED or TRUSTED_PINNED/PINNED,
  // returns `target_state`. Otherwise depending on the capabilities of the
  // window either returns `WindowStateType::kMaximized` or
  // `WindowStateType::kNormal`.
  chromeos::WindowStateType AdjustStateForTabletMode(
      WindowState* window_state,
      chromeos::WindowStateType target_state);

  // Updates the bounds to the maximum possible bounds according to the current
  // window state. If `animate` is set we animate the change.
  void UpdateBounds(WindowState* window_state,
                    chromeos::WindowStateType previous_state,
                    bool animate);

  // Handles Alt+[ if `snap_position` is
  // `SnapPosition::kPrimary`; handles // Alt+] if
  // `snap_position` is `SnapPosition::kSecondary`.
  void CycleTabletSnap(WindowState* window_state, SnapPosition snap_position);

  // Tries to snap the window in tablet split view if possible. Shows a toast if
  // it cannot be snapped.
  void DoTabletSnap(WindowState* window_state,
                    WMEventType snap_event_type,
                    float snap_ratio,
                    WindowSnapActionSource snap_action_source);

  // Called by `WM_EVENT_RESTORE`, or a `WM_EVENT_NORMAL` that is restoring.
  // Restores to the state in `window_states`'s restore history.
  void DoRestore(WindowState* window_state);

  // The original bounds and state object of the window.
  gfx::Rect old_window_bounds_in_screen_;
  std::unique_ptr<WindowState::State> old_state_;

  // The window whose WindowState owns this instance.
  raw_ptr<aura::Window> window_;

  // The creator which needs to be informed when this state goes away. Use a
  // weak ptr since `creator_` can be destroyed before `this`.
  base::WeakPtr<TabletModeWindowManager> const creator_;

  // The state type to be established in AttachState(), unless
  // previous_state->GetType() is MAXIMIZED, MINIMIZED, FULLSCREEN, PINNED, or
  // TRUSTED_PINNED.
  chromeos::WindowStateType state_type_on_attach_;

  // Whether to animate in case of a bounds update when switching to
  // |state_type_on_attach_|.
  bool animate_bounds_on_attach_;

  // The current state type. Due to the nature of this state, this can only be
  // WM_STATE_TYPE{NORMAL, MINIMIZED, MAXIMIZED}.
  chromeos::WindowStateType current_state_type_;

  // If true, the state will not process events.
  bool ignore_wm_events_ = false;
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_STATE_H_
