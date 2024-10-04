// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/math_util.h"

#include <stdint.h>

#include <cmath>
#include <limits>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/geometry/transform.h"

namespace cc {
namespace {

TEST(MathUtilTest, ProjectionOfPerpendicularPlane) {
  // In this case, the m33() element of the transform becomes zero, which could
  // cause a divide-by-zero when projecting points/quads.

  gfx::Transform transform;
  transform.MakeIdentity();
  transform.set_rc(2, 2, 0);

  gfx::RectF rect = gfx::RectF(0, 0, 1, 1);
  gfx::RectF projected_rect = MathUtil::ProjectClippedRect(transform, rect);

  EXPECT_EQ(0, projected_rect.x());
  EXPECT_EQ(0, projected_rect.y());
  EXPECT_TRUE(projected_rect.IsEmpty());
}

TEST(MathUtilTest, ProjectionOfAlmostPerpendicularPlane) {
  // In this case, the m33() element of the transform becomes almost zero, which
  // could cause a divide-by-zero when projecting points/quads.

  gfx::Transform transform;
  // The transform is from an actual test page:
  // [ +1.0000 +0.0000 -1.0000 +3144132.0000
  //   +0.0000 +1.0000 +0.0000 +0.0000
  //   +16331238407143424.0000 +0.0000 -0.0000 +51346917453137000267776.0000
  //   +0.0000 +0.0000 +0.0000 +1.0000 ]
  transform.MakeIdentity();
  transform.set_rc(0, 2, -1);
  transform.set_rc(0, 3, 3144132.0);
  transform.set_rc(2, 0, 16331238407143424.0);
  transform.set_rc(2, 2, -1e-33);
  transform.set_rc(2, 3, 51346917453137000267776.0);

  gfx::RectF rect = gfx::RectF(0, 0, 1, 1);
  gfx::RectF projected_rect = MathUtil::ProjectClippedRect(transform, rect);

  EXPECT_EQ(0, projected_rect.x());
  EXPECT_EQ(0, projected_rect.y());
  EXPECT_TRUE(projected_rect.IsEmpty()) << projected_rect.ToString();
}

TEST(MathUtilTest, EnclosingClippedRectHandlesInfinityY) {
  HomogeneousCoordinate h1(100, 10, 0, 1);
  HomogeneousCoordinate h2(10, 10, 0, 1);
  HomogeneousCoordinate h3(-10, -1, 0, -1);
  HomogeneousCoordinate h4(-100, -1, 0, -1);

  // The bounds of the enclosing clipped rect should be 100 to 10 for x
  // and 10 to infinity for y. However, if there is a bug where the result
  // is set so big as to destroy the precision of ymin, we can't deal well
  // with the resulting rect.
  gfx::RectF result = MathUtil::ComputeEnclosingClippedRect(h1, h2, h3, h4);

  EXPECT_FALSE(result.IsEmpty());
  EXPECT_TRUE(result.Contains(50.0f, 50.0f));
  EXPECT_TRUE(result.Contains(10.1f, 10.1f));
  EXPECT_TRUE(result.Contains(50.0f, 50000.0f));
  EXPECT_FALSE(result.Contains(100.1f, 50.0f));
  EXPECT_FALSE(result.Contains(9.9f, 50.0f));
  EXPECT_FALSE(result.Contains(50.0f, 9.9f));
}

TEST(MathUtilTest, EnclosingClippedRectHandlesNegativeInfinityX) {
  HomogeneousCoordinate h1(100, 10, 0, 1);
  HomogeneousCoordinate h2(-110, -10, 0, -1);
  HomogeneousCoordinate h3(-110, -100, 0, -1);
  HomogeneousCoordinate h4(100, 100, 0, 1);

  // The bounds of the enclosing clipped rect should be 100 to -infinity for x
  // and 10 to 100 for y. However, if there is a bug where the result
  // is set so big as to destroy the precision of ymin, we can't deal well
  // with the resulting rect.
  gfx::RectF result = MathUtil::ComputeEnclosingClippedRect(h1, h2, h3, h4);

  EXPECT_FALSE(result.IsEmpty());
  EXPECT_TRUE(result.Contains(50.0f, 50.0f));
  EXPECT_TRUE(result.Contains(10.1f, 10.1f));
  EXPECT_TRUE(result.Contains(0.0f, 99.9f));
  EXPECT_FALSE(result.Contains(100.1f, 50.0f));
  EXPECT_FALSE(result.Contains(50.0f, 100.1f));
  EXPECT_FALSE(result.Contains(50.0f, 9.9f));
}

TEST(MathUtilTest, EnclosingClippedRectHandlesInfinityXY) {
  HomogeneousCoordinate h1(10, 10, 0, 1);
  HomogeneousCoordinate h2(0, 0, 0, -1);
  HomogeneousCoordinate h3(20, -10, 0, 1);
  HomogeneousCoordinate h4(10, -10, 0, 1);

  // The bounds of the enclosing clipped rect should be 10 to infinity for x
  // and -infinity to infinity for y.
  // It would be quite easy for this result to not include anything useful.
  gfx::RectF result = MathUtil::ComputeEnclosingClippedRect(h1, h2, h3, h4);

  // Notes: (A) In the mapped shape, (B) In the enclosing rect, but not the
  // mapped shape, (C) In the mapped shape, but clipped.
  EXPECT_FALSE(result.IsEmpty());
  EXPECT_TRUE(result.Contains(10.0f, 10.0f));      // Note (A)
  EXPECT_TRUE(result.Contains(10.11f, 10.1f));     // Note (A)
  EXPECT_TRUE(result.Contains(10.1f, 10.11f));     // Note (B)
  EXPECT_TRUE(result.Contains(1000.1f, 1000.2f));  // Note (B)
  EXPECT_TRUE(result.Contains(20.0f, -10.0f));     // Note (A)
  EXPECT_TRUE(result.Contains(20.1f, -10.0f));     // Note (A)
  EXPECT_TRUE(result.Contains(20.0f, -10.1f));     // Note (B)
  EXPECT_TRUE(result.Contains(10.0f, -10.0f));     // Note (A)
  EXPECT_TRUE(result.Contains(10.0f, -10.1f));     // Note (B)
  EXPECT_FALSE(result.Contains(0.0f, 0.0f));       // Note (C)
  EXPECT_FALSE(result.Contains(0.0f, -9.9f));      // Note (C)
}

TEST(MathUtilTest, EnclosingClippedRectUsesCorrectInitialBounds) {
  HomogeneousCoordinate h1(-100, -100, 0, 1);
  HomogeneousCoordinate h2(-10, -10, 0, 1);
  HomogeneousCoordinate h3(10, 10, 0, -1);
  HomogeneousCoordinate h4(100, 100, 0, -1);

  // The bounds of the enclosing clipped rect should be -100 to -10 for both x
  // and y. However, if there is a bug where the initial xmin/xmax/ymin/ymax are
  // initialized to numeric_limits<float>::min() (which is zero, not -flt_max)
  // then the enclosing clipped rect will be computed incorrectly.
  gfx::RectF result = MathUtil::ComputeEnclosingClippedRect(h1, h2, h3, h4);

  // Due to floating point math in ComputeClippedPointForEdge this result
  // is fairly imprecise.  0.15f was empirically determined.
  EXPECT_RECTF_NEAR(gfx::RectF(-100, -100, 90, 90), result, 0.15f);
}

TEST(MathUtilTest, EnclosingClippedRectHandlesSmallPositiveW) {
  // When all homogeneous coordinates have w > 0, no clipping against the w = 0
  // plane is performed and the projected points are sent to gfx::QuadF's
  // bounding box function. w can be made arbitrarily close to 0 on the positive
  // side and cause precision problems later on unless it's handled properly.

  // Coordinates inspired by a real test page. One edge maps to approximately
  // negative infinity, and the other is at x~109.
  HomogeneousCoordinate h1(-154.0f, -109.0f, 0.0f, 6e-8f);
  HomogeneousCoordinate h2(152.0f, 44.0f, 0.0f, 1.4f);
  HomogeneousCoordinate h3(152.0f, 261.0f, 0.0f, 1.4f);
  HomogeneousCoordinate h4(-154.0f, 108.0f, 0.0f, 6e-8f);

  // Confirm original behavior is problematic if we just divide by w.
  gfx::QuadF naiveQuad = {{h1.x() / h1.w(), h1.y() / h1.w()},
                          {h2.x() / h2.w(), h2.y() / h2.w()},
                          {h3.x() / h3.w(), h3.y() / h3.w()},
                          {h4.x() / h4.w(), h4.y() / h4.w()}};
  // The calculated min and max coordinates differ by ~2^31, well outside a
  // floats ability to represent onscreen pixel coordinates and in this case,
  // the projected bounds fail to represent that one edge is still on screen.
  gfx::RectF naiveBounds = naiveQuad.BoundingBox();
  EXPECT_TRUE(naiveBounds.right() <= 0.0f);

  // The bounds of the enclosing clipped rect should be neg. infinity to ~109
  // for x, and neg. infinity to pos. infinity for y.
  gfx::RectF goodBounds = MathUtil::ComputeEnclosingClippedRect(h1, h2, h3, h4);
  EXPECT_FALSE(goodBounds.IsEmpty());
  EXPECT_FLOAT_EQ(-HomogeneousCoordinate::kInfiniteCoordinate, goodBounds.y());
  EXPECT_FLOAT_EQ(HomogeneousCoordinate::kInfiniteCoordinate,
                  goodBounds.bottom());
  EXPECT_FLOAT_EQ(-HomogeneousCoordinate::kInfiniteCoordinate, goodBounds.x());
  // 0.01f was empirically determined.
  EXPECT_NEAR(152.0f / 1.4f, goodBounds.right(), 0.01f);
}

TEST(MathUtilTest, EnclosingRectOfVerticesUsesCorrectInitialBounds) {
  std::array<gfx::PointF, 3> vertices;
  vertices[0] = gfx::PointF(-10, -100);
  vertices[1] = gfx::PointF(-100, -10);
  vertices[2] = gfx::PointF(-30, -30);

  // The bounds of the enclosing rect should be -100 to -10 for both x and y.
  // However, if there is a bug where the initial xmin/xmax/ymin/ymax are
  // initialized to numeric_limits<float>::min() (which is zero, not -flt_max)
  // then the enclosing clipped rect will be computed incorrectly.
  gfx::RectF result = MathUtil::ComputeEnclosingRectOfVertices(vertices);

  EXPECT_RECTF_EQ(gfx::RectF(-100, -100, 90, 90), result);
}

TEST(MathUtilTest, SmallestAngleBetweenVectors) {
  gfx::Vector2dF x(1, 0);
  gfx::Vector2dF y(0, 1);
  gfx::Vector2dF test_vector(0.5, 0.5);

  // Orthogonal vectors are at an angle of 90 degress.
  EXPECT_EQ(90, MathUtil::SmallestAngleBetweenVectors(x, y));

  // A vector makes a zero angle with itself.
  EXPECT_EQ(0, MathUtil::SmallestAngleBetweenVectors(x, x));
  EXPECT_EQ(0, MathUtil::SmallestAngleBetweenVectors(y, y));
  EXPECT_EQ(0, MathUtil::SmallestAngleBetweenVectors(test_vector, test_vector));

  // Parallel but reversed vectors are at 180 degrees.
  EXPECT_FLOAT_EQ(180, MathUtil::SmallestAngleBetweenVectors(x, -x));
  EXPECT_FLOAT_EQ(180, MathUtil::SmallestAngleBetweenVectors(y, -y));
  EXPECT_FLOAT_EQ(
      180, MathUtil::SmallestAngleBetweenVectors(test_vector, -test_vector));

  // The test vector is at a known angle.
  EXPECT_FLOAT_EQ(
      45, std::floor(MathUtil::SmallestAngleBetweenVectors(test_vector, x)));
  EXPECT_FLOAT_EQ(
      45, std::floor(MathUtil::SmallestAngleBetweenVectors(test_vector, y)));
}

TEST(MathUtilTest, VectorProjection) {
  gfx::Vector2dF x(1, 0);
  gfx::Vector2dF y(0, 1);
  gfx::Vector2dF test_vector(0.3f, 0.7f);

  // Orthogonal vectors project to a zero vector.
  EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0, 0), MathUtil::ProjectVector(x, y));
  EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0, 0), MathUtil::ProjectVector(y, x));

  // Projecting a vector onto the orthonormal basis gives the corresponding
  // component of the vector.
  EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(test_vector.x(), 0),
                      MathUtil::ProjectVector(test_vector, x));
  EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0, test_vector.y()),
                      MathUtil::ProjectVector(test_vector, y));

  // Finally check than an arbitrary vector projected to another one gives a
  // vector parallel to the second vector.
  gfx::Vector2dF target_vector(0.5, 0.2f);
  gfx::Vector2dF projected_vector =
      MathUtil::ProjectVector(test_vector, target_vector);
  EXPECT_EQ(projected_vector.x() / target_vector.x(),
            projected_vector.y() / target_vector.y());
}

