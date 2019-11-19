// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_STATE_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_STATE_H_

#include <memory>

#include "ash/wm/window_state.h"
#include "base/macros.h"

namespace ash {
class TabletModeWindowManager;

// The TabletModeWindowState implementation which reduces all possible window
// states to minimized and maximized. If a window cannot be maximized it will be
// set to normal. If a window cannot fill the entire workspace it will be
// centered within the workspace.
class TabletModeWindowState : public WindowState::State {
 public:
  // Called when the window position might need to be updated.
  static void UpdateWindowPosition(WindowState* window_state, bool animate);

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
                        TabletModeWindowManager* creator,
                        bool snap,
                        bool animate_bounds_on_attach,
                        bool entering_tablet_mode);
  ~TabletModeWindowState() override;

  void set_ignore_wm_events(bool ignore) { ignore_wm_events_ = ignore; }

  // Leaves the tablet mode by reverting to previous state object.
  void LeaveTabletMode(WindowState* window_state, bool was_in_overview);

  // WindowState::State overrides:
  void OnWMEvent(WindowState* window_state, const WMEvent* event) override;

  WindowStateType GetType() const override;
  void AttachState(WindowState* window_state,
                   WindowState::State* previous_state) override;
  void DetachState(WindowState* window_state) override;

  WindowState::State* old_state() { return old_state_.get(); }

 private:
  // Updates the window to |new_state_type| and resulting bounds:
  // Either full screen, maximized centered or minimized. If the state does not
  // change, only the bounds will be changed. If |animate| is set, the bound
  // change get animated.
  void UpdateWindow(WindowState* window_state,
                    WindowStateType new_state_type,
                    bool animate);

  // Depending on the capabilities of the window we either return
  // |WindowStateType::kMaximized| or |WindowStateType::kNormal|.
  WindowStateType GetMaximizedOrCenteredWindowType(WindowState* window_state);

  // If |target_state| is LEFT/RIGHT_SNAPPED and the window can be snapped,
  // returns |target_state|. Otherwise depending on the capabilities of the
  // window either returns |WindowStateType::kMaximized| or
  // |WindowStateType::kNormal|.
  WindowStateType GetSnappedWindowStateType(WindowState* window_state,
                                            WindowStateType target_state);

  // Updates the bounds to the maximum possible bounds according to the current
  // window state. If |animated| is set we animate the change.
  void UpdateBounds(WindowState* window_state, bool animated);

  // The original state object of the window.
  std::unique_ptr<WindowState::State> old_state_;

  // The window whose WindowState owns this instance.
  aura::Window* window_;

  // The creator which needs to be informed when this state goes away.
  TabletModeWindowManager* creator_;

  // The state type to be established in AttachState(), unless
  // previous_state->GetType() is MAXIMIZED, MINIMIZED, FULLSCREEN, PINNED, or
  // TRUSTED_PINNED.
  WindowStateType state_type_on_attach_;

  // Whether to animate in case of a bounds update when switching to
  // |state_type_on_attach_|.
  bool animate_bounds_on_attach_;

  // The current state type. Due to the nature of this state, this can only be
  // WM_STATE_TYPE{NORMAL, MINIMIZED, MAXIMIZED}.
  WindowStateType current_state_type_;

  // If true, the state will not process events.
  bool ignore_wm_events_ = false;

  DISALLOW_COPY_AND_ASSIGN(TabletModeWindowState);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_STATE_H_
