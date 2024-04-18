// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/rounded_rect_cutout_path_builder.h"

#include <ostream>

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

}  // namespace
}  // namespace ash
