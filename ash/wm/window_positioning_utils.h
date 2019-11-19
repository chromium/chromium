// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_POSITIONING_UTILS_H_
#define ASH_WM_WINDOW_POSITIONING_UTILS_H_

#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace display {
class Display;
}

namespace gfx {
class Rect;
class Size;
}

namespace ash {

// We force at least this many DIPs for any window on the screen.
const int kMinimumOnScreenArea = 25;

// Adjusts |bounds| so that the size does not exceed |max_size|.
ASH_EXPORT void AdjustBoundsSmallerThan(const gfx::Size& max_size,
                                        gfx::Rect* bounds);

// Move the given bounds inside the given |visible_area| in parent coordinates,
// including a safety margin given by |min_width| and |min_height|.
// This also ensures that the top of the bounds is visible.
ASH_EXPORT void AdjustBoundsToEnsureWindowVisibility(
    const gfx::Rect& visible_area,
    int min_width,
    int min_height,
    gfx::Rect* bounds);

// Move the given bounds inside the given |visible_area| in parent coordinates,
// including a safety margin given by |kMinimumOnScreenArea|.
// This also ensures that the top of the bounds is visible.
ASH_EXPORT void AdjustBoundsToEnsureMinimumWindowVisibility(
    const gfx::Rect& visible_area,
    gfx::Rect* bounds);

// Returns the bounds of a left snapped window with default width in parent
// coordinates.
ASH_EXPORT gfx::Rect GetDefaultLeftSnappedWindowBoundsInParent(
    aura::Window* window);

// Returns the bounds of a right snapped window with default width in parent
// coordinates.
ASH_EXPORT gfx::Rect GetDefaultRightSnappedWindowBoundsInParent(
    aura::Window* window);

// Moves the window to the center of the display.
ASH_EXPORT void CenterWindow(aura::Window* window);

// Sets the bounds of |window| to |bounds_in_screen|. This may move |window|
// to |display| if necessary.
ASH_EXPORT void SetBoundsInScreen(aura::Window* window,
                                  const gfx::Rect& bounds_in_screen,
                                  const display::Display& display);

}  // namespace ash

#endif  // ASH_WM_WINDOW_POSITIONING_UTILS_H_
