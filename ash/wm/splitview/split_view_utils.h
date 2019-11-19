// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_UTILS_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_UTILS_H_

#include "ash/ash_export.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

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
  SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT,
  // Used to fade in and out the other highlight. There are normally two
  // highlights, one on each side. When entering a state with a preview
  // highlight, one highlight is the preview highlight, and the other highlight
  // is the other highlight.
  SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN,
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

// Animates |layer|'s opacity based on |type|.
void DoSplitviewOpacityAnimation(ui::Layer* layer, SplitviewAnimationType type);

// Animates |layer|'s transform based on |type|.
void DoSplitviewTransformAnimation(ui::Layer* layer,
                                   SplitviewAnimationType type,
                                   const gfx::Transform& target_transform);

// Restores split view and overview based on the current split view's state.
// If |refresh_snapped_windows| is true, it will update the left and right
// snapped windows based on the MRU windows snapped states.
void MaybeRestoreSplitView(bool refresh_snapped_windows);

// Returns true if we allow dragging an overview window to snap to split view in
// clamshell mode.
ASH_EXPORT bool IsClamshellSplitViewModeEnabled();

// Checks multi-display support for overview and split view.
ASH_EXPORT bool AreMultiDisplayOverviewAndSplitViewEnabled();

// Returns true if split view mode is supported.
ASH_EXPORT bool ShouldAllowSplitView();

// Returns true if |window| can be activated and snapped in split view.
ASH_EXPORT bool CanSnapInSplitview(aura::Window* window);

// Displays a toast notifying users the application selected for split view is
// not compatible.
ASH_EXPORT void ShowAppCannotSnapToast();

ASH_EXPORT bool IsPhysicalLeftOrTop(SplitViewController::SnapPosition position);

// Returns the desired snap position based on |location_in_screen|. The window
// needs to be dragged into the drag indicator area on the edge of the screen
// to be able to get snapped.
ASH_EXPORT SplitViewController::SnapPosition GetSnapPosition(
    aura::Window* window,
    const gfx::Point& location_in_screen,
    const gfx::Rect& work_area);

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_UTILS_H_
