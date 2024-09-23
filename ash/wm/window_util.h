// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_UTIL_H_
#define ASH_WM_WINDOW_UTIL_H_

#include <stdint.h>

#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/wm_metrics.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/wm/core/window_util.h"

class PrefRegistrySimple;

namespace gfx {
class Point;
class Rect;
class RectF;
}  // namespace gfx

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace views {
class BubbleDialogDelegate;
class View;
}  // namespace views

namespace ash::window_util {

ASH_EXPORT int GetMiniWindowRoundedCornerRadius();

// Returns the rounded corners for a mini window representation of
// `source_window`. It takes into account if the `source_window`
// belongs to a snap group or not.
// If `include_header_rounding` is false, function returns the radii of only
// bottom two corners of mini window.
ASH_EXPORT gfx::RoundedCornersF GetMiniWindowRoundedCorners(
    const aura::Window* source_window,
    bool include_header_rounding,
    std::optional<float> scale = std::nullopt);

// See ui/wm/core/window_util.h for ActivateWindow(), DeactivateWindow(),
// IsActiveWindow() and CanActivateWindow().
ASH_EXPORT aura::Window* GetActiveWindow();
ASH_EXPORT aura::Window* GetFocusedWindow();

// Returns true if `win1` is stacked (not directly) below `win2`. Note that this
// API only applies for windows with the same direct parent.
ASH_EXPORT bool IsStackedBelow(aura::Window* win1, aura::Window* win2);

// Returns the top most window for the given `windows` list by first finding the
// lowest common parent of all the `windows` and then finding the window among
// `windows` that is top-most in terms of z-order. Note that this doesn't take
// account of the visibility of the windows.
ASH_EXPORT aura::Window* GetTopMostWindow(const aura::Window::Windows& windows);

// Sort the windows in `window_set` according to their stacking order in the
// window tree. Windows which are descendants of a different root window will be
// returned in an arbitrary order relative to each-other.
ASH_EXPORT std::vector<aura::Window*> SortWindowsBottomToTop(
    std::set<raw_ptr<aura::Window, SetExperimental>> window_set);

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

// Returns true if `window` is currently in tab-dragging process.
ASH_EXPORT bool IsDraggingTabs(const aura::Window* window);

// Returns true if `window` should be excluded from the cycle list and/or
// overview.
ASH_EXPORT bool ShouldExcludeForCycleList(const aura::Window* window);
ASH_EXPORT bool ShouldExcludeForOverview(const aura::Window* window);

// Removes all windows in |out_window_list| whose transient root is also in
// |out_window_list|. Also replaces transient descendants with their transient
// roots, ensuring only one unique instance of each transient root. This is used
// by overview and window cycler to avoid showing multiple previews for windows
// linked by transient and creating items using transient descendants.
ASH_EXPORT void EnsureTransientRoots(
    std::vector<raw_ptr<aura::Window, VectorExperimental>>* out_window_list);

// Minimizes a hides list of |windows| without any animations.
ASH_EXPORT void MinimizeAndHideWithoutAnimation(
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows);

// Returns the RootWindow at |point_in_screen| in virtual screen coordinates.
// Returns nullptr if the root window does not exist at the given point.
ASH_EXPORT aura::Window* GetRootWindowAt(const gfx::Point& point_in_screen);

// Returns the RootWindow that shares the most area with |rect_in_screen| in
// virtual screen coordinates.
ASH_EXPORT aura::Window* GetRootWindowMatching(const gfx::Rect& rect_in_screen);

// Returns true if |window| is an ARC PIP window.
ASH_EXPORT bool IsArcPipWindow(const aura::Window* window);

// Expands the Android PIP window.
ASH_EXPORT void ExpandArcPipWindow();

// Returns true if any window is being dragged, or we are in overview mode and
// an item is being dragged around.
bool IsAnyWindowDragged();

// Adjusts the z-order stacking of `window_to_fix` in its parent to match its
// order in the MRU window list. This is done after the window is moved from one
// parent container to another by means of calling `AddChild()` which adds it as
// the top-most window, which doesn't necessarily match the MRU order.
// `window_to_fix` must be a child of a desk container, and the root of a
// transient hierarchy (if it belongs to one).
// This function must be called after `AddChild()` was called to add the
// `window_to_fix`.
void FixWindowStackingAccordingToGlobalMru(aura::Window* window_to_fix);

// Returns the top window on MRU window list, or null if the list is empty.
ASH_EXPORT aura::Window* GetTopWindow();
ASH_EXPORT aura::Window* GetTopNonFloatedWindow();

// Returns the floated window for the active desk if it exists.
ASH_EXPORT aura::Window* GetFloatedWindowForActiveDesk();

// Returns whether the top window should be minimized on back action.
ASH_EXPORT bool ShouldMinimizeTopWindowOnBack();

// Returns true if `window` is in minimized state, or is in floated state and
// tucked to the side in tablet mode.
ASH_EXPORT bool IsMinimizedOrTucked(aura::Window* window);

// Sends |ui::VKEY_BROWSER_BACK| key press and key release event to the
// WindowTreeHost associated with |root_window|.
void SendBackKeyEvent(aura::Window* root_window);

// Iterates through all the windows in the transient tree associated with
// |window| that are visible.
WindowTransientDescendantIteratorRange GetVisibleTransientTreeIterator(
    aura::Window* window);

// Applies the `transform` to `window` and all of its transient children,
// except those with `kExcludeFromTransientTreeTransformKey` set to true.
// Note `transform` is the transform that is applied to `window` and needs to be
// adjusted for the transient child windows.
ASH_EXPORT void SetTransform(aura::Window* window,
                             const gfx::Transform& transform);

// Calculates the bounds of the |transformed_window|. Those bounds are a union
// of all regular (normal and panel) windows in the |transformed_window|'s
// transient hierarchy. The returned Rect is in screen coordinates. The returned
// bounds are adjusted to allow the original |transformed_window|'s header to be
// hidden if |top_inset| is not zero.
ASH_EXPORT gfx::RectF GetTransformedBounds(aura::Window* transformed_window,
                                int top_inset);

// Returns the `BubbleDialogDelegate` associated with the given
// `transient_window`, if it's a bubble dialog.
ASH_EXPORT views::BubbleDialogDelegate* AsBubbleDialogDelegate(
    aura::Window* transient_window);

// Returns the `DialogDelegate` associated with the given `transient_window`, if
// it's a dialog.
views::DialogDelegate* AsDialogDelegate(aura::Window* transient_window);

// If multi profile is on, check if |window| should be shown for the current
// user.
bool ShouldShowForCurrentUser(aura::Window* window);

ASH_EXPORT aura::Window* GetEventHandlerForEvent(const ui::LocatedEvent& event);

// TODO(zxdan): Remove this method after all related code being migrated to the
// new way of getting input device settings. Note: this method is being
// deprecated. Please use IsNaturalScrollOn(const ui::ScrollEvent&).
ASH_EXPORT bool IsNaturalScrollOn();

// Checks the device settings to see if natural scroll for the touchpad is
// turned on.
ASH_EXPORT bool IsNaturalScrollOn(const ui::ScrollEvent& event);

// The thumbnail window (transformed window for non-minimized state in overview,
// mirror window for minimized state in overview and alt+tab windows) may need
// to be rounded depending on the state of the backdrop. This helper takes into
// account the rounded corners of `backdrop_view`, if it exists. Used for
// overview and alt+tab.
ASH_EXPORT bool ShouldRoundThumbnailWindow(
    views::View* backdrop_view,
    const gfx::RectF& thumbnail_bounds_in_screen);

// Returns the target snap ratio for the given `window` or
// `chromeos::kDefaultSnapRatio` if the target snap ratio doesn't exist.
float GetSnapRatioForWindow(aura::Window* window);

// Registers the per-profile preferences for whether faster splitscreen setup is
// enabled.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns true if `SplitViewOverviewSession` is created through faster split
// screen setup, i.e. partial overview is started on the other side of the
// screen when `window` is snapped.
bool IsInFasterSplitScreenSetupSession(const aura::Window* window);

// Returns true if overview is in session in clamshell mode and any overview
// grid is in faster splitview. This is a specific mode during which we don't
// show the desk bar or save desk buttons.
bool IsInFasterSplitScreenSetupSession();

// Returns the target bounds of `window` in screen coordinates.
ASH_EXPORT gfx::Rect GetTargetScreenBounds(aura::Window* window);

}  // namespace ash::window_util

#endif  // ASH_WM_WINDOW_UTIL_H_
