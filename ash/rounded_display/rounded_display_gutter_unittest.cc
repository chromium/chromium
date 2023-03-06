// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rounded_display/rounded_display_gutter.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "base/check.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ash {
namespace {

// TODO(zoraiznaeem): Write pixel tests for various types of gutter textures.
using RoundedCorner = RoundedDisplayGutter::RoundedCorner;
using RoundedCornerPosition = RoundedDisplayGutter::RoundedCorner::Position;

constexpr int kCornerRadiusInPixels_16 = 16;
constexpr int kCornerRadiusInPixels_14 = 14;

// If a rounded-corner of radius r, is enclosed in a square of length
// r, the ratio of area of corner to area of square is always 1-pie/4. It is the
// expected ratio of pixels drawn into buffer of size r^2.
constexpr float kExpectedDrawnPixelsToTotalRatio = 0.2146;

// This tolerance works for rounded corner of radius 16 or 14. If different
// values of radius are used to draw the corner, the tolerance value needs to be
// changed. The bigger the corner radius, the lower the tolerance should be and
// vice versa.
constexpr float kTolerance = 0.077;

// Calculate the ratio of drawn pixels to total pixels in the grid enclosed
// between `start_row` to `start_row` + `end_row` and `start_col` to `start_col`
// + `end_col`.
float CalculateDrawnPixelsRatio(SkBitmap& bitmap,
                                int start_row,
                                int end_row,
                                int start_col,
                                int end_col) {
  DCHECK(start_row >= 0 && start_col >= 0);
  DCHECK(start_row < bitmap.height() && start_col < bitmap.width());
  DCHECK(end_row >= 0 && end_col >= 0);
  DCHECK(end_row < bitmap.height() && end_col < bitmap.width());

  int n_drawn_pixels = 0;

  for (int row = start_row; row <= end_row; row++) {
    for (int col = start_col; col <= end_col; col++) {
      // Any non transparent pixel is counted as a drawn pixel.
      if (bitmap.getColor4f(col, row).fA > 0.0) {
        n_drawn_pixels++;
      }
    }
  }

  int n_pixels = (end_row - start_row + 1) * (end_col - start_col + 1);

  return static_cast<float>(n_drawn_pixels) / n_pixels;
}

// By sampling pixel colors, this method tests if a correct type of
// RoundedCorner of `radius` is painted in the bitmap at position
// (`start_row`, `start_col`).
testing::AssertionResult TestCornerIsDrawn(SkBitmap& bitmap,
                                           int start_row,
                                           int start_col,
                                           int radius,
                                           RoundedCorner::Position position) {
  int end_row = start_row + radius - 1;
  int end_col = start_col + radius - 1;

  float drawn_pixels_ratio =
      CalculateDrawnPixelsRatio(bitmap, start_row, end_row, start_col, end_col);

  // We expect 1 - pie/4 pixels to be drawn for corner mask texture but given
  // the discrete nature of pixels and blending at the edges, we
  // expect to have drawn more pixels.
  EXPECT_NEAR(drawn_pixels_ratio, kExpectedDrawnPixelsToTotalRatio, kTolerance);
  // We overestimate number of drawn pixels.
  EXPECT_GE(drawn_pixels_ratio, kExpectedDrawnPixelsToTotalRatio);

  // For the mask texture, this pixel will always be a transparent pixel.
  EXPECT_EQ(
      bitmap.getColor(start_col + (radius * 0.5), start_row + (radius * 0.5)),
      SK_ColorTRANSPARENT);

  int delta_move_quarter = radius * 0.25 - 1;
  int delta_move_three_fourth = radius * 0.75 + 1;

  // This pixel will only be drawn for the upper left corner.
  EXPECT_EQ(bitmap.getColor(start_col + delta_move_quarter,
                            start_row + delta_move_quarter),
            position == RoundedCorner::Position::kUpperLeft
                ? SK_ColorBLACK
                : SK_ColorTRANSPARENT);

  // This pixel will only be drawn for the lower left corner.
  EXPECT_EQ(bitmap.getColor(start_col + delta_move_three_fourth,
                            start_row + delta_move_quarter),
            position == RoundedCorner::Position::kUpperRight
                ? SK_ColorBLACK
                : SK_ColorTRANSPARENT);

  // This pixel will only be drawn for the upper right corner.
  EXPECT_EQ(bitmap.getColor(start_col + delta_move_quarter,
                            start_row + delta_move_three_fourth),
            position == RoundedCorner::Position::kLowerLeft
                ? SK_ColorBLACK
                : SK_ColorTRANSPARENT);

  // This pixel will only be drawn for the lower right corner.
  EXPECT_EQ(bitmap.getColor(start_col + delta_move_three_fourth,
                            start_row + delta_move_three_fourth),
            position == RoundedCorner::Position::kLowerRight
                ? SK_ColorBLACK
                : SK_ColorTRANSPARENT);

  return ::testing::AssertionSuccess();
}

testing::AssertionResult TestGutterHasCornerDrawn(
    SkBitmap& texture_bitmap,
    const RoundedCorner& corner,
    const RoundedDisplayGutter* gutter) {
  const gfx::Vector2d& offset =
      corner.bounds().OffsetFromOrigin() - gutter->bounds().OffsetFromOrigin();

  return TestCornerIsDrawn(texture_bitmap, offset.y(), offset.x(),
                           corner.radius(), corner.position());
}

// We minimally adjust the `bounds` to either have the same width or height of
// the `gutter_bounds`.
gfx::Rect AdjustToGutterBounds(gfx::Rect bounds, gfx::Rect gutter_bounds) {
  gfx::Rect adjusted_bounds(bounds);

  if (gutter_bounds.width() >= gutter_bounds.height()) {
    adjusted_bounds.set_height(gutter_bounds.height());
  }

  if (gutter_bounds.width() < gutter_bounds.height()) {
    adjusted_bounds.set_width(gutter_bounds.width());
  }

  return adjusted_bounds;
}

// Note: This method can only calculate the not drawn region of gutter with two
// adjacent corners.
gfx::Rect CalculateNotDrawnRegionOfGutter(
    const RoundedDisplayGutter* gutter,
    const std::vector<RoundedCorner>& corners) {
  gfx::Rect not_drawn_region = gutter->bounds();

  for (auto& corner : corners) {
    if (corner.DoesPaint()) {
      // To subtract the corner bounds from gutter bounds, corner bound needs
      // complete intersection with gutter bounds either in x or y direction.
      gfx::Rect adjusted_corner_bounds =
          AdjustToGutterBounds(corner.bounds(), gutter->bounds());
      not_drawn_region.Subtract(adjusted_corner_bounds);
    }
  }

  not_drawn_region.Offset(-gutter->bounds().OffsetFromOrigin());
  return not_drawn_region;
}

// Gets the bitmap of texture created by the `gutter`.
SkBitmap GetGutterSkBitmap(RoundedDisplayGutter* gutter) {
  gfx::Canvas canvas(gutter->bounds().size(), 1.0, true);
  gutter->Paint(&canvas);
  return canvas.GetBitmap();
}

class RoundedDisplayGutterTest : public testing::Test {
 public:
  RoundedDisplayGutterTest() = default;

