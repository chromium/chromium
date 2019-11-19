// Copyright 2014 The Chromium Authors. All rights reserved.
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

}  // namespace ash

#endif  // ASH_WM_WINDOW_STATE_UTIL_H_