TEST(MathUtilTest, MapEnclosedRectWith2dAxisAlignedTransform) {
  gfx::Rect input(1, 2, 3, 4);
  gfx::Rect output;
  gfx::Transform transform;

  // Identity.
  output =
      MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(transform, input);
  EXPECT_EQ(input, output);

  // Integer translate.
  transform.Translate(2.0, 3.0);
  output =
      MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(transform, input);
  EXPECT_EQ(gfx::Rect(3, 5, 3, 4), output);

  // Non-integer translate.
  transform.Translate(0.5, 0.5);
  output =
      MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(transform, input);
  EXPECT_EQ(gfx::Rect(4, 6, 2, 3), output);

  // Scale.
  transform = gfx::Transform();
  transform.Scale(2.0, 3.0);
  output =
      MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(transform, input);
  EXPECT_EQ(gfx::Rect(2, 6, 6, 12), output);

  // Rotate Z.
  transform = gfx::Transform();
  transform.Translate(1.0, 2.0);
  transform.RotateAboutZAxis(90.0);
  transform.Translate(-1.0, -2.0);
  output =
      MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(transform, input);
  EXPECT_EQ(gfx::Rect(-3, 2, 4, 3), output);

  // Rotate X.
  transform = gfx::Transform();
  transform.RotateAboutXAxis(90.0);
  output =
      MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(transform, input);
  EXPECT_TRUE(output.IsEmpty());

  transform = gfx::Transform();
  transform.RotateAboutXAxis(180.0);
  output =
      MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(transform, input);
  EXPECT_EQ(gfx::Rect(1, -6, 3, 4), output);

  // Rotate Y.
  transform = gfx::Transform();
  transform.RotateAboutYAxis(90.0);
  output =
      MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(transform, input);
  EXPECT_TRUE(output.IsEmpty());

  transform = gfx::Transform();
  transform.RotateAboutYAxis(180.0);
  output =
      MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(transform, input);
  EXPECT_EQ(gfx::Rect(-4, 2, 3, 4), output);

  // Translate Z.
  transform = gfx::Transform();
  transform.ApplyPerspectiveDepth(10.0);
  transform.Translate3d(0.0, 0.0, 5.0);
  output =
      MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(transform, input);
  EXPECT_EQ(gfx::Rect(2, 4, 6, 8), output);
}

