// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/pointer_crop_geometry.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ttc {
namespace {

constexpr gfx::Size kViewport(1200, 800);

TEST(PointerCropGeometryTest, NoPointsOrNoViewportGivesNothing) {
  EXPECT_TRUE(ComputePointerCropRect({}, kViewport).IsEmpty());
  EXPECT_TRUE(ComputePointerCropRect({{600, 400}}, gfx::Size()).IsEmpty());
}

// A single spot in the middle of the page gets the 150 DIP margin on every
// side, widened to the 500 DIP minimum width, centered on it.
TEST(PointerCropGeometryTest, SinglePointGetsMarginsCenteredOnIt) {
  EXPECT_EQ(gfx::Rect(350, 250, 500, 300),
            ComputePointerCropRect({{600, 400}}, kViewport));
}

// Spots near the edges keep the crop's size; the crop slides inward instead.
TEST(PointerCropGeometryTest, CropIsShiftedToStayInsideTheViewport) {
  EXPECT_EQ(gfx::Rect(0, 0, 500, 300),
            ComputePointerCropRect({{10, 10}}, kViewport));
  EXPECT_EQ(gfx::Rect(700, 500, 500, 300),
            ComputePointerCropRect({{1190, 790}}, kViewport));
}

// Several spots get their bounding box plus the margin on every side,
// centered on the box.
TEST(PointerCropGeometryTest, SeveralPointsAreEnclosedWithMargins) {
  // Bounds are (300, 200)-(700, 300): 400x100 plus 150 on each side.
  EXPECT_EQ(
      gfx::Rect(150, 50, 700, 400),
      ComputePointerCropRect({{300, 200}, {700, 300}, {500, 250}}, kViewport));
  // Bounds are (400, 200)-(500, 400): 100x200. With margins that is 400 wide,
  // so it is widened to the minimum.
  EXPECT_EQ(
      gfx::Rect(200, 50, 500, 500),
      ComputePointerCropRect({{400, 200}, {410, 210}, {500, 400}}, kViewport));
}

// A viewport smaller than the crop would be, in either direction or both, is
// used whole in that direction.
TEST(PointerCropGeometryTest, SmallViewportIsUsedWhole) {
  EXPECT_EQ(gfx::Rect(0, 0, 320, 200),
            ComputePointerCropRect({{100, 100}}, gfx::Size(320, 200)));
  EXPECT_EQ(gfx::Rect(0, 250, 400, 300),
            ComputePointerCropRect({{200, 400}}, gfx::Size(400, 800)));
  EXPECT_EQ(gfx::Rect(350, 0, 500, 200),
            ComputePointerCropRect({{600, 100}}, gfx::Size(1200, 200)));
}

// A spot outside the viewport (e.g. a stale sample from before a resize) is
// treated as on the nearest edge rather than producing a crop off the page.
TEST(PointerCropGeometryTest, PointOutsideViewportIsClampedToIt) {
  EXPECT_EQ(gfx::Rect(0, 500, 500, 300),
            ComputePointerCropRect({{-50, 900}}, kViewport));
  // Also when another spot is inside: the bounds are (0, 400)-(300, 400).
  EXPECT_EQ(gfx::Rect(0, 250, 600, 300),
            ComputePointerCropRect({{-400, 400}, {300, 400}}, kViewport));
}

// Fractional positions produce a whole-pixel rectangle that still encloses
// the intended area.
TEST(PointerCropGeometryTest, FractionalPointsGiveEnclosingWholePixels) {
  const gfx::Rect crop = ComputePointerCropRect({{600.5f, 400.25f}}, kViewport);
  EXPECT_EQ(gfx::Rect(350, 250, 501, 301), crop);
  EXPECT_TRUE(gfx::Rect(kViewport).Contains(crop));
}

}  // namespace
}  // namespace ttc