  RoundedDisplayGutterTest(const RoundedDisplayGutterTest&) = delete;
  RoundedDisplayGutterTest& operator=(const RoundedDisplayGutterTest&) = delete;
};

TEST_F(RoundedDisplayGutterTest, BoundsOfGuttersWithOneCorner) {
  {
    RoundedCorner corner(RoundedCornerPosition::kUpperLeft,
                         kCornerRadiusInPixels_16, gfx::Point(0, 0));

    std::vector<RoundedCorner> corners;
    corners.emplace_back(std::move(corner));

    auto gutter = RoundedDisplayGutter::CreateGutter(std::move(corners),
                                                     /*is_overlay=*/true);

    EXPECT_EQ(gutter->bounds(), gfx::Rect(0, 0, kCornerRadiusInPixels_16,
                                          kCornerRadiusInPixels_16));
  }
  {
    RoundedCorner corner(RoundedCornerPosition::kUpperLeft,
                         kCornerRadiusInPixels_16, gfx::Point(15, 15));

    std::vector<RoundedCorner> corners;
    corners.emplace_back(std::move(corner));

    auto gutter = RoundedDisplayGutter::CreateGutter(std::move(corners),
                                                     /*is_overlay=*/true);
    EXPECT_EQ(gutter->bounds(), gfx::Rect(15, 15, kCornerRadiusInPixels_16,
                                          kCornerRadiusInPixels_16));
  }
}

TEST_F(RoundedDisplayGutterTest, BoundsOfGuttersWithTwoCorners) {
  {
    // Corners are horizontally adjacent to each other.
    RoundedCorner upper_left_corner(RoundedCornerPosition::kUpperLeft,
                                    kCornerRadiusInPixels_16, gfx::Point(0, 0));
    RoundedCorner upper_right_corner(RoundedCornerPosition::kUpperRight,
                                     kCornerRadiusInPixels_14,
                                     gfx::Point(100, 0));

    std::vector<RoundedCorner> corners;
    corners.emplace_back(std::move(upper_left_corner));
    corners.emplace_back(std::move(upper_right_corner));

    auto gutter = RoundedDisplayGutter::CreateGutter(std::move(corners),
                                                     /*is_overlay=*/true);

    // Expect the bounds to union of both corner bounds.
    EXPECT_EQ(gutter->bounds(), gfx::Rect(0, 0, 100 + kCornerRadiusInPixels_14,
                                          kCornerRadiusInPixels_16));
  }
  {
    // Corners are vertically adjacent to each other.
    RoundedCorner upper_right_corner(RoundedCornerPosition::kUpperRight,
                                     kCornerRadiusInPixels_16,
                                     gfx::Point(50, 0));
    RoundedCorner lower_right_corner(RoundedCornerPosition::kLowerRight,
                                     kCornerRadiusInPixels_16,
                                     gfx::Point(50, 100));

    std::vector<RoundedCorner> corners;
    corners.emplace_back(std::move(upper_right_corner));
    corners.emplace_back(std::move(lower_right_corner));

    auto gutter = RoundedDisplayGutter::CreateGutter(std::move(corners),
                                                     /*is_overlay=*/true);

    // Expect the bounds to union of both corner bounds.
    EXPECT_EQ(gutter->bounds(), gfx::Rect(50, 0, kCornerRadiusInPixels_16,
                                          100 + kCornerRadiusInPixels_16));
  }
  {
    // Corners are not adjacent to each other.
    RoundedCorner upper_left_corner(RoundedCornerPosition::kUpperLeft,
                                    kCornerRadiusInPixels_16, gfx::Point(0, 0));
    RoundedCorner lower_right_corner(RoundedCornerPosition::kLowerRight,
                                     kCornerRadiusInPixels_16,
                                     gfx::Point(50, 50));

    std::vector<RoundedCorner> corners;
    corners.emplace_back(std::move(upper_left_corner));
    corners.emplace_back(std::move(lower_right_corner));

    auto gutter = RoundedDisplayGutter::CreateGutter(std::move(corners),
                                                     /*is_overlay=*/true);

    // Expect the bounds to union of both corner bounds.
    EXPECT_EQ(gutter->bounds(), gfx::Rect(0, 0, 50 + kCornerRadiusInPixels_16,
                                          50 + kCornerRadiusInPixels_16));
  }

  {
    // One of the corners has zero radius.
    RoundedCorner upper_left_corner(RoundedCornerPosition::kUpperLeft, 0,
                                    gfx::Point(0, 0));
    RoundedCorner lower_right_corner(RoundedCornerPosition::kLowerRight,
                                     kCornerRadiusInPixels_16,
                                     gfx::Point(50, 50));

    std::vector<RoundedCorner> corners;
    corners.emplace_back(std::move(upper_left_corner));
    corners.emplace_back(std::move(lower_right_corner));

    auto gutter = RoundedDisplayGutter::CreateGutter(std::move(corners),
                                                     /*is_overlay=*/true);

    // Expect the bounds to be that of non-zero corner.
    EXPECT_EQ(gutter->bounds(), gfx::Rect(50, 50, kCornerRadiusInPixels_16,
                                          kCornerRadiusInPixels_16));
  }
}

TEST_F(RoundedDisplayGutterTest, BoundsOfGuttersWithThreeCorners) {
  {
    // Corners are horizontally adjacent to each other.
    RoundedCorner upper_left_corner(RoundedCornerPosition::kUpperLeft,
                                    kCornerRadiusInPixels_16, gfx::Point(0, 0));
    RoundedCorner upper_right_corner(RoundedCornerPosition::kUpperRight,
                                     kCornerRadiusInPixels_14,
                                     gfx::Point(100, 0));
    RoundedCorner lower_left_corner(RoundedCornerPosition::kLowerLeft,
                                    kCornerRadiusInPixels_14,
                                    gfx::Point(0, 50));

    std::vector<RoundedCorner> corners;
    corners.emplace_back(std::move(upper_left_corner));
    corners.emplace_back(std::move(upper_right_corner));
    corners.emplace_back(std::move(lower_left_corner));

    auto gutter = RoundedDisplayGutter::CreateGutter(std::move(corners),
                                                     /*is_overlay=*/true);

    // Expect the bounds to union of all corner bounds.
    EXPECT_EQ(gutter->bounds(), gfx::Rect(0, 0, 100 + kCornerRadiusInPixels_14,
                                          50 + kCornerRadiusInPixels_14));
  }
}

TEST_F(RoundedDisplayGutterTest, CorrectTextures_GutterHasSingleCorner) {
  {
    RoundedCorner upper_right_corner(RoundedCornerPosition::kUpperRight,
                                     kCornerRadiusInPixels_16,
                                     gfx::Point(0, 0));

    std::vector<RoundedCorner> corners;
    corners.emplace_back(std::move(upper_right_corner));

    auto gutter = RoundedDisplayGutter::CreateGutter(std::move(corners),
                                                     /*is_overlay=*/true);

    auto& gutter_corners = gutter->GetGutterCorners();
    SkBitmap texture_bitmap = GetGutterSkBitmap(gutter.get());

    EXPECT_TRUE(TestGutterHasCornerDrawn(texture_bitmap, gutter_corners.at(0),
                                         gutter.get()));
  }
  {
    RoundedCorner upper_left_corner(RoundedCornerPosition::kUpperLeft,
                                    kCornerRadiusInPixels_16, gfx::Point(0, 0));

    std::vector<RoundedCorner> corners;
    corners.emplace_back(std::move(upper_left_corner));

    auto gutter = RoundedDisplayGutter::CreateGutter(std::move(corners),
                                                     /*is_overlay=*/true);

    auto& gutter_corners = gutter->GetGutterCorners();
    SkBitmap texture_bitmap = GetGutterSkBitmap(gutter.get());

    EXPECT_TRUE(TestGutterHasCornerDrawn(texture_bitmap, gutter_corners.at(0),
                                         gutter.get()));
  }
  {
    RoundedCorner lower_left_corner(RoundedCornerPosition::kLowerLeft,
                                    kCornerRadiusInPixels_14, gfx::Point(0, 0));

    std::vector<RoundedCorner> corners;
    corners.emplace_back(std::move(lower_left_corner));

    auto gutter = RoundedDisplayGutter::CreateGutter(std::move(corners),
                                                     /*is_overlay=*/true);

    auto& gutter_corners = gutter->GetGutterCorners();
    SkBitmap texture_bitmap = GetGutterSkBitmap(gutter.get());

    EXPECT_TRUE(TestGutterHasCornerDrawn(texture_bitmap, gutter_corners.at(0),
                                         gutter.get()));
  }
  {
    RoundedCorner lower_right_corner(RoundedCornerPosition::kLowerRight,
                                     kCornerRadiusInPixels_14,
                                     gfx::Point(0, 0));

    std::vector<RoundedCorner> corners;
    corners.emplace_back(std::move(lower_right_corner));

    auto gutter = RoundedDisplayGutter::CreateGutter(std::move(corners),
                                                     /*is_overlay=*/true);

    auto& gutter_corners = gutter->GetGutterCorners();
    SkBitmap texture_bitmap = GetGutterSkBitmap(gutter.get());

    EXPECT_TRUE(TestGutterHasCornerDrawn(texture_bitmap, gutter_corners.at(0),
                                         gutter.get()));
  }
}

TEST_F(RoundedDisplayGutterTest,
       GutterHasTwoVerticallyAdjacentCorners_BothCornersPainted) {
  RoundedCorner upper_right_corner(RoundedCornerPosition::kUpperRight,
                                   kCornerRadiusInPixels_16, gfx::Point(50, 0));
  RoundedCorner lower_right_corner(RoundedCornerPosition::kLowerRight,
                                   kCornerRadiusInPixels_16,
                                   gfx::Point(50, 100));

  std::vector<RoundedCorner> corners;
  corners.emplace_back(std::move(upper_right_corner));
  corners.emplace_back(std::move(lower_right_corner));

  auto gutter = RoundedDisplayGutter::CreateGutter(std::move(corners),
                                                   /*is_overlay=*/true);

  SkBitmap texture_bitmap = GetGutterSkBitmap(gutter.get());
  auto& gutter_corners = gutter->GetGutterCorners();

  EXPECT_TRUE(TestGutterHasCornerDrawn(texture_bitmap, gutter_corners.at(0),
                                       gutter.get()));
  EXPECT_TRUE(TestGutterHasCornerDrawn(texture_bitmap, gutter_corners.at(1),
                                       gutter.get()));

  gfx::Rect not_drawn_region =
      CalculateNotDrawnRegionOfGutter(gutter.get(), gutter_corners);

  // We should not draw any thing between corners.
  EXPECT_EQ(CalculateDrawnPixelsRatio(texture_bitmap, not_drawn_region.y(),
                                      not_drawn_region.bottom() - 1,
                                      not_drawn_region.x(),
                                      not_drawn_region.right() - 1),
            0);
}

TEST_F(RoundedDisplayGutterTest,
       GutterHasTwoHorizontallyAdjacentCorners_BothCornersPainted) {
  RoundedCorner upper_left_corner(RoundedCornerPosition::kUpperLeft,
                                  kCornerRadiusInPixels_16, gfx::Point(0, 0));
  RoundedCorner upper_right_corner(RoundedCornerPosition::kUpperRight,
                                   kCornerRadiusInPixels_16,
                                   gfx::Point(100, 0));

  std::vector<RoundedCorner> corners;
  corners.emplace_back(std::move(upper_left_corner));
  corners.emplace_back(std::move(upper_right_corner));

  auto gutter = RoundedDisplayGutter::CreateGutter(std::move(corners),
                                                   /*is_overlay=*/true);

  SkBitmap texture_bitmap = GetGutterSkBitmap(gutter.get());
  auto& gutter_corners = gutter->GetGutterCorners();

  EXPECT_TRUE(TestGutterHasCornerDrawn(texture_bitmap, gutter_corners.at(0),
                                       gutter.get()));
  EXPECT_TRUE(TestGutterHasCornerDrawn(texture_bitmap, gutter_corners.at(1),
                                       gutter.get()));

  gfx::Rect not_drawn_region =
      CalculateNotDrawnRegionOfGutter(gutter.get(), gutter_corners);

  // We should not draw any thing between corners.
  EXPECT_EQ(CalculateDrawnPixelsRatio(texture_bitmap, not_drawn_region.y(),
                                      not_drawn_region.bottom() - 1,
                                      not_drawn_region.x(),
                                      not_drawn_region.right() - 1),
            0);
}

TEST_F(RoundedDisplayGutterTest,
       GutterHasTwoAdjacentCornersWithDifferentRadii) {
  RoundedCorner upper_left_corner(RoundedCornerPosition::kUpperLeft,
                                  kCornerRadiusInPixels_16, gfx::Point(0, 0));
  RoundedCorner upper_right_corner(RoundedCornerPosition::kUpperRight,
                                   kCornerRadiusInPixels_14,
                                   gfx::Point(100, 0));

  std::vector<RoundedCorner> corners;
  corners.emplace_back(std::move(upper_left_corner));
  corners.emplace_back(std::move(upper_right_corner));

  auto gutter = RoundedDisplayGutter::CreateGutter(std::move(corners),
                                                   /*is_overlay=*/true);

  SkBitmap texture_bitmap = GetGutterSkBitmap(gutter.get());
  auto& gutter_corners = gutter->GetGutterCorners();

  EXPECT_TRUE(TestGutterHasCornerDrawn(texture_bitmap, gutter_corners.at(0),
                                       gutter.get()));
  EXPECT_TRUE(TestGutterHasCornerDrawn(texture_bitmap, gutter_corners.at(1),
                                       gutter.get()));

  gfx::Rect not_drawn_region =
      CalculateNotDrawnRegionOfGutter(gutter.get(), gutter_corners);

  // We should not draw any thing between corners.
  EXPECT_EQ(CalculateDrawnPixelsRatio(texture_bitmap, not_drawn_region.y(),
                                      not_drawn_region.bottom() - 1,
                                      not_drawn_region.x(),
                                      not_drawn_region.right() - 1),
            0);
}

TEST_F(RoundedDisplayGutterTest, GutterHasFourCorners_AllCornersPainted) {
  RoundedCorner upper_left_corner(RoundedCornerPosition::kUpperLeft,
                                  kCornerRadiusInPixels_16, gfx::Point(0, 0));
  RoundedCorner upper_right_corner(RoundedCornerPosition::kUpperRight,
                                   kCornerRadiusInPixels_16,
                                   gfx::Point(100, 0));
  RoundedCorner lower_left_corner(RoundedCornerPosition::kLowerLeft,
                                  kCornerRadiusInPixels_14, gfx::Point(0, 100));
  RoundedCorner lower_right_corner(RoundedCornerPosition::kLowerRight,
                                   kCornerRadiusInPixels_14,
                                   gfx::Point(100, 100));

  std::vector<RoundedCorner> corners;
  corners.emplace_back(std::move(upper_left_corner));
  corners.emplace_back(std::move(upper_right_corner));
  corners.emplace_back(std::move(lower_left_corner));
  corners.emplace_back(std::move(lower_right_corner));

  auto gutter = RoundedDisplayGutter::CreateGutter(std::move(corners),
                                                   /*is_overlay=*/true);

  SkBitmap texture_bitmap = GetGutterSkBitmap(gutter.get());
  auto& gutter_corners = gutter->GetGutterCorners();

  EXPECT_TRUE(TestGutterHasCornerDrawn(texture_bitmap, gutter_corners.at(0),
                                       gutter.get()));
  EXPECT_TRUE(TestGutterHasCornerDrawn(texture_bitmap, gutter_corners.at(1),
                                       gutter.get()));
  EXPECT_TRUE(TestGutterHasCornerDrawn(texture_bitmap, gutter_corners.at(2),
                                       gutter.get()));
  EXPECT_TRUE(TestGutterHasCornerDrawn(texture_bitmap, gutter_corners.at(3),
                                       gutter.get()));
}

TEST_F(RoundedDisplayGutterTest, GutterHasTwoAdjacentCorners_OneCornerPainted) {
  RoundedCorner upper_left_corner(RoundedCornerPosition::kUpperLeft, 0,
                                  gfx::Point(0, 0));
  RoundedCorner lower_right_corner(RoundedCornerPosition::kLowerRight,
                                   kCornerRadiusInPixels_16,
                                   gfx::Point(50, 50));

  std::vector<RoundedCorner> corners;
  corners.emplace_back(std::move(upper_left_corner));
  corners.emplace_back(std::move(lower_right_corner));

  auto gutter = RoundedDisplayGutter::CreateGutter(std::move(corners),
                                                   /*is_overlay=*/true);

  SkBitmap texture_bitmap = GetGutterSkBitmap(gutter.get());
  auto& gutter_corners = gutter->GetGutterCorners();

  // Since only one corner is drawn, there is no transparent region compared to
  // the textures of gutters with two or more drawn corners.
  EXPECT_TRUE(TestGutterHasCornerDrawn(texture_bitmap, gutter_corners.at(1),
                                       gutter.get()));
}

}  // namespace
}  // namespace ash