TEST(MathUtilTest, MapEnclosingRectWithLargeTransforms) {
  gfx::Rect input(1, 2, 100, 200);
  gfx::Rect output;

  gfx::Transform large_x_scale = gfx::Transform::MakeScale(1e37, 1.0);

  gfx::Transform infinite_x_scale;
  infinite_x_scale = large_x_scale * large_x_scale;

  gfx::Transform large_y_scale = gfx::Transform::MakeScale(1.0, 1e37);

  gfx::Transform infinite_y_scale;
  infinite_y_scale = large_y_scale * large_y_scale;

  gfx::Transform rotation;
  rotation.RotateAboutYAxis(170.0);

  // The following code should not crash due to NaNs. The result rects are
  // empty because either the geometry was saturated or NaNs were set to 0.
  output = MathUtil::MapEnclosingClippedRect(large_x_scale, input);
  EXPECT_TRUE(output.IsEmpty());

  output = MathUtil::MapEnclosingClippedRect(large_x_scale * rotation, input);
  EXPECT_TRUE(output.IsEmpty());

  output = MathUtil::MapEnclosingClippedRect(infinite_x_scale, input);
  EXPECT_TRUE(output.IsEmpty());

  output =
      MathUtil::MapEnclosingClippedRect(infinite_x_scale * rotation, input);
  EXPECT_TRUE(output.IsEmpty());

  output = MathUtil::MapEnclosingClippedRect(large_y_scale, input);
  EXPECT_TRUE(output.IsEmpty());

  output = MathUtil::MapEnclosingClippedRect(large_y_scale * rotation, input);
  EXPECT_TRUE(output.IsEmpty());

  output = MathUtil::MapEnclosingClippedRect(infinite_y_scale, input);
  EXPECT_TRUE(output.IsEmpty());

  output =
      MathUtil::MapEnclosingClippedRect(infinite_y_scale * rotation, input);
  EXPECT_TRUE(output.IsEmpty());
}

TEST(MathUtilTest, MapEnclosingRectIgnoringError) {
  float scale = 2.00001;
  gfx::Rect input(0, 0, 1000, 500);
  gfx::Rect output;

  gfx::Transform transform = gfx::Transform::MakeScale(scale);
  output =
      MathUtil::MapEnclosingClippedRectIgnoringError(transform, input, 0.f);
  EXPECT_EQ(gfx::Rect(0, 0, 2001, 1001), output);

  output =
      MathUtil::MapEnclosingClippedRectIgnoringError(transform, input, 0.002f);
  EXPECT_EQ(gfx::Rect(0, 0, 2001, 1001), output);

  output =
      MathUtil::MapEnclosingClippedRectIgnoringError(transform, input, 0.02f);
  EXPECT_EQ(gfx::Rect(0, 0, 2000, 1000), output);
}

