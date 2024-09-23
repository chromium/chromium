// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_FOCUS_INDICATOR_H_
#define ASH_PICKER_VIEWS_PICKER_FOCUS_INDICATOR_H_

#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Canvas;
class Point;
}  // namespace gfx

namespace ash {

// Paints a focus indicator onto `canvas`. The painted indicator looks like a
// vertical bar with half-rounded edges.
void PaintPickerFocusIndicator(gfx::Canvas* canvas,
                               const gfx::Point& origin,
                               int height,
                               SkColor color);

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_FOCUS_INDICATOR_H_
