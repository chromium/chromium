// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_state_util.h"

#include "ash/public/cpp/window_animation_types.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
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

}  // namespace ash