TEST(MathUtilTest, ProjectEnclosingRectWithLargeTransforms) {
  gfx::Rect input(1, 2, 100, 200);
  gfx::Rect output;

  gfx::Transform large_x_scale = gfx::Transform::MakeScale(1e37, 1.0);

  gfx::Transform infinite_x_scale;
  infinite_x_scale = large_x_scale * large_x_scale;

  gfx::Transform large_y_scale = gfx::Transform::MakeScale(1.0, 1e37);

  gfx::Transform infinite_y_scale;
  infinite_y_scale = large_y_scale * large_y_scale;

  gfx::Transform rotation;
  rotation.RotateAboutYAxis(170.0);

  // The following code should not crash due to NaNs. The result rects are
  // empty because either the geometry was saturated or NaNs were set to 0.
  output = MathUtil::ProjectEnclosingClippedRect(large_x_scale, input);
  EXPECT_TRUE(output.IsEmpty());

  output =
      MathUtil::ProjectEnclosingClippedRect(large_x_scale * rotation, input);
  EXPECT_TRUE(output.IsEmpty());

  output = MathUtil::ProjectEnclosingClippedRect(infinite_x_scale, input);
  EXPECT_TRUE(output.IsEmpty());

  output =
      MathUtil::ProjectEnclosingClippedRect(infinite_x_scale * rotation, input);
  EXPECT_TRUE(output.IsEmpty());

  output = MathUtil::ProjectEnclosingClippedRect(large_y_scale, input);
  EXPECT_TRUE(output.IsEmpty());

  output =
      MathUtil::ProjectEnclosingClippedRect(large_y_scale * rotation, input);
  EXPECT_TRUE(output.IsEmpty());

  output = MathUtil::ProjectEnclosingClippedRect(infinite_y_scale, input);
  EXPECT_TRUE(output.IsEmpty());

  output =
      MathUtil::ProjectEnclosingClippedRect(infinite_y_scale * rotation, input);
  EXPECT_TRUE(output.IsEmpty());
}

TEST(MathUtilTest, RoundUp) {
  static_assert(MathUtil::UncheckedRoundUp(45, 10) == 50);
  for (int multiplier = 1; multiplier <= 10; ++multiplier) {
    // Try attempts in descending order, so that we can
    // determine the correct value before it's needed.
    int correct;
    for (int attempt = 5 * multiplier; attempt >= -5 * multiplier; --attempt) {
      if ((attempt % multiplier) == 0)
        correct = attempt;
      EXPECT_EQ(correct, MathUtil::UncheckedRoundUp(attempt, multiplier))
          << "attempt=" << attempt << " multiplier=" << multiplier;
    }
  }

  for (unsigned multiplier = 1; multiplier <= 10; ++multiplier) {
    // Try attempts in descending order, so that we can
    // determine the correct value before it's needed.
    unsigned correct;
    for (unsigned attempt = 5 * multiplier; attempt > 0; --attempt) {
      if ((attempt % multiplier) == 0)
        correct = attempt;
      EXPECT_EQ(correct, MathUtil::UncheckedRoundUp(attempt, multiplier))
          << "attempt=" << attempt << " multiplier=" << multiplier;
    }
    EXPECT_EQ(0u, MathUtil::UncheckedRoundUp(0u, multiplier))
        << "attempt=0 multiplier=" << multiplier;
  }
}

TEST(MathUtilTest, RoundUpAlmostOverflow) {
  // This is the largest multiple of 64 before rounding up overflows.
  constexpr int value = 2147483584;
  constexpr int multiple = 64;

  static_assert(MathUtil::VerifyRoundup<int>(value, multiple));
  static_assert(!MathUtil::VerifyRoundup<int>(value + 1, multiple));

  EXPECT_EQ(MathUtil::UncheckedRoundUp<int>(value, multiple), value);
}

TEST(MathUtilTest, RoundUpOverflow) {
  // Rounding up 123 by 50 is 150, which overflows int8_t, but fits in uint8_t.
  EXPECT_FALSE(MathUtil::VerifyRoundup<int8_t>(123, 50));
  EXPECT_TRUE(MathUtil::VerifyRoundup<uint8_t>(123, 50));
}

TEST(MathUtilTest, RoundDown) {
  static_assert(MathUtil::UncheckedRoundDown(45, 10) == 40);
  for (int multiplier = 1; multiplier <= 10; ++multiplier) {
    // Try attempts in ascending order, so that we can
    // determine the correct value before it's needed.
    int correct;
    for (int attempt = -5 * multiplier; attempt <= 5 * multiplier; ++attempt) {
      if ((attempt % multiplier) == 0)
        correct = attempt;
      EXPECT_EQ(correct, MathUtil::UncheckedRoundDown(attempt, multiplier))
          << "attempt=" << attempt << " multiplier=" << multiplier;
    }
  }

  for (unsigned multiplier = 1; multiplier <= 10; ++multiplier) {
    // Try attempts in ascending order, so that we can
    // determine the correct value before it's needed.
    unsigned correct;
    for (unsigned attempt = 0; attempt <= 5 * multiplier; ++attempt) {
      if ((attempt % multiplier) == 0)
        correct = attempt;
      EXPECT_EQ(correct, MathUtil::UncheckedRoundDown(attempt, multiplier))
          << "attempt=" << attempt << " multiplier=" << multiplier;
    }
  }
}

TEST(MathUtilTest, RoundDownAlmostOverflow) {
  // This is the smallest multiple of 10 before rounding down overflows.
  constexpr int value = -2147483640;
  constexpr int multiple = 10;

  static_assert(MathUtil::VerifyRoundDown(value, multiple));
  static_assert(!MathUtil::VerifyRoundDown(value - 1, multiple));

  EXPECT_EQ(MathUtil::UncheckedRoundDown<int>(value, multiple), value);
}

TEST(MathUtilTest, RoundDownUnderflow) {
  // Rounding down -123 by 50 is -150, which underflows int8_t, but fits in
  // int16_t.
  EXPECT_FALSE(MathUtil::VerifyRoundDown<int8_t>(-123, 50));
  EXPECT_TRUE(MathUtil::VerifyRoundDown<int16_t>(-123, 50));
}

#define EXPECT_SIMILAR_VALUE(x, y) \
  EXPECT_TRUE(MathUtil::IsFloatNearlyTheSame(x, y))
#define EXPECT_DISSIMILAR_VALUE(x, y) \
  EXPECT_FALSE(MathUtil::IsFloatNearlyTheSame(x, y))

// Arbitrary point that shouldn't be different from zero.
static const float zeroish = 1.0e-11f;

