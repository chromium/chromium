// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PIP_PIP_WINDOW_RESIZER_H_
#define ASH_WM_PIP_PIP_WINDOW_RESIZER_H_

#include "ash/ash_export.h"
#include "ash/wm/window_resizer.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point_f.h"

namespace gfx {
class Rect;
}

namespace ash {

class WindowState;

// Controls resizing for windows with the PIP window state type. This
// includes things like snapping the PIP window to the edges of the work area
// and handling swipe-to-dismiss.
class ASH_EXPORT PipWindowResizer : public WindowResizer {
 public:
  explicit PipWindowResizer(WindowState* window_state);

  PipWindowResizer(const PipWindowResizer&) = delete;
  PipWindowResizer& operator=(const PipWindowResizer&) = delete;

  ~PipWindowResizer() override;

  // WindowResizer:
  void Drag(const gfx::PointF& location_in_parent, int event_flags) override;
  void Pinch(const gfx::PointF& location, float scale) override;
  void CompleteDrag() override;
  void RevertDrag() override;
  void FlingOrSwipe(ui::GestureEvent* event) override;

 private:
  WindowState* window_state() { return window_state_; }

  // Invoked during pinch to calculate the window bounds.
  // `location_in_parent` is the current location of the gesture.
  gfx::Rect CalculateBoundsForPinch(
      const gfx::PointF& location_in_parent) const;

  gfx::Rect ComputeFlungPosition();

  std::optional<gfx::PointF> last_location_in_screen_;
  bool last_event_was_pinch_ = false;
  int fling_velocity_x_ = 0;
  int fling_velocity_y_ = 0;
  float in_screen_fraction_ = 1.f;
  float accumulated_scale_ = 1.f;
  bool moved_or_resized_ = false;
  bool may_dismiss_horizontally_ = false;
  bool may_dismiss_vertically_ = false;
};

}  // namespace ash

#endif  // ASH_WM_PIP_PIP_WINDOW_RESIZER_H_
