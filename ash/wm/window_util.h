// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_UTIL_H_
#define ASH_WM_WINDOW_UTIL_H_

#include <stdint.h>
#include <vector>

#include "ash/ash_export.h"
#include "ui/base/ui_base_types.h"
#include "ui/wm/core/window_util.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
class Rect;
}

namespace ash {

namespace window_util {

// See ui/wm/core/window_util.h for ActivateWindow(), DeactivateWindow(),
// IsActiveWindow() and CanActivateWindow().
ASH_EXPORT aura::Window* GetActiveWindow();
ASH_EXPORT aura::Window* GetFocusedWindow();

// Returns the window with capture, null if no window currently has capture.
ASH_EXPORT aura::Window* GetCaptureWindow();

// Returns the Windows that may block events.
// If |min_container| is non-null then windows that are not children of
// |min_container| or not stacked above (z-order) will not receive events.
// |system_modal_container| is the window system modal windows appear in. If
// there is a system modal window in it, then events that are not targetted
// at the active modal window (or an ancestor or transient ancestor) will not
// receive events.
ASH_EXPORT void GetBlockingContainersForRoot(
    aura::Window* root_window,
    aura::Window** min_container,
    aura::Window** system_modal_container);

// Returns true if |window|'s location can be controlled by the user.
ASH_EXPORT bool IsWindowUserPositionable(aura::Window* window);

// Pins the window on top of other windows.
ASH_EXPORT void PinWindow(aura::Window* window, bool trusted);

// Indicates that the window should autohide the shelf when it is the active
// window.
ASH_EXPORT void SetAutoHideShelf(aura::Window* window, bool autohide);

// Moves |window| to the root window for the given |display_id|, if it is not
// already in the same root window. Returns true if |window| was moved.
ASH_EXPORT bool MoveWindowToDisplay(aura::Window* window, int64_t display_id);

// Convenience for window->delegate()->GetNonClientComponent(location) that
// returns HTNOWHERE if window->delegate() is null.
ASH_EXPORT int GetNonClientComponent(aura::Window* window,
                                     const gfx::Point& location);

// When set, the child windows should get a slightly larger hit region to make
// resizing easier.
ASH_EXPORT void SetChildrenUseExtendedHitRegionForWindow(aura::Window* window);

// Requests the |window| to close and destroy itself. This is intended to
// forward to an associated widget.
ASH_EXPORT void CloseWidgetForWindow(aura::Window* window);

// Installs a resize handler on the window that makes it easier to resize
// the window.
ASH_EXPORT void InstallResizeHandleWindowTargeterForWindow(
    aura::Window* window);

// Returns true if |window| is currently in tab-dragging process.
ASH_EXPORT bool IsDraggingTabs(const aura::Window* window);

// Returns true if |window| should be excluded from the cycle list and/or
// overview.
ASH_EXPORT bool ShouldExcludeForCycleList(const aura::Window* window);
ASH_EXPORT bool ShouldExcludeForOverview(const aura::Window* window);

// Removes all windows in |out_window_list| whose transient root is also in
// |out_window_list|. This is used by overview and window cycler to avoid
// showing multiple previews for windows linked by transient.
ASH_EXPORT void RemoveTransientDescendants(
    std::vector<aura::Window*>* out_window_list);

// Hides a list of |windows| without any animations, in case users wants to hide
// them right away or apply their own animations. Setting |minimize| to true
// will result in also setting the window states to minimized.
ASH_EXPORT void HideAndMaybeMinimizeWithoutAnimation(
    std::vector<aura::Window*> windows,
    bool minimize);

// Returns the RootWindow at |point_in_screen| in virtual screen coordinates.
// Returns nullptr if the root window does not exist at the given point.
ASH_EXPORT aura::Window* GetRootWindowAt(const gfx::Point& point_in_screen);

// Returns the RootWindow that shares the most area with |rect_in_screen| in
// virtual screen coordinates.
ASH_EXPORT aura::Window* GetRootWindowMatching(const gfx::Rect& rect_in_screen);

// Returns true if |window| is an ARC app window.
ASH_EXPORT bool IsArcWindow(const aura::Window* window);

// Returns true if |window| is an ARC PIP window.
ASH_EXPORT bool IsArcPipWindow(const aura::Window* window);

}  // namespace window_util
}  // namespace ash

#endif  // ASH_WM_WINDOW_UTIL_H_
