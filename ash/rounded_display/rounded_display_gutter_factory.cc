// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rounded_display/rounded_display_gutter_factory.h"

#include <array>
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
    const gfx::RoundedCornersF& display_radii,
    const gfx::Size& display_size) {
  switch (position) {
    case RoundedCornerPosition::kUpperLeft:
      return RoundedCorner(position, display_radii.upper_left(),
                           gfx::Point(0, 0));
    case RoundedCornerPosition::kUpperRight:
      return RoundedCorner(
          position, display_radii.upper_right(),
          gfx::Point(display_size.width() - display_radii.upper_right(), 0));
    case RoundedCornerPosition::kLowerLeft:
      return RoundedCorner(
          position, display_radii.lower_left(),
          gfx::Point(0, display_size.height() - display_radii.lower_left()));
    case RoundedCornerPosition::kLowerRight:
      return RoundedCorner(
          position, display_radii.lower_right(),
          gfx::Point(display_size.width() - display_radii.lower_right(),
                     display_size.height() - display_radii.lower_right()));
  }
}

// Create the gutter with RoundedCorners encoded in
// `corner_positions_bit_mask`. We do not create the gutter if all of
// rounded_corners of the gutter have zero radius.
std::unique_ptr<RoundedDisplayGutter> CreateGutter(
    const gfx::Size& display_size,
    const gfx::RoundedCornersF& display_radii,
    int corner_positions_bit_mask,
    bool is_overlay_gutter) {
  std::vector<RoundedCorner> corners;

  for (RoundedDisplayGutter::RoundedCorner::Position position :
       {RoundedCorner::kLowerLeft, RoundedCorner::kLowerRight,
        RoundedCorner::kUpperLeft, RoundedCorner::kUpperRight}) {
    if (corner_positions_bit_mask & position) {
      corners.push_back(
          CreateRoundedCornerForDisplay(position, display_radii, display_size));
    }
  }

  // We only create a gutter if at least one its corners paint.
  for (auto& corner : corners) {
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
    const gfx::Size& display_panel_size,
    const gfx::RoundedCornersF& display_radii,
    bool create_vertical_gutters) {
  std::vector<std::unique_ptr<RoundedDisplayGutter>> gutters;
  gutters.reserve(2);

  if (create_vertical_gutters) {
    // Left overlay gutter.
    MaybeAppendGutter(gutters,
                      CreateGutter(display_panel_size, display_radii,
                                   RoundedCornerPosition::kUpperLeft |
                                       RoundedCornerPosition::kLowerLeft,
                                   /*is_overlay_gutter=*/true));

    // Right overlay gutter.
    MaybeAppendGutter(gutters,
                      CreateGutter(display_panel_size, display_radii,
                                   RoundedCornerPosition::kUpperRight |
                                       RoundedCornerPosition::kLowerRight,
                                   /*is_overlay_gutter=*/true));

  } else {
    // Upper overlay gutter.
    MaybeAppendGutter(gutters,
                      CreateGutter(display_panel_size, display_radii,
                                   RoundedCornerPosition::kUpperLeft |
                                       RoundedCornerPosition::kUpperRight,
                                   /*is_overlay_gutter=*/true));

    // Lower overlay gutter.
    MaybeAppendGutter(gutters,
                      CreateGutter(display_panel_size, display_radii,
                                   RoundedCornerPosition::kLowerLeft |
                                       RoundedCornerPosition::kLowerRight,
                                   /*is_overlay_gutter=*/true));
  }

  return gutters;
}

std::vector<std::unique_ptr<RoundedDisplayGutter>>
RoundedDisplayGutterFactory::CreateNonOverlayGutters(
    const gfx::Size& display_panel_size,
    const gfx::RoundedCornersF& display_radii) {
  std::vector<std::unique_ptr<RoundedDisplayGutter>> gutters;
  gutters.reserve(4);

  MaybeAppendGutter(gutters, CreateGutter(display_panel_size, display_radii,
                                          0 | RoundedCornerPosition::kUpperLeft,
                                          /*is_overlay_gutter=*/false));
  MaybeAppendGutter(gutters,
                    CreateGutter(display_panel_size, display_radii,
                                 0 | RoundedCornerPosition::kUpperRight,
                                 /*is_overlay_gutter=*/false));
  MaybeAppendGutter(gutters, CreateGutter(display_panel_size, display_radii,
                                          0 | RoundedCornerPosition::kLowerLeft,
                                          /*is_overlay_gutter=*/false));
  MaybeAppendGutter(gutters,
                    CreateGutter(display_panel_size, display_radii,
                                 0 | RoundedCornerPosition::kLowerRight,
                                 /*is_overlay_gutter=*/false));

  return gutters;
}

}  // namespace ash
