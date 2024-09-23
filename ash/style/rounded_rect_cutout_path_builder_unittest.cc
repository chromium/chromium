// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/rounded_rect_cutout_path_builder.h"

#include <ostream>

#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/geometry/size_f.h"

// Pretty print SkRect for failure logs.
void PrintTo(const SkRect& size, std::ostream* os) {
  *os << "(" << size.x() << ", " << size.y() << ") [" << size.width() << ", "
      << size.height() << "]";
}

namespace ash {
namespace {

constexpr gfx::SizeF kViewSize(114.f, 432.f);

TEST(RoundedRectCutoutPathBuilderTest, RectanglePoints) {
  RoundedRectCutoutPathBuilder builder(kViewSize);
  builder.CornerRadius(0);
  SkPath path = builder.Build();
  // A radius of 0 should be a rectangle so we have 4 points and the starting
  // point.
  EXPECT_EQ(path.countPoints(), 4 + 1);
}

TEST(RoundedRectCutoutPathBuilderTest, RoundedCorners) {
  RoundedRectCutoutPathBuilder builder(kViewSize);
  builder.CornerRadius(16);
  SkPath path = builder.Build();
  // A rounded rect has 12 points (3 for each ronded corner, one for the control
  // point in conic and two for the start and end) and the starting
  // point.
  EXPECT_EQ(path.countPoints(), 12 + 1);
}

TEST(RoundedRectCutoutPathBuilderTest, OneCutout) {
  RoundedRectCutoutPathBuilder builder(kViewSize);
  builder.AddCutout(RoundedRectCutoutPathBuilder::Corner::kLowerRight,
                    gfx::SizeF(30, 30));
  builder.CornerRadius(0);
  SkPath path = builder.Build();

  // Cutouts have 3 rounded corners each. Each rounded corner has 3 points (so 9
  // total). There are 3 other corners and the starting point. 13 total.
  EXPECT_EQ(path.countPoints(), 9 + 3 + 1);
}

TEST(RoundedRectCutoutPathBuilderTest, TwoCutouts) {
  RoundedRectCutoutPathBuilder builder(kViewSize);
  builder
      .AddCutout(RoundedRectCutoutPathBuilder::Corner::kLowerRight,
                 gfx::SizeF(30, 30))
      .AddCutout(RoundedRectCutoutPathBuilder::Corner::kUpperRight,
                 gfx::SizeF(40, 20))
      .CornerRadius(0);
  SkPath path = builder.Build();

  // 2 cutouts, 2 normal corners, and the starting point.
  EXPECT_EQ(path.countPoints(), 9 + 9 + 2 + 1);

  // Ensure the path is complete.
  EXPECT_TRUE(path.isLastContourClosed());
  // The bounds should be equal to the View that the path will clip.
  EXPECT_THAT(
      path.getBounds(),
      testing::Eq(SkRect::MakeSize({kViewSize.width(), kViewSize.height()})));
}

TEST(RoundedRectCutoutPathBuilderTest, RemoveCutout) {
  RoundedRectCutoutPathBuilder builder(kViewSize);
  builder.CornerRadius(0);
  // Add cutout.
  builder.AddCutout(RoundedRectCutoutPathBuilder::Corner::kUpperLeft,
                    gfx::SizeF(40, 40));
  // Remove the cutout.
  builder.AddCutout(RoundedRectCutoutPathBuilder::Corner::kUpperLeft,
                    gfx::SizeF());

  // The resulting path should be a rectangle.
  SkPath path = builder.Build();
  EXPECT_EQ(path.countPoints(), 5);

  SkRect bounds;
  EXPECT_TRUE(path.isRect(&bounds));
  EXPECT_THAT(
      bounds,
      testing::Eq(SkRect::MakeSize({kViewSize.width(), kViewSize.height()})));
}

TEST(RoundedRectCutoutPathBuilderTest, ExtraLargeCutout) {
  RoundedRectCutoutPathBuilder builder(gfx::SizeF{100.0f, 100.0f});

  // Add cutout that is more than half the height.
  builder.AddCutout(RoundedRectCutoutPathBuilder::Corner::kUpperLeft,
                    gfx::SizeF(55.0f, 55.0f));

  SkPath path = builder.Build();
  // 3 * 3 points for the rounded corners. 9 points in the cutout. 1 starting
  // point.
  EXPECT_EQ(path.countPoints(), 9 + 9 + 1);

  SkRect bounds = path.getBounds();
  EXPECT_THAT(bounds, testing::Eq(SkRect::MakeSize({100.0f, 100.0f})));
}

TEST(RoundedRectCutoutPathBuilderDeathTest, MaximumCutout) {
  RoundedRectCutoutPathBuilder builder(gfx::SizeF{100.0f, 100.0f});
  builder.CornerRadius(4);
  builder.CutoutOuterCornerRadius(8);

  // cutout + outer corner + corner radius is allowed to equal the bounds.
  // 4 + 8 + 88 = 100.
  builder.AddCutout(RoundedRectCutoutPathBuilder::Corner::kUpperLeft,
                    gfx::SizeF(88.0f, 55.0f));

  SkPath path = builder.Build();
  EXPECT_FALSE(path.isEmpty());
}

TEST(RoundedRectCutoutPathBuilderDeathTest, CutoutTooLarge) {
  RoundedRectCutoutPathBuilder builder(gfx::SizeF{100.0f, 100.0f});
  builder.CornerRadius(4);
  builder.CutoutOuterCornerRadius(8);

  // When cutout + outer corner + corner radius is larger than the
  // bounds, we expect a crash. 4 + 8 + 89 = 101.
  builder.AddCutout(RoundedRectCutoutPathBuilder::Corner::kUpperLeft,
                    gfx::SizeF(89.0f, 55.0f));

  EXPECT_CHECK_DEATH_WITH(
      {
        // This should crash because the cutout is larger than the bounds.
        SkPath path = builder.Build();
      },
      "must be less than or equal to bounds");
}

TEST(RoundedRectCutoutPathBuilderDeathTest, CutoutsIntersect) {
  RoundedRectCutoutPathBuilder builder(gfx::SizeF{100.0f, 100.0f});
  builder.CornerRadius(8);
  // Technically, this can be drawn but looks strange. So this crashes because
  // we require that there is at least `outer_corner_radius * 2` between
  // cutouts.
  builder.AddCutout(RoundedRectCutoutPathBuilder::Corner::kUpperLeft,
                    gfx::SizeF(70.0f, 70.0f));
  builder.AddCutout(RoundedRectCutoutPathBuilder::Corner::kUpperRight,
                    gfx::SizeF(30.0f, 70.0f));

  EXPECT_CHECK_DEATH_WITH(
      {
        // Cutouts overlap so this should crash.
        SkPath path = builder.Build();
      },
      "cutouts intersect");
}

}  // namespace
}  // namespace ash
