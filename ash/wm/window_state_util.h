// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_STATE_UTIL_H_
#define ASH_WM_WINDOW_STATE_UTIL_H_

#include "ash/ash_export.h"

namespace ash {

class WindowState;
class WindowStateDelegate;

// Toggle the full screen from inside a WindowState::State handler.
ASH_EXPORT void ToggleFullScreen(WindowState* window_state,
                                 WindowStateDelegate* delegate);

// Toggle the maximized state by a caption event from inside a
// `WindowState::State` handler.
void ToggleMaximizeCaption(WindowState* window_state);

// Toggle the maximized state from inside a `WindowState::State` handler.
void ToggleMaximize(WindowState* window_state);

}  // namespace ash

#endif  // ASH_WM_WINDOW_STATE_UTIL_H_
