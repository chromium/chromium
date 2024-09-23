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
inline constexpr int kMinimumOnScreenArea = 25;

// This specifies how much percent (30%) of a window rect must be visible when
// the window is added to the workspace.
inline constexpr float kMinimumPercentOnScreenArea = 0.3f;

// In clamshell mode, users can snap left/right for horizontal display and
// top/bottom for vertical display. For primary-landscape-oriented display,
// |kPrimary| and |kSecondary| are left snap and right snap.
// For other orientation see the table of description for
// `IsLayoutHorizontal()`.
enum class SnapViewType { kPrimary, kSecondary };

// Adjusts |bounds| so that the size does not exceed |max_size|.
ASH_EXPORT void AdjustBoundsSmallerThan(const gfx::Size& max_size,
                                        gfx::Rect* bounds);

// Adjusts the given `bounds` to guarantee its minimum visibility inside the
// `visible_area`. If `client_controlled` is true (e.g., arc apps), one more dip
// will be applied to the minimum size to keep a different minimum size as the
// non-client controlled bounds adjustment. This is done to avoid bounds
// adjustment loop, which might happen as setting the bounds of a
// client-controlled app is a 'request' and client might adjust the value on
// their side. This also ensures that the top of the `bounds` is visible. Note,
// the coordinate of the given `visible_area` and `bounds` should be aligned,
// either both in the screen coordinate or both in the parent coordinate.
ASH_EXPORT void AdjustBoundsToEnsureMinimumWindowVisibility(
    const gfx::Rect& visible_area,
    bool client_controlled,
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
