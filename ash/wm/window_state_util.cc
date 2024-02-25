// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_state_util.h"

#include "ash/public/cpp/window_animation_types.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/wm_event.h"
#include "ui/wm/core/window_util.h"

namespace ash {

void ToggleFullScreen(WindowState* window_state,
                      WindowStateDelegate* delegate) {
  // Window which cannot be maximized should not be full screen'ed.
  // It can, however, be restored if it was full screen'ed.
  bool is_fullscreen = window_state->IsFullscreen();
  if (!is_fullscreen &&
      (!window_state->CanMaximize() || !window_state->CanFullscreen())) {
    // If `window` cannot be maximized, then do a window bounce animation.
    wm::AnimateWindow(window_state->window(), wm::WINDOW_ANIMATION_TYPE_BOUNCE);
    return;
  }

  if (delegate && delegate->ToggleFullscreen(window_state))
    return;
  ::wm::SetWindowFullscreen(window_state->window(), !is_fullscreen);
}

void ToggleMaximizeCaption(WindowState* window_state) {
  // Note that this function is shared across handlers so ensure all actions
  // here are supported by them (e.g., SetBoundsDirect* may not work as intended
  // for `ClientControlledState`).
  if (window_state->IsFullscreen()) {
    const WMEvent wm_event(WM_EVENT_TOGGLE_FULLSCREEN);
    window_state->OnWMEvent(&wm_event);
  } else if (window_state->IsMaximized()) {
    window_state->Restore();
  } else if (window_state->IsNormalOrSnapped() || window_state->IsFloated()) {
    if (window_state->CanMaximize()) {
      window_state->Maximize();
    }
  }
}

void ToggleMaximize(WindowState* window_state) {
  // Note that this function is shared across handlers so ensure all actions
  // here are supported by them (e.g., SetBoundsDirect* may not work as intended
  // for `ClientControlledState`).
  if (window_state->IsFullscreen()) {
    const WMEvent wm_event(WM_EVENT_TOGGLE_FULLSCREEN);
    window_state->OnWMEvent(&wm_event);
  } else if (window_state->IsMaximized()) {
    window_state->Restore();
  } else if (window_state->CanMaximize()) {
    window_state->Maximize();
  } else {
    // If `window` cannot be maximized, then do a window bounce animation.
    wm::AnimateWindow(window_state->window(), wm::WINDOW_ANIMATION_TYPE_BOUNCE);
  }
}

}  // namespace ash
