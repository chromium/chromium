// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/verification_shapes.h"

#include "base/hash/sha1.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_start {

// See internal go/oobe-verification-shapes for details.
TEST(Verification, ShapesCompatibility) {
  ShapeList shapes = GenerateShapes("ABC123");
  ASSERT_EQ(shapes.size(), 4u);

  EXPECT_EQ(shapes[0].shape, Shape::kDiamond);
  EXPECT_EQ(shapes[0].color, Color::kGreen);
  EXPECT_EQ(shapes[0].digit, 0);

  EXPECT_EQ(shapes[1].shape, Shape::kTriangle);
  EXPECT_EQ(shapes[1].color, Color::kBlue);
  EXPECT_EQ(shapes[1].digit, 2);

  EXPECT_EQ(shapes[2].shape, Shape::kSquare);
  EXPECT_EQ(shapes[2].color, Color::kRed);
  EXPECT_EQ(shapes[2].digit, 3);

  EXPECT_EQ(shapes[3].shape, Shape::kTriangle);
  EXPECT_EQ(shapes[3].color, Color::kBlue);
  EXPECT_EQ(shapes[3].digit, 0);
}

}  // namespace quick_start
}  // namespace ash