TEST(MathUtilTest, Approximate) {
  // Same should be similar.
  EXPECT_SIMILAR_VALUE(1.0f, 1.0f);

  // Zero should not cause similarity issues.
  EXPECT_SIMILAR_VALUE(0.0f, 0.0f);

  // Chosen sensitivity makes hardware sense, whether small or large.
  EXPECT_SIMILAR_VALUE(0.0f, std::nextafter(0.0f, 1.0f));
  EXPECT_SIMILAR_VALUE(1000000.0f, std::nextafter(1000000.0f, 0.0f));

  // Make sure that neither the side you approach, nor the order of
  // parameters matter at the borderline case.
  EXPECT_SIMILAR_VALUE(std::nextafter(0.0f, 1.0f), 0.0f);
  EXPECT_SIMILAR_VALUE(std::nextafter(1000000.0f, 0.0f), 1000000.0f);
  EXPECT_SIMILAR_VALUE(0.0f, std::nextafter(0.0f, -1.0f));
  EXPECT_SIMILAR_VALUE(1000000.0f, std::nextafter(1000000.0f, 1e9f));
  EXPECT_SIMILAR_VALUE(std::nextafter(0.0f, -1.0f), 0.0f);
  EXPECT_SIMILAR_VALUE(std::nextafter(1000000.0f, 1e9f), 1000000.0f);

  // Double check our arbitrary constant.  Mostly this is for the
  // following Point tests.
  EXPECT_SIMILAR_VALUE(0.0f, zeroish);

  // Arbitrary point that is different from one for Approximate tests.
  EXPECT_SIMILAR_VALUE(1.0f, 1.000001f);

  // Arbitrary (large) difference close to 1.
  EXPECT_SIMILAR_VALUE(10000000.0f, 10000001.0f);

  // Make sure one side being zero doesn't hide real differences.
  EXPECT_DISSIMILAR_VALUE(0.0f, 1.0f);
  EXPECT_DISSIMILAR_VALUE(1.0f, 0.0f);

  // Make sure visible differences don't disappear.
  EXPECT_DISSIMILAR_VALUE(1.0f, 2.0f);
  EXPECT_DISSIMILAR_VALUE(10000.0f, 10001.0f);
}

#define EXPECT_SIMILAR_POINT_F(x, y) \
  EXPECT_TRUE(MathUtil::IsNearlyTheSameForTesting(gfx::PointF x, gfx::PointF y))
#define EXPECT_DISSIMILAR_POINT_F(x, y) \
  EXPECT_FALSE(                         \
      MathUtil::IsNearlyTheSameForTesting(gfx::PointF x, gfx::PointF y))

TEST(MathUtilTest, ApproximatePointF) {
  // Same is similar.
  EXPECT_SIMILAR_POINT_F((0.0f, 0.0f), (0.0f, 0.0f));

  // Not over sensitive on each axis.
  EXPECT_SIMILAR_POINT_F((zeroish, 0.0f), (0.0f, 0.0f));
  EXPECT_SIMILAR_POINT_F((0.0f, zeroish), (0.0f, 0.0f));
  EXPECT_SIMILAR_POINT_F((0.0f, 0.0f), (zeroish, 0.0f));
  EXPECT_SIMILAR_POINT_F((0.0f, 0.0f), (0.0f, zeroish));

  // Still sensitive to any axis.
  EXPECT_DISSIMILAR_POINT_F((1.0f, 0.0f), (0.0f, 0.0f));
  EXPECT_DISSIMILAR_POINT_F((0.0f, 1.0f), (0.0f, 0.0f));
  EXPECT_DISSIMILAR_POINT_F((0.0f, 0.0f), (1.0f, 0.0f));
  EXPECT_DISSIMILAR_POINT_F((0.0f, 0.0f), (0.0f, 1.0f));

  // Not crossed over, sensitive on each side of each axis.
  EXPECT_SIMILAR_POINT_F((0.0f, 1.0f), (0.0f, 1.0f));
  EXPECT_SIMILAR_POINT_F((1.0f, 2.0f), (1.0f, 2.0f));
  EXPECT_DISSIMILAR_POINT_F((3.0f, 2.0f), (1.0f, 2.0f));
  EXPECT_DISSIMILAR_POINT_F((1.0f, 3.0f), (1.0f, 1.0f));
  EXPECT_DISSIMILAR_POINT_F((1.0f, 2.0f), (3.0f, 2.0f));
  EXPECT_DISSIMILAR_POINT_F((1.0f, 2.0f), (1.0f, 3.0f));
}

#define EXPECT_SIMILAR_POINT_3F(x, y) \
  EXPECT_TRUE(                        \
      MathUtil::IsNearlyTheSameForTesting(gfx::Point3F x, gfx::Point3F y))
#define EXPECT_DISSIMILAR_POINT_3F(x, y) \
  EXPECT_FALSE(                          \
      MathUtil::IsNearlyTheSameForTesting(gfx::Point3F x, gfx::Point3F y))

TEST(MathUtilTest, ApproximatePoint3F) {
  // Same same.
  EXPECT_SIMILAR_POINT_3F((0.0f, 0.0f, 0.0f), (0.0f, 0.0f, 0.0f));
  EXPECT_SIMILAR_POINT_3F((zeroish, 0.0f, 0.0f), (0.0f, 0.0f, 0.0f));
  EXPECT_SIMILAR_POINT_3F((0.0f, zeroish, 0.0f), (0.0f, 0.0f, 0.0f));
  EXPECT_SIMILAR_POINT_3F((0.0f, 0.0f, zeroish), (0.0f, 0.0f, 0.0f));
  EXPECT_SIMILAR_POINT_3F((0.0f, 0.0f, 0.0f), (zeroish, 0.0f, 0.0f));
  EXPECT_SIMILAR_POINT_3F((0.0f, 0.0f, 0.0f), (0.0f, zeroish, 0.0f));
  EXPECT_SIMILAR_POINT_3F((0.0f, 0.0f, 0.0f), (0.0f, 0.0f, zeroish));

  // Not crossed over, sensitive on each side of each axis.
  EXPECT_SIMILAR_POINT_3F((1.0f, 2.0f, 3.0f), (1.0f, 2.0f, 3.0f));
  EXPECT_DISSIMILAR_POINT_3F((4.0f, 2.0f, 3.0f), (1.0f, 2.0f, 3.0f));
  EXPECT_DISSIMILAR_POINT_3F((1.0f, 4.0f, 3.0f), (1.0f, 1.0f, 3.0f));
  EXPECT_DISSIMILAR_POINT_3F((1.0f, 2.0f, 4.0f), (1.0f, 2.0f, 1.0f));
  EXPECT_DISSIMILAR_POINT_3F((1.0f, 2.0f, 3.0f), (4.0f, 2.0f, 3.0f));
  EXPECT_DISSIMILAR_POINT_3F((1.0f, 2.0f, 3.0f), (1.0f, 4.0f, 3.0f));
  EXPECT_DISSIMILAR_POINT_3F((1.0f, 2.0f, 3.0f), (1.0f, 2.0f, 4.0f));
}

