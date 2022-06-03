// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/laser/laser_segment_utils.h"
#include "ash/test/ash_test_base.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ash {
namespace {
// |kEpsilon| is used to check that AngleOfPointInNewCoordinates produces to
// expected angles. This function is only used after points are projected in
// opposite directions along the normal, so they should never be to close to
// each other, checking for equality up to 5 significant figures is more than
// enough.
const float kEpsilon = 0.0001f;
}  // namespace

// Helper function to check if a given |point| has the |expected_angle_degree|
// in the coordinate system formed by |origin| and |direction|.
void CheckAngleOfPointInNewCoordinates(const gfx::PointF& origin,
                                       const gfx::Vector2dF& direction,
                                       const gfx::PointF& point,
                                       float expected_angle_degree) {
  float result = AngleOfPointInNewCoordinates(origin, direction, point);
  EXPECT_NEAR(gfx::DegToRad(expected_angle_degree), result, kEpsilon);
}

// Helper function to check if the computed variables match the expected ones.
void CheckNormalLineVariables(const gfx::PointF& start_point,
                              const gfx::PointF& end_point,
                              float expected_slope,
                              float expected_start_intercept,
                              float expected_end_intercept) {
  float calculated_slope;
  float calculated_start_y_intercept;
  float calculated_end_y_intercept;

  ComputeNormalLineVariables(start_point, end_point, &calculated_slope,
                             &calculated_start_y_intercept,
                             &calculated_end_y_intercept);
  EXPECT_NEAR(expected_slope, calculated_slope, kEpsilon);
  EXPECT_NEAR(expected_start_intercept, calculated_start_y_intercept, kEpsilon);
  EXPECT_NEAR(expected_end_intercept, calculated_end_y_intercept, kEpsilon);
}

// Helper function to check if the a given line segment has an expected
// undefined normal line.
void CheckUndefinedNormalLine(const gfx::PointF& start_point,
                              const gfx::PointF& end_point) {
  float calculated_slope;
  float calculated_start_y_intercept;
  float calculated_end_y_intercept;

  ComputeNormalLineVariables(start_point, end_point, &calculated_slope,
                             &calculated_start_y_intercept,
                             &calculated_end_y_intercept);
  EXPECT_TRUE(std::isnan(calculated_slope));
  EXPECT_TRUE(std::isnan(calculated_start_y_intercept));
  EXPECT_TRUE(std::isnan(calculated_end_y_intercept));
}

// Helper function to check that the projections from the given line variables
// and |distance| match those expected in |expected_projections|.
void CheckProjectedPoints(const gfx::PointF& start_point,
                          float slope,
                          float y_intercept,
                          float distance,
                          std::vector<gfx::PointF>& expected_projections) {
  // There can only be two projections.
  EXPECT_EQ(2u, expected_projections.size());

  gfx::PointF calculated_first_projection;
  gfx::PointF calculated_second_projection;

  ComputeProjectedPoints(start_point, slope, y_intercept, distance,
                         &calculated_first_projection,
                         &calculated_second_projection);

  std::vector<gfx::PointF> calculated_projections = {
      calculated_first_projection, calculated_second_projection};

  // Sort the points, so that we do not have to enter the projections in the
  // right order.
  std::sort(calculated_projections.begin(), calculated_projections.end());
  std::sort(expected_projections.begin(), expected_projections.end());

  EXPECT_EQ(expected_projections[0], calculated_projections[0]);
  EXPECT_EQ(expected_projections[1], calculated_projections[1]);
}

// Helper function that checks that an IsFirstPointSmallerAngle will return
// false if |larger_angle_point| is the first point argument and return true if
// |larger_angle_point| is the second point argument.
void CheckFirstPointSmaller(const gfx::PointF& start_point,
                            const gfx::PointF& end_point,
                            const gfx::PointF& larger_angle_point,
                            const gfx::PointF& smaller_angle_point) {
  EXPECT_FALSE(IsFirstPointSmallerAngle(
      start_point, end_point, larger_angle_point, smaller_angle_point));
  EXPECT_TRUE(IsFirstPointSmallerAngle(
      start_point, end_point, smaller_angle_point, larger_angle_point));
}

using LaserSegmentUtilsTest = testing::Test;

