// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FLOAT_TABLET_MODE_FLOAT_WINDOW_RESIZER_H_
#define ASH_WM_FLOAT_TABLET_MODE_FLOAT_WINDOW_RESIZER_H_

#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_resizer.h"

namespace ash {

class SplitViewDragIndicators;
class WindowState;

// TODO(crbug.com/1351562): The following constants are the same the drag window
// from shelf feature. They need to be changed for this feature, or moved to a
// shared location.

// A window has to be dragged toward the direction of the edge of the screen for
// a minimum of `kMinDragDistance` to a point within `kScreenEdgeInsetForSnap`
// of the edge of the screen, or dragged inside `kDistanceEdge` from edge to be
// snapped.
constexpr int kMinDragDistance = 96;
constexpr int kScreenEdgeInsetForSnap = 48;

// WindowResizer implementation for floated windows in tablet mode.
// TODO(crbug.com/1338715): This resizer adds the most basic dragging. It needs
// to stick to edges and magnetize to corners on release.
class TabletModeFloatWindowResizer : public WindowResizer {
 public:
  explicit TabletModeFloatWindowResizer(WindowState* window_state);
  TabletModeFloatWindowResizer(const TabletModeFloatWindowResizer&) = delete;
  TabletModeFloatWindowResizer& operator=(const TabletModeFloatWindowResizer&) =
      delete;
  ~TabletModeFloatWindowResizer() override;

  // WindowResizer:
  void Drag(const gfx::PointF& location_in_parent, int event_flags) override;
  void CompleteDrag() override;
  void RevertDrag() override;
  void FlingOrSwipe(ui::GestureEvent* event) override;

 private:
  // Responsible for showing an indication of whether the dragged window will be
  // snapped on drag complete.
  std::unique_ptr<SplitViewDragIndicators> split_view_drag_indicators_;

  // The location in parent passed to `Drag()`.
  gfx::PointF last_location_in_parent_;

  // The snap position computed in `Drag()`. It is then cached for use in
  // `CompleteDrag()`.
  SplitViewController::SnapPosition snap_position_ =
      SplitViewController::SnapPosition::kNone;
};

}  // namespace ash

#endif  // ASH_WM_FLOAT_TABLET_MODE_FLOAT_WINDOW_RESIZER_H_
