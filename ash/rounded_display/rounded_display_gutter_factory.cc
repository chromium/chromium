// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rounded_display/rounded_display_gutter_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/rounded_display/rounded_display_gutter.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace {

using Gutters = std::vector<RoundedDisplayGutter*>;
using RoundedCorner = RoundedDisplayGutter::RoundedCorner;
using RoundedCornerPosition = RoundedDisplayGutter::RoundedCorner::Position;

// Create RoundedCorner for a given position of the display.
RoundedDisplayGutter::RoundedCorner CreateRoundedCornerForDisplay(
    RoundedCornerPosition position,
    const gfx::RoundedCornersF& panel_radii,
    const gfx::Size& panel_size) {
  switch (position) {
    case RoundedCornerPosition::kUpperLeft:
      return RoundedCorner(position, panel_radii.upper_left(),
                           gfx::Point(0, 0));
    case RoundedCornerPosition::kUpperRight:
      return RoundedCorner(
          position, panel_radii.upper_right(),
          gfx::Point(panel_size.width() - panel_radii.upper_right(), 0));
    case RoundedCornerPosition::kLowerLeft:
      return RoundedCorner(
          position, panel_radii.lower_left(),
          gfx::Point(0, panel_size.height() - panel_radii.lower_left()));
    case RoundedCornerPosition::kLowerRight:
      return RoundedCorner(
          position, panel_radii.lower_right(),
          gfx::Point(panel_size.width() - panel_radii.lower_right(),
                     panel_size.height() - panel_radii.lower_right()));
  }
}

// Create the gutter with RoundedCorners encoded in
// `corner_positions_bit_mask`. We do not create the gutter if all of
// rounded_corners of the gutter have zero radius.
std::unique_ptr<RoundedDisplayGutter> CreateGutter(
    const gfx::Size& panel_size,
    const gfx::RoundedCornersF& panel_radii,
    int corner_positions_bit_mask,
    bool is_overlay_gutter) {
  std::vector<RoundedCorner> corners;

  for (RoundedDisplayGutter::RoundedCorner::Position position :
       {RoundedCorner::kLowerLeft, RoundedCorner::kLowerRight,
        RoundedCorner::kUpperLeft, RoundedCorner::kUpperRight}) {
    if (corner_positions_bit_mask & position) {
      corners.push_back(
          CreateRoundedCornerForDisplay(position, panel_radii, panel_size));
    }
  }

  // We only create a gutter if at least one its corners paint.
  for (const auto& corner : corners) {
    if (corner.DoesPaint()) {
      return RoundedDisplayGutter::CreateGutter(std::move(corners),
                                                is_overlay_gutter);
    }
  }

  return nullptr;
}

void MaybeAppendGutter(
    std::vector<std::unique_ptr<RoundedDisplayGutter>>& gutters,
    std::unique_ptr<RoundedDisplayGutter> gutter) {
  if (gutter) {
    gutters.push_back(std::move(gutter));
  }
}

}  // namespace

std::vector<std::unique_ptr<RoundedDisplayGutter>>
RoundedDisplayGutterFactory::CreateOverlayGutters(
    const gfx::Size& panel_size,
    const gfx::RoundedCornersF& panel_radii,
    bool create_vertical_gutters) {
  std::vector<std::unique_ptr<RoundedDisplayGutter>> gutters;

  if (create_vertical_gutters) {
    // Left overlay gutter.
    MaybeAppendGutter(gutters,
                      CreateGutter(panel_size, panel_radii,
                                   RoundedCornerPosition::kUpperLeft |
                                       RoundedCornerPosition::kLowerLeft,
                                   /*is_overlay_gutter=*/true));

    // Right overlay gutter.
    MaybeAppendGutter(gutters,
                      CreateGutter(panel_size, panel_radii,
                                   RoundedCornerPosition::kUpperRight |
                                       RoundedCornerPosition::kLowerRight,
                                   /*is_overlay_gutter=*/true));

  } else {
    // Upper overlay gutter.
    MaybeAppendGutter(gutters,
                      CreateGutter(panel_size, panel_radii,
                                   RoundedCornerPosition::kUpperLeft |
                                       RoundedCornerPosition::kUpperRight,
                                   /*is_overlay_gutter=*/true));

    // Lower overlay gutter.
    MaybeAppendGutter(gutters,
                      CreateGutter(panel_size, panel_radii,
                                   RoundedCornerPosition::kLowerLeft |
                                       RoundedCornerPosition::kLowerRight,
                                   /*is_overlay_gutter=*/true));
  }

  return gutters;
}

}  // namespace ash
