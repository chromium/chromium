// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ROUNDED_RECT_CUTOUT_PATH_BUILDER_H_
#define ASH_STYLE_ROUNDED_RECT_CUTOUT_PATH_BUILDER_H_

#include <optional>

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/geometry/size_f.h"

namespace ash {

// Generates a clip path to render rounded corners and cutouts of arbitrary
// size and number (up to one cutout per corner).
//
// ______
// |    |__ < Cutout Upper Right
// |       |
// |       |
// |_______|
class ASH_EXPORT RoundedRectCutoutPathBuilder {
 public:
  // Possible corners that can have a cutout in counter-clockwise order.
  enum class Corner { kUpperLeft, kLowerLeft, kLowerRight, kUpperRight };

  // Constructs a builder where the desired clipping region size is `bounds`.
  // Due to the default values of the corner radii, the minimum dimensions are
  // 32 X 32 px.
  RoundedRectCutoutPathBuilder(gfx::SizeF bounds);
  RoundedRectCutoutPathBuilder(const RoundedRectCutoutPathBuilder&);
  RoundedRectCutoutPathBuilder& operator=(const RoundedRectCutoutPathBuilder&);
  ~RoundedRectCutoutPathBuilder();

  // Sets the radius of the corners of the rectangle formed by bounds (not part
  // of the cutout).
  RoundedRectCutoutPathBuilder& CornerRadius(int radius);

  // Add a cutout at `corner` of `size`. Each `corner` may have a cutout.
  // Setting a `corner` multiple times will replace that cutout. A `size` of
  // zero will result in that cutout being removed.
  RoundedRectCutoutPathBuilder& AddCutout(Corner corner, gfx::SizeF size);

  // Sets the radius of the interior corner of the cutout.
  RoundedRectCutoutPathBuilder& CutoutInnerCornerRadius(int radius);

  // Sets the radius of the two exterior corners in the cutout.
  RoundedRectCutoutPathBuilder& CutoutOuterCornerRadius(int radius);

  // Returns the configured path. May CHECK if the path cannot be drawn if the
  // `bounds` or corner sizes are not sufficiently large for the configured
  // radii.
  SkPath Build();

 private:
  gfx::SizeF bounds_;

  // The radius of the corners without the cutout.
  int corner_radius_ = 16;

  // Radii of the corners in the cutout.
  int cutout_inner_corner_radius_ = 16;
  int cutout_outer_corner_radius_ = 12;

  // Dimensions of each cutout.
  base::flat_map<Corner, gfx::SizeF> cutouts_;
};

}  // namespace ash

#endif  // ASH_STYLE_ROUNDED_RECT_CUTOUT_PATH_BUILDER_H_
