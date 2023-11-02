// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_MOVE_WINDOW_UTIL_H_
#define ASH_DISPLAY_DISPLAY_MOVE_WINDOW_UTIL_H_

#include "ash/ash_export.h"

namespace ash {

namespace display_move_window_util {

// Returns true if active window can be moved between displays by accelerator.
ASH_EXPORT bool CanHandleMoveActiveWindowBetweenDisplays();

// Handles moving current active window from its display to another display.
ASH_EXPORT void HandleMoveActiveWindowBetweenDisplays();

}  // namespace display_move_window_util

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_MOVE_WINDOW_UTIL_H_
