// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_UTILS_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_UTILS_H_

#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/window_positioning_utils.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/widget/widget.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

class SplitViewOverviewSession;

// Enum of the different splitview mode animations. Sorted by property
// (opacity/transform) and then alphabetically.
enum SplitviewAnimationType {
  // Used to fade in and out the highlights on either side which indicate where
  // to drag a selector item.
  SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN,
  SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN_CANNOT_SNAP,
  SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT,
  // Used to fade in and out the other highlight. There are normally two
  // highlights, one on each side. When entering a state with a preview
  // highlight, one highlight is the preview highlight, and the other highlight
  // is the other highlight.
  SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN,
  SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN_CANNOT_SNAP,
  SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_OUT,
  // Used to fade in and out the label on the overview item which warns users
  // the item cannot be snapped. The label appears on the overview item after
  // another window has been snapped.
  SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_IN,
  SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_OUT,
  // Used to fade in and out the preview area highlight which indicates the
  // bounds of the window that is about to get snapped.
  SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_IN,
  SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_OUT,
  // Used to fade in and out the labels which appear on either side of overview
  // mode when a overview item is selected. They indicate where to drag the
  // selector item if it is snappable, or if an item cannot be snapped.
  SPLITVIEW_ANIMATION_TEXT_FADE_IN,
  SPLITVIEW_ANIMATION_TEXT_FADE_OUT,
  // Used when the text fades in or out with the highlights, as opposed to
  // fading in when the highlights change bounds. Has slightly different
  // animation values.
  SPLITVIEW_ANIMATION_TEXT_FADE_IN_WITH_HIGHLIGHT,
  SPLITVIEW_ANIMATION_TEXT_FADE_OUT_WITH_HIGHLIGHT,
  // Used to slide in and out the other highlight.
  SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_IN,
  SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_OUT,
  // Used to slide in and out the text label on the other highlight.
  SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_IN,
  SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_OUT,
  // Used to animate the inset of the preview area to nothing.
  SPLITVIEW_ANIMATION_PREVIEW_AREA_NIX_INSET,
  // Used to slide in and out the preview area highlight.
  SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN,
  SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_OUT,
  // Used to slide in and out the text label on the preview area highlight.
  SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_IN,
  SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_OUT,
  // Used to apply window transform on the selector item after it gets snapped
  // or on the dragged window after the drag ends.
  SPLITVIEW_ANIMATION_SET_WINDOW_TRANSFORM,
};

// This class observes the window transform animation and relayout the window's
// transient bubble dialogs when animation is completed. This is needed in some
// splitview and overview cases as in splitview or overview, the window can have
// an un-identity transform in place when its bounds changed. And when this
// happens, its transient bubble dialogs won't have the correct bounds as the
// bounds are calculated based on the transformed window bounds. We'll need to
// manually relayout the bubble dialogs after the window's transform reset to
// the identity transform so that the bubble dialogs can have correct bounds.
class ASH_EXPORT WindowTransformAnimationObserver
    : public ui::ImplicitAnimationObserver,
      public aura::WindowObserver {
 public:
  explicit WindowTransformAnimationObserver(aura::Window* window);
  ~WindowTransformAnimationObserver() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  const raw_ptr<aura::Window> window_;

  WindowTransformAnimationObserver(const WindowTransformAnimationObserver&) =
      delete;
  WindowTransformAnimationObserver& operator=(
      const WindowTransformAnimationObserver&) = delete;
};

// Animates |layer|'s opacity based on |type|.
void DoSplitviewOpacityAnimation(ui::Layer* layer, SplitviewAnimationType type);

// Animates |layer|'s transform based on |type|.
void DoSplitviewTransformAnimation(
    ui::Layer* layer,
    SplitviewAnimationType type,
    const gfx::Transform& target_transform,
    const std::vector<ui::ImplicitAnimationObserver*>& animation_observers);

// Animates |layer|'s clip rect based on |type|.
void DoSplitviewClipRectAnimation(
    ui::Layer* layer,
    SplitviewAnimationType type,
    const gfx::Rect& target_clip_rect,
    std::unique_ptr<ui::ImplicitAnimationObserver> animation_observer);

