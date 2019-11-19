// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_HIGHLIGHT_VIEW_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_HIGHLIGHT_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "base/optional.h"
#include "ui/views/view.h"

namespace ash {

class RoundedRectView;
class SplitViewHighlightViewTestApi;

// View that is used for displaying and animating the highlights which tell
// users where to drag windows to enter splitview, and previews the space which
// a snapped window will occupy. It is a view consisting of a rectangle with
// rounded corners on the left or top, a rectangle in the middle and a rectangle
// with rounded corners on the right or bottom. It is done this way to ensure
// rounded corners remain the same during the duration of an animation.
// (Transforming a rounded rect will stretch the corners, and having to repaint
// every animation tick is expensive.)
//
// Although rounded corners are prevented from stretching along one axis, there
// is one animation where the rounded corners will stretch along the
// perpendicular axis. Specifically, the preview area has a small inset (on all
// four sides) until you actually snap the window, and then the preview area
// animates to nix that inset while fading out. So the rounded corners will
// stretch by an amount depending on the dimensions of the work area, but it is
// unlikely to be noticeable under normal circumstances.
class ASH_EXPORT SplitViewHighlightView : public views::View {
 public:
  explicit SplitViewHighlightView(bool is_right_or_bottom);
  ~SplitViewHighlightView() override;

  // Updates bounds, animating if |animation_type| has a value.
  void SetBounds(const gfx::Rect& bounds,
                 bool landscape,
                 const base::Optional<SplitviewAnimationType>& animation_type);

  void SetColor(SkColor color);

  // Called to update the opacity of the highlights view on transition from
  // |previous_window_dragging_state| to |window_dragging_state|. If
  // |previews_only|, then there shall be no visible drag indicators except for
  // snap previews. The highlights are white if |can_dragged_window_be_snapped|,
  // black otherwise.
  void OnWindowDraggingStateChanged(
      SplitViewDragIndicators::WindowDraggingState window_dragging_state,
      SplitViewDragIndicators::WindowDraggingState
          previous_window_dragging_state,
      bool previews_only,
      bool can_dragged_window_be_snapped);

 private:
  friend class SplitViewHighlightViewTestApi;

  // The three components of this view.
  RoundedRectView* left_top_ = nullptr;
  RoundedRectView* right_bottom_ = nullptr;
  views::View* middle_ = nullptr;

  bool landscape_ = true;
  // Determines whether this particular highlight view is located at the right
  // or bottom of the screen. These highlights animate in the opposite direction
  // as left or top highlights, so when we use SetBounds extra calucations have
  // to be done to ensure the animation is correct.
  const bool is_right_or_bottom_;

  DISALLOW_COPY_AND_ASSIGN(SplitViewHighlightView);
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_HIGHLIGHT_VIEW_H_