TEST_F(LaserSegmentUtilsTest, AngleOfPointInNewCoordinates) {
  {
    // Verify angles remain the same if the origin is at (0, 0) and the
    // direction remains the same (1, 0).
    const gfx::PointF origin(0.0f, 0.0f);
    const gfx::Vector2dF direction(1.0f, 0.0f);

    // The functions range is (-180.0, 180.0).
    for (float angle = -179.0f; angle < 180.0f; angle += 10.0f) {
      float rad = gfx::DegToRad(angle);
      gfx::PointF new_point(cos(rad), sin(rad));
      CheckAngleOfPointInNewCoordinates(origin, direction, new_point, angle);
    }
  }
  {
    // Verify angles are shifted by 45 degrees if the origin is at (0, 0) and
    // the direction is (1, 1).
    const gfx::PointF origin(0.0f, 0.0f);
    const gfx::Vector2dF direction(1.0f, 1.0f);

    // The functions range is (-180.0, 180.0).
    for (float angle = -179.0f; angle < 180.0f; angle += 10.0f) {
      float rad = gfx::DegToRad(angle + 45.0f);
      gfx::PointF new_point(cos(rad), sin(rad));
      CheckAngleOfPointInNewCoordinates(origin, direction, new_point, angle);
    }
  }
  {
    // Verify angles remain the same if the points are translated by (1, 1),
    // if the origin is at (1, 1) and the direction remains the same (1, 0).
    const gfx::PointF origin(1.0f, 1.0f);
    const gfx::Vector2dF direction(1.0f, 0.0f);

    // The functions range is (-180.0f, 180.0f).
    for (float angle = -179.0f; angle < 180.0f; angle += 10.0f) {
      float rad = gfx::DegToRad(angle);
      gfx::PointF new_point(cos(rad) + origin.x(), sin(rad) + origin.y());
      CheckAngleOfPointInNewCoordinates(origin, direction, new_point, angle);
    }
  }
  {
    // Verify angles are shifted by 45 degress if the points are translated by
    // (1, 1), if the origin is at (1, 1) and the direction remains the same
    // (1, 0).
    const gfx::PointF origin(1.0f, 1.0f);
    const gfx::Vector2dF direction(1.0f, 1.0f);

    // The functions range is (-180.0, 180.0).
    for (float angle = -179.0f; angle < 180.0f; angle += 10.0f) {
      float rad = gfx::DegToRad(angle + 45.0f);
      gfx::PointF new_point(cos(rad) + origin.x(), sin(rad) + origin.y());
      CheckAngleOfPointInNewCoordinates(origin, direction, new_point, angle);
    }
  }
}

TEST_F(LaserSegmentUtilsTest, ComputeNormalLineVariables) {
  {
    // Verify a line y=x should have a normal line y=-x+b. At point (0,0), b
    // should equal y+x = 0. At point (1,1), b should equal y+x = 2.
    const gfx::PointF start(0.0f, 0.0f);
    const gfx::PointF end(1.0f, 1.0f);
    float slope = -1.0f;
    float start_intercept = 0.0f;
    float end_intercept = 2.0f;
    CheckNormalLineVariables(start, end, slope, start_intercept, end_intercept);
  }
  {
    // Verify a line y=-x should have a normal line y=x+b. At point 0.0f), b
    // should equal y-x =.0f. At point (1,-1), b should equal y-x = -2.
    const gfx::PointF start(0.0f, 0.0f);
    const gfx::PointF end(1.0f, -1.0f);
    float slope = 1.0f;
    float start_intercept = 0.0f;
    float end_intercept = -2.0f;
    CheckNormalLineVariables(start, end, slope, start_intercept, end_intercept);
  }
  {
    // Verify a line x=5 should have a normal line y.0f with intercepts at the
    // previous y point.
    const gfx::PointF start(5.0f, 0.0f);
    const gfx::PointF end(5.0f, 5.0f);
    float slope = 0.0f;
    float start_intercept = 0.0f;
    float end_intercept = 5.0f;
    CheckNormalLineVariables(start, end, slope, start_intercept, end_intercept);
  }
  {
    // Verify a line parallel to the x-axis should be undefined. The line values
    // should not matter.
    const gfx::PointF start(0.0f, 5.0f);
    const gfx::PointF end(5.0f, 5.0f);
    CheckUndefinedNormalLine(start, end);
  }
}