// Returns the `SplitViewOverviewSession` for the root window of `window`.
ASH_EXPORT SplitViewOverviewSession* GetSplitViewOverviewSession(
    aura::Window* window);

// Returns true if `window` is currently snapped.
bool IsSnapped(aura::Window* window);

// Returns the length of the window according to the screen orientation.
ASH_EXPORT int GetWindowLength(aura::Window* window, bool horizontal);

// Returns the corresponding `chromeos::WindowStateType` for the given
// `snap_position`.
chromeos::WindowStateType GetWindowStateTypeFromSnapPosition(
    SnapPosition snap_position);

// Returns the corresponding `SnapPosition` for the given
// `chromeos::WindowStateType`, which must be snapped.
SnapPosition ToSnapPosition(chromeos::WindowStateType type);

// Transforms `window` based on whether it is the primary or secondary window
// and its distance from `divider_position` during split view resizing.
void SetWindowTransformDuringResizing(aura::Window* window,
                                      int divider_position);

// Restores split view and overview based on the current split view's state.
// If |refresh_snapped_windows| is true, it will update the left and right
// snapped windows based on the MRU windows snapped states.
void MaybeRestoreSplitView(bool refresh_snapped_windows);

// Returns true if split view mode is supported.
ASH_EXPORT bool ShouldAllowSplitView();

// Displays a toast notifying users the application selected for split view is
// not compatible.
void ShowAppCannotSnapToast();

// Calculates the snap position for a dragged window at |location_in_screen|,
// ignoring any properties of the window itself. The |root_window| is of the
// current screen. `initial_location_in_screen` is the location at drag start if
// the drag began in `root_window`, and is empty otherwise. To be snappable
// (meaning the return value is not `SnapPosition::kNone`),
// `location_in_screen` must be either inside `snap_distance_from_edge` or
// dragged toward the edge for at least `minimum_drag_distance` distance until
// it's dragged into a suitable edge of the work area of `root_window` (i.e.,
// `horizontal_edge_inset` if dragged horizontally to snap, or
// `vertical_edge_inset` if dragged vertically).
SnapPosition GetSnapPositionForLocation(
    aura::Window* root_window,
    const gfx::Point& location_in_screen,
    const std::optional<gfx::Point>& initial_location_in_screen,
    int snap_distance_from_edge,
    int minimum_drag_distance,
    int horizontal_edge_inset,
    int vertical_edge_inset);

// Returns the desired snap position. To be snappable, |window| must 1)
// satisfy |SplitViewController::CanSnapWindow| for |root_window|, and
// 2) be snappable according to
// |GetSnapPositionForLocation| above.
// |initial_location_in_screen| is the window location at drag start in
// its initial window. Otherwise, the arguments are the same as above.
ASH_EXPORT SnapPosition
GetSnapPosition(aura::Window* root_window,
                aura::Window* window,
                const gfx::Point& location_in_screen,
                const gfx::Point& initial_location_in_screen,
                int snap_distance_from_edge,
                int minimum_drag_distance,
                int horizontal_edge_inset,
                int vertical_edge_inset);

// The return values of these two functions together indicate what actual
// positions correspond to |PRIMARY| and |SECONDARY|:
// |IsLayoutHorizontal|  |IsLayoutPrimary|    |PRIMARY|           |SECONDARY|
// --------------------------------------------------------------------------
// true                  true                   left                 right
// true                  false                  right                left
// false                 true                   top                  bottom
// false                 false                  bottom               top
// In both clamshell and tablet mode, these functions return values based on
// display orientation. |window| is used to find the nearest display to check
// if the display layout is horizontal and is primary or not.
ASH_EXPORT bool IsLayoutHorizontal(aura::Window* window);
ASH_EXPORT bool IsLayoutHorizontal(const display::Display& display);
ASH_EXPORT bool IsLayoutPrimary(aura::Window* window);
ASH_EXPORT bool IsLayoutPrimary(const display::Display& display);