// This takes a quad for which two points, (at x = -99) are behind and below
// the eyepoint and checks to make sure we build a quad that doesn't include
// anything from w<0 space.  We used to build a degenerate quad.
TEST(MathUtilTest, MapClippedQuadDuplicateTriangle) {
  gfx::Transform transform;
  transform.MakeIdentity();
  transform.ApplyPerspectiveDepth(50.0);
  transform.RotateAboutYAxis(89.0);
  // We are almost looking along the X-Y plane from (-50, almost 0)

  gfx::QuadF src_quad(gfx::PointF(0.0f, -50.0f), gfx::PointF(0.0f, -100.0f),
                      gfx::PointF(-99.0f, -300.0f),
                      gfx::PointF(-99.0f, -100.0f));

  std::array<gfx::Point3F, 6> clipped_quad;
  int num_vertices_in_clipped_quad;

  MathUtil::MapClippedQuad3d(transform, src_quad, clipped_quad,
                             &num_vertices_in_clipped_quad);

  // If we include anything from w<0 space, it will produce positive y
  // coordinates rather than negative ones.
  for (int i = 0; i < num_vertices_in_clipped_quad; ++i) {
    EXPECT_LE(clipped_quad[i].y(), 0);
  }

  EXPECT_EQ(num_vertices_in_clipped_quad, 4);
}

// This takes a quad for which two points are identical and checks to make
// sure we build a triangle.
TEST(MathUtilTest, MapClippedQuadDuplicatePoints) {
  gfx::Transform transform;
  transform.MakeIdentity();
  transform.RotateAboutYAxis(45.0);

  gfx::QuadF src_quad(gfx::PointF(-99.0f, -50.0f), gfx::PointF(-99.0f, -50.0f),
                      gfx::PointF(0.0f, 100.0f), gfx::PointF(0.0f, -100.0f));

  std::array<gfx::Point3F, 6> clipped_quad;
  int num_vertices_in_clipped_quad;

  MathUtil::MapClippedQuad3d(transform, src_quad, clipped_quad,
                             &num_vertices_in_clipped_quad);

  EXPECT_EQ(num_vertices_in_clipped_quad, 3);
}

// This takes a quad for which two points are identical and checks to make
// sure we build a triangle.  The quirk here is that the two shared points are
// first and last, not sequential.
TEST(MathUtilTest, MapClippedQuadDuplicatePointsWrapped) {
  gfx::Transform transform;
  transform.MakeIdentity();
  transform.RotateAboutYAxis(45.0);

  gfx::QuadF src_quad(gfx::PointF(-99.0f, -50.0f), gfx::PointF(0.0f, 100.0f),
                      gfx::PointF(0.0f, -100.0f), gfx::PointF(-99.0f, -50.0f));

  std::array<gfx::Point3F, 6> clipped_quad;
  int num_vertices_in_clipped_quad;

  MathUtil::MapClippedQuad3d(transform, src_quad, clipped_quad,
                             &num_vertices_in_clipped_quad);

  EXPECT_EQ(num_vertices_in_clipped_quad, 3);
}

// Here we map and clip a quad with only one point that disappears to infinity
// behind us.  We don't want two vertices at infinity crossing in and out
// of w < 0 space.
TEST(MathUtilTest, MapClippedQuadDuplicateQuad) {
  gfx::Transform transform;
  transform.MakeIdentity();
  transform.ApplyPerspectiveDepth(50.0);
  transform.RotateAboutYAxis(89.0);

  gfx::QuadF src_quad(gfx::PointF(0.0f, -50.0f), gfx::PointF(400.0f, -50.0f),
                      gfx::PointF(0.0f, -100.0f), gfx::PointF(-99.0f, -300.0f));

  std::array<gfx::Point3F, 6> clipped_quad;
  int num_vertices_in_clipped_quad;

  MathUtil::MapClippedQuad3d(transform, src_quad, clipped_quad,
                             &num_vertices_in_clipped_quad);

  // If we include anything from w<0 space, it will produce positive y
  // coordinates rather than negative ones.
  for (int i = 0; i < num_vertices_in_clipped_quad; ++i) {
    EXPECT_LE(clipped_quad[i].y(), 0);
  }

  EXPECT_EQ(num_vertices_in_clipped_quad, 5);
}

#define EXPECT_LT_LT(a, b, c)  \
  do {                         \
    auto b_evaluated = b;      \
    EXPECT_LT(a, b_evaluated); \
    EXPECT_LT(b_evaluated, c); \
  } while (0)

#define EXPECT_LE_LT(a, b, c)  \
  do {                         \
    auto b_evaluated = b;      \
    EXPECT_LE(a, b_evaluated); \
    EXPECT_LT(b_evaluated, c); \
  } while (0)

#define EXPECT_LT_LE(a, b, c)  \
  do {                         \
    auto b_evaluated = b;      \
    EXPECT_LT(a, b_evaluated); \
    EXPECT_LE(b_evaluated, c); \
  } while (0)

#define EXPECT_LE_LE(a, b, c)  \
  do {                         \
    auto b_evaluated = b;      \
    EXPECT_LE(a, b_evaluated); \
    EXPECT_LE(b_evaluated, c); \
  } while (0)

