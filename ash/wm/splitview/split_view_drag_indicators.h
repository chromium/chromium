// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_DRAG_INDICATORS_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_DRAG_INDICATORS_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Widget;
}  // namespace views

namespace ash {

// Enum which contains the indicators that SplitViewDragIndicators can display.
// Converted to a bitmask to make testing easier.
enum class IndicatorType {
  kLeftHighlight = 1,
  kLeftText = 2,
  kRightHighlight = 4,
  kRightText = 8
};

// An overlay in overview mode which guides users while they are attempting to
// enter splitview. Displays text and highlights when dragging an overview
// window. Displays a highlight of where the window will end up when an overview
// window has entered a snap region.
class ASH_EXPORT SplitViewDragIndicators {
 public:
  // Enum for purposes of providing |SplitViewDragIndicators| with information
  // about window dragging.
  enum class WindowDraggingState {
    // Not dragging, or split view is unsupported (see |ShouldAllowSplitView|).
    kNoDrag,

    // Started dragging from overview or from the shelf. Split view is
    // supported. Not currently dragging in a snap area, or the dragged window
    // is not eligible to be snapped in split view.
    kFromOverview,

    // Started dragging from the top. Split view is supported. Not currently
    // dragging in a snap area, or the dragged window is not eligible to be
    // snapped in split view.
    kFromTop,

    // Started dragging from the shelf. Split view is supported. Not currently
    // dragging in a snap area, or the dragged window is not eligible to be
    // snapped in split view.
    kFromShelf,

    // Currently dragging in the |SplitViewController::LEFT| snap area, and the
    // dragged window is eligible to be snapped in split view.
    kToSnapLeft,

    // Currently dragging in the |SplitViewController::RIGHT| snap area, and the
    // dragged window is eligible to be snapped in split view.
    kToSnapRight
  };

  // |SplitViewController::LEFT|, if |window_dragging_state| is |kToSnapLeft|
  // |SplitViewController::RIGHT|, if |window_dragging_state| is |kToSnapRight|
  // |SplitViewController::NONE| otherwise
  static SplitViewController::SnapPosition GetSnapPosition(
      WindowDraggingState window_dragging_state);

  // |kNoDrag| if |is_dragging| is false or split view is unsupported. If
  // |is_dragging| is true and split view is supported, then:
  // |non_snap_state|, if |snap_position| is |SplitViewController::NONE|
  // |kToSnapLeft|, if |snap_position| is |SplitViewController::LEFT|
  // |kToSnapRight|, if |snap_position| is |SplitViewController::RIGHT|
  static WindowDraggingState ComputeWindowDraggingState(
      bool is_dragging,
      WindowDraggingState non_snap_state,
      SplitViewController::SnapPosition snap_position);

  SplitViewDragIndicators(aura::Window* root_window);
  ~SplitViewDragIndicators();

  void SetDraggedWindow(aura::Window* dragged_window);
  void SetWindowDraggingState(WindowDraggingState window_dragging_state);
  void OnDisplayBoundsChanged();
  bool GetIndicatorTypeVisibilityForTesting(IndicatorType type) const;
  gfx::Rect GetLeftHighlightViewBoundsForTesting() const;
  WindowDraggingState current_window_dragging_state() const {
    return current_window_dragging_state_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(SplitViewDragIndicatorsTest,
                           SplitViewDragIndicatorsWidgetReparenting);
  class RotatedImageLabelView;
  class SplitViewDragIndicatorsView;

  // The root content view of |widget_|.
  SplitViewDragIndicatorsView* indicators_view_ = nullptr;

  WindowDraggingState current_window_dragging_state_ =
      WindowDraggingState::kNoDrag;

  // The SplitViewDragIndicators widget. It covers the entire root window
  // and displays regions and text indicating where users should drag windows
  // enter split view.
  std::unique_ptr<views::Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(SplitViewDragIndicators);
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_DRAG_INDICATORS_H_
