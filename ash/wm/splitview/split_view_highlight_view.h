// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_HIGHLIGHT_VIEW_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_HIGHLIGHT_VIEW_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// View that is used for displaying and animating the highlights which tell
// users where to drag windows to enter splitview, and previews the space which
// a snapped window will occupy. It is a view consists of one solid color layer
// with rounded corners. If animations are needed, they are performed by
// animating the layer's clip rect.
class ASH_EXPORT SplitViewHighlightView : public views::View {
  METADATA_HEADER(SplitViewHighlightView, views::View)

 public:
  explicit SplitViewHighlightView(bool is_right_or_bottom);
  ~SplitViewHighlightView() override;

  SplitViewHighlightView(const SplitViewHighlightView&) = delete;
  SplitViewHighlightView& operator=(const SplitViewHighlightView&) = delete;

  // Updates bounds, animating if |animation_type| has a value.
  void SetBounds(const gfx::Rect& bounds,
                 const std::optional<SplitviewAnimationType>& animation_type);

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
  // Determines whether this particular highlight view is located at the right
  // or bottom of the screen. These highlights animate in the opposite direction
  // as left or top highlights, so when we use |SetBounds()| extra calucations
  // have to be done to ensure the animation is correct.
  const bool is_right_or_bottom_;
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_HIGHLIGHT_VIEW_H_