// Here we map and clip a quad with a point that disappears to infinity behind
// us while staying finite in one dimension (i.e., x goes to 0 as w goes to 0,
// and x' is constant along the edge).
TEST(MathUtilTest, MapClippedQuadInfiniteInSomeDimensions) {
  gfx::Transform transform;
  transform.MakeIdentity();
  transform.ApplyPerspectiveDepth(50.0);
  transform.RotateAboutXAxis(89.0);

  gfx::QuadF src_quad(gfx::PointF(0.0f, 0.0f), gfx::PointF(0.0f, 100.0f),
                      gfx::PointF(100.0f, 100.0f), gfx::PointF(100.0f, 0.0f));

  std::array<gfx::Point3F, 6> clipped_quad;
  int num_vertices_in_clipped_quad;

  MathUtil::MapClippedQuad3d(transform, src_quad, clipped_quad,
                             &num_vertices_in_clipped_quad);

  EXPECT_EQ(num_vertices_in_clipped_quad, 4);

  EXPECT_EQ(clipped_quad[0].x(), 0.0f);
  EXPECT_EQ(clipped_quad[0].y(), 0.0f);
  EXPECT_EQ(clipped_quad[0].z(), 0.0f);

  EXPECT_EQ(clipped_quad[1].x(), 0.0f);
  EXPECT_LT_LT(17000.0f, clipped_quad[1].y(), 18000.0f);
  EXPECT_LT_LE(998000.0f, clipped_quad[1].z(), 1000000.0f);

  EXPECT_LT_LE(998000.0f, clipped_quad[2].x(), 1000000.0f);
  EXPECT_LT_LT(8500.0f, clipped_quad[2].y(), 9000.0f);
  EXPECT_LT_LE(499000.0f, clipped_quad[2].z(), 500000.0f);

  EXPECT_EQ(clipped_quad[3].x(), 100.0f);
  EXPECT_EQ(clipped_quad[3].y(), 0.0f);
  EXPECT_EQ(clipped_quad[3].z(), 0.0f);
}

// Here we map and clip a quad with a point that disappears to infinity behind
// us while staying finite in one dimension (i.e., x goes to 0 as w goes to 0,
// and x' is constant along the edge).  This differs from the previous test
// in that the edge with constant x' is at 100 rather than 0.
TEST(MathUtilTest, MapClippedQuadInfiniteInSomeDimensionsNonZero) {
  gfx::Transform transform;
  transform.MakeIdentity();
  transform.Translate(100.0, 0.0);
  transform.ApplyPerspectiveDepth(50.0);
  transform.RotateAboutXAxis(89.0);
  transform.Translate(-100.0, 0.0);

  gfx::QuadF src_quad(gfx::PointF(0.0f, 0.0f), gfx::PointF(0.0f, 100.0f),
                      gfx::PointF(100.0f, 100.0f), gfx::PointF(100.0f, 0.0f));

  std::array<gfx::Point3F, 6> clipped_quad;
  int num_vertices_in_clipped_quad;

  MathUtil::MapClippedQuad3d(transform, src_quad, clipped_quad,
                             &num_vertices_in_clipped_quad);

  EXPECT_EQ(num_vertices_in_clipped_quad, 4);

  EXPECT_EQ(clipped_quad[0].x(), 0.0f);
  EXPECT_EQ(clipped_quad[0].y(), 0.0f);
  EXPECT_EQ(clipped_quad[0].z(), 0.0f);

  EXPECT_LE_LT(-1000000.0f, clipped_quad[1].x(), -998000.0f);
  EXPECT_LT_LT(8500.0f, clipped_quad[1].y(), 9000.0f);
  EXPECT_LT_LE(499000.0f, clipped_quad[1].z(), 500000.0f);

  EXPECT_EQ(clipped_quad[2].x(), 100.0f);
  EXPECT_LT_LT(17000.0f, clipped_quad[2].y(), 18000.0f);
  EXPECT_LT_LE(996000.0f, clipped_quad[2].z(), 1000000.0f);

  EXPECT_EQ(clipped_quad[3].x(), 100.0f);
  EXPECT_EQ(clipped_quad[3].y(), 0.0f);
  EXPECT_EQ(clipped_quad[3].z(), 0.0f);
}

// Test that planes that are parallel to the z axis (other than those going
// through the origin!) just fall through to clipping by points.
TEST(MathUtilTest, MapClippedQuadClampInvisiblePlane) {
  gfx::Transform transform;

  gfx::QuadF src_quad(gfx::PointF(0.0f, 0.0f), gfx::PointF(0.0f, 1000.0f),
                      gfx::PointF(1000.0f, 1000.0f),
                      gfx::PointF(1000.0f, 0.0f));

  std::array<gfx::Point3F, 6> clipped_quad;
  int num_vertices_in_clipped_quad;

  transform.MakeIdentity();
  transform.Translate(100.0, 0.0);
  transform.RotateAboutYAxis(90.0);
  transform.Scale(10000.0f, 10000.0);

  MathUtil::MapClippedQuad3d(transform, src_quad, clipped_quad,
                             &num_vertices_in_clipped_quad);

  EXPECT_EQ(num_vertices_in_clipped_quad, 4);

  EXPECT_EQ(clipped_quad[0].x(), 100.0f);
  EXPECT_EQ(clipped_quad[0].y(), 0.0f);
  EXPECT_EQ(clipped_quad[0].z(), 0.0f);

  EXPECT_EQ(clipped_quad[1].x(), 100.0f);
  EXPECT_EQ(clipped_quad[1].y(), 1000000.0f);
  EXPECT_EQ(clipped_quad[1].z(), 0.0f);

  EXPECT_EQ(clipped_quad[2].x(), 100.0f);
  EXPECT_EQ(clipped_quad[2].y(), 1000000.0f);
  EXPECT_EQ(clipped_quad[2].z(), -1000000.0f);

  EXPECT_EQ(clipped_quad[3].x(), 100.0f);
  EXPECT_EQ(clipped_quad[3].y(), 0.0f);
  EXPECT_EQ(clipped_quad[3].z(), -1000000.0f);

  transform.MakeIdentity();
  transform.Translate(0.0, -50.0);
  transform.RotateAboutXAxis(-90.0);
  transform.Scale(10000.0f, 10000.0);

  MathUtil::MapClippedQuad3d(transform, src_quad, clipped_quad,
                             &num_vertices_in_clipped_quad);

  EXPECT_EQ(num_vertices_in_clipped_quad, 4);

  EXPECT_EQ(clipped_quad[0].x(), 0.0f);
  EXPECT_EQ(clipped_quad[0].y(), -50.0f);
  EXPECT_EQ(clipped_quad[0].z(), 0.0f);

  EXPECT_EQ(clipped_quad[1].x(), 0.0f);
  EXPECT_EQ(clipped_quad[1].y(), -50.0f);
  EXPECT_EQ(clipped_quad[1].z(), -1000000.0f);

  EXPECT_EQ(clipped_quad[2].x(), 1000000.0f);
  EXPECT_EQ(clipped_quad[2].y(), -50.0f);
  EXPECT_EQ(clipped_quad[2].z(), -1000000.0f);

  EXPECT_EQ(clipped_quad[3].x(), 1000000.0f);
  EXPECT_EQ(clipped_quad[3].y(), -50.0f);
  EXPECT_EQ(clipped_quad[3].z(), 0.0f);

  transform.MakeIdentity();
  transform.Translate(10.0, 10.0);
  transform.Rotate(30.0);
  transform.RotateAboutXAxis(90.0);
  transform.Scale(10000.0, 10000.0);

  MathUtil::MapClippedQuad3d(transform, src_quad, clipped_quad,
                             &num_vertices_in_clipped_quad);

  EXPECT_EQ(num_vertices_in_clipped_quad, 4);

  EXPECT_EQ(clipped_quad[0].x(), 10.0f);
  EXPECT_EQ(clipped_quad[0].y(), 10.0f);
  EXPECT_EQ(clipped_quad[0].z(), 0.0f);

  EXPECT_EQ(clipped_quad[1].x(), 10.0f);
  EXPECT_EQ(clipped_quad[1].y(), 10.0f);
  EXPECT_EQ(clipped_quad[1].z(), 1000000.0f);

  EXPECT_EQ(clipped_quad[2].x(), 1000000.0f);
  EXPECT_EQ(clipped_quad[2].y(), 1000000.0f);
  EXPECT_EQ(clipped_quad[2].z(), 1000000.0f);

  EXPECT_EQ(clipped_quad[3].x(), 1000000.0f);
  EXPECT_EQ(clipped_quad[3].y(), 1000000.0f);
  EXPECT_EQ(clipped_quad[3].z(), 0.0f);
}