TEST_F(LaserSegmentUtilsTest, ComputeProjectedPoints) {
  {
    // Verify projecting along y=x from (0, 0) by distance sqrt(2) should result
    // in two projections: (1, 1) and (-1, -1). We start from (0, 0) and
    // translate by (1, 1) and (-1, -1) (vectors with slope 1) to get to (1, 1)
    // and (-1, -1). The length of the distance from (1, 1) is sqrt(1*1 + 1*1) =
    // sqrt(2).
    const gfx::PointF start(0.0f, 0.0f);
    const float slope = 1.0f;
    const float y_intercept = 0.0f;
    const float distance = sqrt(2.0f);
    std::vector<gfx::PointF> expected_projections = {gfx::PointF(1.0f, 1.0f),
                                                     gfx::PointF(-1.0f, -1.0f)};
    CheckProjectedPoints(start, slope, y_intercept, distance,
                         expected_projections);
  }
  {
    // Verify projecting along y=-2x+2 from (2, -2) by distance 2*sqrt(5) should
    // result in two projections: (0, 2) and (4, -6). We start from (2, -2) and
    // translate by (-2, 4) and (2, -4) (vectors with slope -2) to get (0, 2)
    // and (4, -6). The length of the distance from (2, -2) is sqrt(2*2 + 4*4) =
    // sqrt(20) = 2*sqrt(5).
    const gfx::PointF start(2.0f, -2.0f);
    const float slope = -2.0f;
    const float y_intercept = 2.0f;
    const float distance = 2.0f * sqrt(5.0f);
    std::vector<gfx::PointF> expected_projections = {gfx::PointF(0.0f, 2.0f),
                                                     gfx::PointF(4.0f, -6.0f)};
    CheckProjectedPoints(start, slope, y_intercept, distance,
                         expected_projections);
  }
  {
    // Verify projecting along y=5 from (5,5) by distance 2 should
    // result in two projections: (5,7) and (5,3).
    const gfx::PointF start(5.0f, 5.0f);
    const float slope = 0.0f;
    const float y_intercept = 5.0f;
    const float distance = 2.0f;
    std::vector<gfx::PointF> expected_projections = {gfx::PointF(7.0f, 5.0f),
                                                     gfx::PointF(3.0f, 5.0f)};
    CheckProjectedPoints(start, slope, y_intercept, distance,
                         expected_projections);
  }
}

TEST_F(LaserSegmentUtilsTest, IsFirstPointSmallerAngle) {
  {
    // Verify this function works in the case direction is unchanged.
    const gfx::PointF start_point(0.0f, 0.0f);
    const gfx::PointF end_point(1.0f, 0.0f);
    const gfx::PointF positive_angle(1.0f, 1.0f);
    const gfx::PointF negative_angle(-1.0f, -1.0f);
    CheckFirstPointSmaller(start_point, end_point, positive_angle,
                           negative_angle);
  }
  {
    // Verify this function works in the case direction is 90 degrees.
    const gfx::PointF start_point(0.0f, 0.0f);
    const gfx::PointF end_point(0.0f, 1.0f);
    const gfx::PointF positive_angle(-1.0f, 0.0f);
    const gfx::PointF negative_angle(1.0f, 0.0f);
    CheckFirstPointSmaller(start_point, end_point, positive_angle,
                           negative_angle);
  }
  {
    // Verify this function works in the case direction is 45 degrees.
    const gfx::PointF start_point(0.0f, 0.0f);
    const gfx::PointF end_point(1.0f, 1.0f);
    const gfx::PointF positive_angle(0.0f, 1.0f);
    const gfx::PointF negative_angle(1.0f, 0.0f);
    CheckFirstPointSmaller(start_point, end_point, positive_angle,
                           negative_angle);
  }
  {
    // Verify this function works in the case direction is -135 degrees.
    const gfx::PointF start_point(0.0f, 0.0f);
    const gfx::PointF end_point(-1.0f, -1.0f);
    const gfx::PointF positive_angle(0.0f, -1.0f);
    const gfx::PointF negative_angle(-1.0f, 0.0f);
    CheckFirstPointSmaller(start_point, end_point, positive_angle,
                           negative_angle);
  }
}

}  // namespace ash
