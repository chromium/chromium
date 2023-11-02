// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_UTILS_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_UTILS_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/transform.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

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
  aura::Window* const window_;

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
// current screen. |initial_location_in_screen| is the location at drag start if
// the drag began in |root_window|, and is empty otherwise. To be snappable
// (meaning the return value is not |SplitViewController::SnapPosition::kNone|),
// |location_in_screen| must be either inside |snap_distance_from_edge| or
// dragged toward the edge for at least |minimum_drag_distance| distance until
// it's dragged into a suitable edge of the work area of |root_window| (i.e.,
// |horizontal_edge_inset| if dragged horizontally to snap, or
// |vertical_edge_inset| if dragged vertically).
SplitViewController::SnapPosition GetSnapPositionForLocation(
    aura::Window* root_window,
    const gfx::Point& location_in_screen,
    const absl::optional<gfx::Point>& initial_location_in_screen,
    int snap_distance_from_edge,
    int minimum_drag_distance,
    int horizontal_edge_inset,
    int vertical_edge_inset);

// Returns the desired snap position. To be snappable, |window| must 1)
// satisfy |SplitViewController::CanSnapWindow| for |root_window|, and
// 2) be snappable according to |GetSnapPositionForLocation| above.
// |initial_location_in_screen| is the window location at drag start in
// its initial window. Otherwise, the arguments are the same as above.
ASH_EXPORT SplitViewController::SnapPosition GetSnapPosition(
    aura::Window* root_window,
    aura::Window* window,
    const gfx::Point& location_in_screen,
    const gfx::Point& initial_location_in_screen,
    int snap_distance_from_edge,
    int minimum_drag_distance,
    int horizontal_edge_inset,
    int vertical_edge_inset);

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_UTILS_H_