// Returns true if `position` actually signifies a left or top position,
// according to the return values of `IsLayoutHorizontal` and
// `IsLayoutPrimary`. Physical position refers to the position of the window
// on the display that is held upward.
ASH_EXPORT bool IsPhysicallyLeftOrTop(SnapPosition position,
                                      aura::Window* window);
ASH_EXPORT bool IsPhysicallyLeftOrTop(SnapPosition position,
                                      const display::Display& display);

// Returns whether `window`'s snap position is actually in the left or top
// position based on whether the display is in primary screen orientation, where
// `window` must be snapped.
ASH_EXPORT bool IsPhysicallyLeftOrTop(aura::Window* window);

// Returns the maximum value of the `divider_position_`, which is the width of
// the current display's work area bounds in landscape orientation, or height
// of the current display's work area bounds in portrait orientation.
int GetDividerPositionUpperLimit(aura::Window* root_window);

// Returns the minimum length of the window according to the screen orientation.
ASH_EXPORT int GetMinimumWindowLength(aura::Window* window, bool horizontal);

// Returns the target divider position for `root_window` for `snap_ratio` at
// `snap_position`, clamped between 0 and the upper limit of `root_window`.
// `account_for_divider_width` will decide whether the divider shorter side
// width will be subtracted or not.
int CalculateDividerPosition(aura::Window* root_window,
                             SnapPosition snap_position,
                             float snap_ratio,
                             bool account_for_divider_width);

// Returns the divider position, the origin of where `window` is divided on the
// work area. This will be the window length if it is physically left or top, or
// the work area length - window length if it is physically right or bottom. If
// `account_for_divider_width` is true, it will also subtract
// `kSplitviewDividerShortSideLength / 2` from the window length if is
// physically left or top, or `kSplitviewDividerShortSideLength` to `window`
// length if it is physically right or bottom.
int GetEquivalentDividerPosition(aura::Window* window,
                                 bool account_for_divider_width);

// Returns the bounds of a snapped window at `snap_position`, where
// `divider_position` is the end of the primary window width.
// `account_for_divider_width` will decide whether the window bounds need to
// shrink to make room for the divider or not. `window_for_minimum_size` will be
// taken into consideration for the calculation while `is_resizing_with_divider`
// is false.
gfx::Rect CalculateSnappedWindowBoundsInScreen(
    SnapPosition snap_position,
    aura::Window* root_window,
    aura::Window* window_for_minimum_size,
    bool account_for_divider_width,
    int divider_position,
    bool is_resizing_with_divider);

// Returns the snap type of the window's `state_type`, which must be snapped.
// `snap_type` is guaranteed to be snapped already.
SnapViewType ToSnapViewType(chromeos::WindowStateType state_type);
chromeos::WindowStateType ToWindowStateType(SnapViewType snap_type);

// Returns the opposite snap type of `window`, where `window` must be snapped.
// `snap_type` is guaranteed to be snapped already.
SnapViewType GetOppositeSnapType(SnapViewType snap_type);
SnapViewType GetOppositeSnapType(aura::Window* window);

// Returns true if `snap_action_source` can be start faster split screen set up.
ASH_EXPORT bool CanSnapActionSourceStartFasterSplitView(
    WindowSnapActionSource snap_action_source);

// Returns true if `window` should be *excluded* from the occluded window check,
// e.g. if it is not visible or minimized or when it is a float or pip window.
// If this is true, `window` will be ignored when determining whether to show
// partial overview or consider the window for snap to replace.
bool ShouldExcludeForOcclusionCheck(const aura::Window* window,
                                    const aura::Window* target_root);

// Returns the set of windows which can be cycled through in the stacking order
// of the children of the active desk container of `root`. Note this excludes
// windows on other containers, e.g. always-on-top windows and floated windows.
aura::Window::Windows GetActiveDeskAppWindowsInZOrder(aura::Window* root);

// Returns the window that is fully visible (without occlusion) on the
// `target_root` and with the given `snap_type`, excluding `window_to_ignore`.
// Returns nullptr if no such window exists.
aura::Window* GetTopmostVisibleWindowOfSnapType(aura::Window* window_to_ignore,
                                                aura::Window* target_root,
                                                SnapViewType snap_type);

