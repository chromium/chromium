// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_POSITIONER_H_
#define ASH_WM_WINDOW_POSITIONER_H_

#include "ash/ash_export.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace ash::window_positioner {

// A collection of utilities that assist with placing new windows.

// Computes and returns the bounds and show state for new window based on the
// parameter passed AND existing windows. |is_saved_bounds| indicates the
// |bounds_in_out| is the saved bounds.
ASH_EXPORT void GetBoundsAndShowStateForNewWindow(
    bool is_saved_bounds,
    ui::mojom::WindowShowState show_state_in,
    gfx::Rect* bounds_in_out,
    ui::mojom::WindowShowState* show_state_out);

// Check if after removal or hide of the given |removed_window| an
// automated desktop location management can be performed and
// rearrange accordingly.
void RearrangeVisibleWindowOnHideOrRemove(const aura::Window* removed_window);

// Turn the automatic positioning logic temporarily off. Returns the previous
// state.
ASH_EXPORT bool DisableAutoPositioning(bool ignore);

// Check if after insertion or showing of the given |added_window|
// an automated desktop location management can be performed and
// rearrange accordingly.
void RearrangeVisibleWindowOnShow(aura::Window* added_window);

}  // namespace ash::window_positioner

#endif  // ASH_WM_WINDOW_POSITIONER_H_
