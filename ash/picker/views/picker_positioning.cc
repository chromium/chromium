// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_positioning.h"

#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace {

// Padding to separate the Picker window from the caret.
constexpr gfx::Outsets kPaddingAroundCaret(4);

}  // namespace

gfx::Rect GetPickerAnchorBounds(const gfx::Rect& caret_bounds,
                                const gfx::Point& cursor_point,
                                const gfx::Rect& focused_window_bounds) {
  if (caret_bounds != gfx::Rect() &&
      focused_window_bounds.Contains(caret_bounds)) {
    gfx::Rect anchor_rect = caret_bounds;
    anchor_rect.Outset(kPaddingAroundCaret);
    return anchor_rect;
  } else {
    return gfx::Rect(cursor_point, gfx::Size());
  }
}

}  // namespace ash
