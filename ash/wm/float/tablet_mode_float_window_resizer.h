// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FLOAT_TABLET_MODE_FLOAT_WINDOW_RESIZER_H_
#define ASH_WM_FLOAT_TABLET_MODE_FLOAT_WINDOW_RESIZER_H_

#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_resizer.h"

namespace ash {

// WindowResizer implementation for floated windows in tablet mode.
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
  // The location in parent passed to `Drag()`.
  gfx::PointF last_location_in_parent_;
};

}  // namespace ash

#endif  // ASH_WM_FLOAT_TABLET_MODE_FLOAT_WINDOW_RESIZER_H_
