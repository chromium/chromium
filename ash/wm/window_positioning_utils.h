// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_POSITIONING_UTILS_H_
#define ASH_WM_WINDOW_POSITIONING_UTILS_H_

#include "ash/ash_export.h"
#include "ash/display/screen_orientation_controller.h"

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

// In clamshell mode, users can snap left/right for horizontal display and
// top/bottom for vertical display. For primary-landscape-oriented display,
// |kPrimary| and |kSecondary| are left snap and right snap.
// For other orientation see the table of description for
// `SplitViewController::IsLayoutHorizontal()`.
enum class SnapViewType { kPrimary, kSecondary };

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

// Returns the bounds of a snapped window for a given snap |type| and
// |snap_ratio| in clamshell mode.
ASH_EXPORT gfx::Rect GetSnappedWindowBoundsInParent(aura::Window* window,
                                                    SnapViewType type,
                                                    float snap_ratio);

// Returns the bounds of a snapped window with default snapped ratio
// |kDefaultSnapRatio|, in parent coordinates.
ASH_EXPORT gfx::Rect GetDefaultSnappedWindowBoundsInParent(aura::Window* window,
                                                           SnapViewType type);

// Returns the bounds of a snapped window for |display| with |type| and
// |snap_ratio| in clamshell mode whatever coordinates are used for
// |work_area|. Typically, |display| should be the target display window is
// being dragged into.
ASH_EXPORT gfx::Rect GetSnappedWindowBounds(const gfx::Rect& work_area,
                                            const display::Display display,
                                            aura::Window* window,
                                            SnapViewType type,
                                            float snap_ratio);

// Returns the display orientation used for snapping windows in clamshell mode.
// If vertical snap state is not enabled, returns primary-landscape
// orientation. Otherwise, returns the current orientation relative to natural
// orientation of this |display|.
chromeos::OrientationType GetSnapDisplayOrientation(
    const display::Display& display);

// Sets the bounds of |window| to |bounds_in_screen|. This may move |window|
// to |display| if necessary.
ASH_EXPORT void SetBoundsInScreen(aura::Window* window,
                                  const gfx::Rect& bounds_in_screen,
                                  const display::Display& display);

}  // namespace ash

#endif  // ASH_WM_WINDOW_POSITIONING_UTILS_H_
