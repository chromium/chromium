// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_UTILITY_ARC_CURVE_PATH_UTIL_H_
#define ASH_UTILITY_ARC_CURVE_PATH_UTIL_H_

#include <optional>

#include "ash/ash_export.h"
#include "ui/gfx/geometry/size.h"

class SkPath;

namespace ash::util {

// Defines a corner shape consisting of arcs and lines. More info in README.
struct ASH_EXPORT ArcCurveCorner {
  // Indicates a corner location on a bounding rectangle.
  enum class CornerLocation {
    // Enum values are ordered counterclockwise. Do not reorder.
    kTopLeft,
    kBottomLeft,
    kBottomRight,
    kTopRight,

    kMin = kTopLeft,
    kMax = kTopRight
  };

  // NOTE: Ensure `size` accommodates both `concave_radius` and `convex_radius`.
  ArcCurveCorner(CornerLocation location,
                 const gfx::Size& size,
                 float concave_radius,
                 float convex_radius);

  CornerLocation location;

  // Parameters of an arc curve. More info in README.
  gfx::Size size;
  float concave_radius;
  float convex_radius;
};

// Returns the path of a bounding rectangle of `size` with all corners
// rounded with `corner_radius`.
// NOTE: Returns an empty SkPath if the given values are invalid.
ASH_EXPORT SkPath GetArcCurveRectPath(const gfx::Size& size,
                                      const size_t corner_radius);

// Returns the path of a bounding rectangle of `size` with the specified
// `arc_curve_corner`. The corners that are not `arc_curve_corner` are
// rounded with `corner_radius` if any.
// NOTE: Returns an empty SkPath if the given values are invalid.
ASH_EXPORT SkPath
GetArcCurveRectPath(const gfx::Size& size,
                    const ArcCurveCorner& arc_curve_corner,
                    const std::optional<size_t>& corner_radius);

}  // namespace ash::util

#endif  // ASH_UTILITY_ARC_CURVE_PATH_UTIL_H_
