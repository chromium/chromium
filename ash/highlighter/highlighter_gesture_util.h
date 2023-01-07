// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HIGHLIGHTER_HIGHLIGHTER_GESTURE_UTIL_H_
#define ASH_HIGHLIGHTER_HIGHLIGHTER_GESTURE_UTIL_H_

#include "ash/ash_export.h"
#include "ui/gfx/geometry/rect_f.h"

namespace fast_ink {
class FastInkPoints;
}

namespace ash {

// Highlighter gesture recognition result type. This enum is used to back
// an UMA histogram and should be treated as append-only.
enum class HighlighterGestureType {
  kNotRecognized = 0,
  kHorizontalStroke,
  kClosedShape,
  kGestureCount
};

// Returns the recognized gesture type.
HighlighterGestureType ASH_EXPORT
DetectHighlighterGesture(const gfx::RectF& box,
                         const gfx::SizeF& pen_tip_size,
                         const fast_ink::FastInkPoints& points);

}  // namespace ash

#endif  // ASH_HIGHLIGHTER_HIGHLIGHTER_GESTURE_UTIL_H_
