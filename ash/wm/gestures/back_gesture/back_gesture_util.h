// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_UTIL_H_
#define ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_UTIL_H_

#include "cc/paint/paint_flags.h"

namespace gfx {
class Canvas;
class PointF;
class Rect;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

namespace ash {

// Paints the circular shaped highlight border onto `canvas` for `view`.
void DrawCircleHighlightBorder(views::View* view,
                               gfx::Canvas* canvas,
                               const gfx::PointF& circle_center,
                               int radius);

// Paints the round rectangular shaped highlight border onto `canvas` for
// `view`.
void DrawRoundRectHighlightBorder(views::View* view,
                                  gfx::Canvas* canvas,
                                  const gfx::Rect& bounds,
                                  int corner_radius);

}  // namespace ash

#endif  // ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_UTIL_H_