// Returns the window that is fully visible (without occlusion) and snapped to
// the opposite side of the given `window` on the same root window. Returns
// nullptr if no such window exists.
aura::Window* GetOppositeVisibleSnappedWindow(aura::Window* window);

// Given the windows `to_be_snapped` and `opposite_snapped`, returns the snap
// ratio gap or overlap that would be created by snapping them on opposite sides
// of each other.
ASH_EXPORT float GetSnapRatioGap(aura::Window* to_be_snapped,
                                 aura::Window* opposite_snapped);

// Given the windows `to_be_snapped` and `opposite_snapped`, returns true if the
// snap ratio gap or overlap between them is within the snap ratio threshold for
// auto-group and snap-to-replace.
bool IsSnapRatioGapWithinThreshold(aura::Window* to_be_snapped,
                                   aura::Window* opposite_snapped);

// Given `to_be_snapped_window`, the `target_root` it is being dragged to, and
// target `snap_type`, returns the auto-snap ratio for `to_be_snapped_window`
// that will be used if it can be added to a snap group.
float GetAutoSnapRatio(aura::Window* to_be_snapped_window,
                       aura::Window* target_root,
                       SnapViewType snap_type);

// Returns true if the given `window` can be considered as the candidate for
// faster split screen set up. Returns false otherwise. `snap_action_source` is
// used to filter out some unwanted snap sources.
bool ShouldConsiderWindowForSplitViewSetupView(
    aura::Window* window,
    WindowSnapActionSource snap_action_source);

// Returns true if `SplitViewOverviewSession` is allowed to start when the given
// `window` is snapped with given `snap_action_source`. Returns false otherwise.
bool CanStartSplitViewOverviewSessionInClamshell(
    aura::Window* window,
    WindowSnapActionSource snap_action_source);

// Returns true if the snap group is enabled in clamshell mode. The
// `split_view_divider_` will show to indicate that the two windows are in a
// snap-group state.
ASH_EXPORT bool IsSnapGroupEnabledInClamshellMode();

// Gets the expected window component for a window in split view, depending on
// current screen orientation for resizing purpose.
int GetWindowComponentForResize(aura::Window* window);

// Returns true if the split view divider exits which should be taken into
// consideration when calculating the snap ratio.
// TODO(b/329326366): Remove this API and have clients call
// `UpdateSnappedBounds()` directly.
bool ShouldConsiderDivider(aura::Window* window);

// Returns true if the minimum size of `window1` and `window2` and the divider
// width can fit in the work area. The windows should belong to the same root
// window.
bool CanWindowsFitInWorkArea(aura::Window* window1, aura::Window* window2);

// Builds the full histogram that records whether the window layout completes on
// `SplitViewOverviewSession` exit. The full histogram is shown in the example
// below:
// |------------prefix----------|-----root_word-------------------|
// "Ash.SplitViewOverviewSession.WindowLayoutCompleteOnSessionExit"
//                                                               |--ui_mode--|
//                                                            ".ClamshellMode",
ASH_EXPORT std::string BuildWindowLayoutCompleteOnSessionExitHistogram();

// Builds the full histogram that records the exit point of the
// `SplitViewOverviewSession` by inserting the `snap_action_source` and
// appending the ui mode suffix to build the full histogram name.
// The full histogram is shown in the example below:
// |------------prefix----------|-snap_action_source-|-root_word-|--ui_mode--|
// "Ash.SplitViewOverviewSession.DragWindowEdgeToSnap.ExitPoint.ClamshellMode".
ASH_EXPORT std::string BuildSplitViewOverviewExitPointHistogramName(
    WindowSnapActionSource snap_action_source);

// Builds the full histogram that records the pref value when a window is
// snapped.
// |----------prefix---------|-snap_action_source-|
// "Ash.SnapWindowSuggestions.DragWindowEdgeToSnap".
ASH_EXPORT std::string BuildSnapWindowSuggestionsHistogramName(
    WindowSnapActionSource snap_action_source);

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_UTILS_H_