// Test that when the plane passes too far from the origin, we bring it closer
// before clamping coordinates.
TEST(MathUtilTest, MapClippedQuadClampWholePlane) {
  gfx::Transform transform;
  transform.MakeIdentity();
  transform.Scale3d(1000.0, 1000.0, 1000.0);
  transform.Translate3d(0.0, 0.0, 10000.0);
  transform.RotateAboutXAxis(-45.0);

  gfx::QuadF src_quad(gfx::PointF(0.0f, 0.0f), gfx::PointF(0.0f, 10000.0f),
                      gfx::PointF(100.0f, 10000.0f),
                      gfx::PointF(100.0f, -10000.0f));

  std::array<gfx::Point3F, 6> clipped_quad;
  int num_vertices_in_clipped_quad;

  MathUtil::MapClippedQuad3d(transform, src_quad, clipped_quad,
                             &num_vertices_in_clipped_quad);

  EXPECT_EQ(num_vertices_in_clipped_quad, 4);

  EXPECT_EQ(clipped_quad[0].x(), 0.0f);
  EXPECT_EQ(clipped_quad[0].y(), 0.0f);
  EXPECT_LE_LE(750000.0f, clipped_quad[0].z(), 750001.0f);

  EXPECT_EQ(clipped_quad[1].x(), 0.0f);
  EXPECT_LE_LE(999999.0f, clipped_quad[1].y(), 1000000.0f);
  EXPECT_LE_LE(-250001.0f, clipped_quad[1].z(), -249999.0f);

  EXPECT_LE_LE(14100.0f, clipped_quad[2].x(), 14200.0f);
  EXPECT_LE_LE(999999.0f, clipped_quad[2].y(), 1000000.0f);
  EXPECT_LE_LE(-250001.0f, clipped_quad[2].z(), -249999.0f);

  EXPECT_LE_LE(3500.0f, clipped_quad[3].x(), 3600.0f);
  EXPECT_LE_LE(-250001.0f, clipped_quad[3].y(), -249999.0f);
  EXPECT_EQ(clipped_quad[3].z(), 1000000.0f);
}

// Like the previous test, but with a plane with large negative z.
TEST(MathUtilTest, MapClippedQuadClampWholePlaneBelow) {
  gfx::Transform transform;
  transform.MakeIdentity();
  transform.Scale3d(1000.0, 1000.0, 1000.0);
  transform.Translate3d(0.0, 0.0, -5000.0);
  transform.RotateAboutYAxis(30.0);

  gfx::QuadF src_quad(gfx::PointF(0.0f, 0.0f), gfx::PointF(-10000.0f, 100.0f),
                      gfx::PointF(10000.0f, 100.0f),
                      gfx::PointF(10000.0f, 0.0f));

  std::array<gfx::Point3F, 6> clipped_quad;
  int num_vertices_in_clipped_quad;

  MathUtil::MapClippedQuad3d(transform, src_quad, clipped_quad,
                             &num_vertices_in_clipped_quad);

  EXPECT_EQ(num_vertices_in_clipped_quad, 4);

  EXPECT_EQ(clipped_quad[0].x(), 0.0f);
  EXPECT_EQ(clipped_quad[0].y(), 0.0f);
  EXPECT_LE_LE(-750001.0f, clipped_quad[0].z(), -750000.0f);

  EXPECT_EQ(clipped_quad[1].x(), -1000000.0f);
  EXPECT_LE_LE(11540.0f, clipped_quad[1].y(), 11550.0f);
  EXPECT_LE_LE(-172660.0f, clipped_quad[1].z(), -172640.0f);

  EXPECT_LE_LE(433000.0f, clipped_quad[2].x(), 433025.0f);
  EXPECT_LT_LT(4999.9f, clipped_quad[2].y(), 5000.1f);
  EXPECT_EQ(clipped_quad[2].z(), -1000000.0f);

  EXPECT_LE_LE(433000.0f, clipped_quad[3].x(), 433025.0f);
  EXPECT_EQ(clipped_quad[3].y(), 0.0f);
  EXPECT_EQ(clipped_quad[3].z(), -1000000.0f);
}

TEST(MathUtilTest, MapClippedQuadInfiniteMatrix) {
  // clang-format off
  auto transform = gfx::Transform::RowMajor(
      1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, -100.0f, 0.0f, std::numeric_limits<float>::infinity(),
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f);
  // clang-format on

  gfx::QuadF src_quad(gfx::PointF(0.0f, 1.0f), gfx::PointF(1.0f, 1.0f),
                      gfx::PointF(1.0f, 2.0f), gfx::PointF(0.0f, 2.0f));

  std::array<gfx::Point3F, 6> clipped_quad;
  int num_vertices_in_clipped_quad;

  MathUtil::MapClippedQuad3d(transform, src_quad, clipped_quad,
                             &num_vertices_in_clipped_quad);

  // Nothing to test other than we don't fail DCHECK()s.
}

}  // namespace
}  // namespace cc
