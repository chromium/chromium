// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/laser/laser_segment_utils.h"

#include <cmath>
#include <limits>

#include "base/check_op.h"
#include "base/numerics/angle_conversions.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ash {
namespace {

// Solves the equation x = (-b (+|-) sqrt(b^2 - 4ac)) / 2a. |use_plus|
// determines whether + or - is used in the equation; if |use_plus| is true, +
// is used. |a| cannot be 0 (linear equation). Note: This does not handle the
// case where the roots are complex.
float QuadraticEquation(bool use_plus, float a, float b, float c) {
  DCHECK_NE(0.0f, a);
  return (-1.0f * b + sqrt(b * b - 4.0f * a * c) * (use_plus ? 1.0f : -1.0f)) /
         (2.0f * a);
}
}  // namespace

float AngleOfPointInNewCoordinates(const gfx::PointF& origin,
                                   const gfx::Vector2dF& direction,
                                   const gfx::PointF& point) {
  double angle_degrees = base::RadToDeg(atan2(direction.y(), direction.x()));
  gfx::Transform transform;
  transform.Rotate(-angle_degrees);
  transform.Translate(-origin.x(), -origin.y());
  gfx::PointF point_to_transform = transform.MapPoint(point);
  return atan2(point_to_transform.y(), point_to_transform.x());
}

void ComputeNormalLineVariables(const gfx::PointF& start_point,
                                const gfx::PointF& end_point,
                                float* normal_slope,
                                float* start_y_intercept,
                                float* end_y_intercept) {
  float rise = end_point.y() - start_point.y();
  float run = end_point.x() - start_point.x();
  // If the rise of line between the two points is close to zero, the normal of
  // the line is undefined.
  if (fabs(rise) < 0.0001f) {
    *normal_slope = std::numeric_limits<float>::quiet_NaN();
    *start_y_intercept = std::numeric_limits<float>::quiet_NaN();
    *end_y_intercept = std::numeric_limits<float>::quiet_NaN();
    return;
  }

  *normal_slope = -1.0f * (run / rise);
  *start_y_intercept = start_point.y() - *normal_slope * start_point.x();
  *end_y_intercept = end_point.y() - *normal_slope * end_point.x();
}

void ComputeProjectedPoints(const gfx::PointF& point,
                            float line_slope,
                            float line_y_intercept,
                            float projection_distance,
                            gfx::PointF* first_projection,
                            gfx::PointF* second_projection) {
  // If the slope is NaN, the y-intercept should be NaN too. The line is thus
  // vertical and projections will be projected straight up/down from |point|.
  if (std::isnan(line_slope)) {
    DCHECK(std::isnan(line_y_intercept));

    *first_projection =
        gfx::PointF(point.x(), point.y() + round(projection_distance));
    *second_projection =
        gfx::PointF(point.x(), point.y() - round(projection_distance));
    return;
  }

  // |point| must be on the line defined by |line_slope| and |line_y_intercept|.
  DCHECK_LE(fabs(point.y() - (line_slope * point.x() + line_y_intercept)), 2.f);

  // We want the two points along the line given by |slope|(m) and
  // |y_intercept|(b). If |original_point| is defined as (x,y) and
  // |distance_from_old_point| is d, we want the two (dx,dy) which satisfys the
  // two equations (1)dx^2+dy^2=d^2 and (2)y+dy=m(x+dx)+b. Since y,x,b and m are
  // constants we form a new equation (3)dy=mdx + K, where K=mx+b-y. Plugging
  // (3) into (1) we get dx^2+(mdx)^2+2Kmdx+K^2=d^2 ->
  // (m^2+1)dx^2+(2Km)dx+(K^2-d^2)=0. We can then solve for dx using the
  // quadratic equation with variables a=m^2+1, b=2Km, c=K^2-d^2. We plug
  // dx into (3) to find dy. The new points will then be (x+dx,y+dy).
  float constant = line_y_intercept + line_slope * point.x() - point.y();
  float a = 1.0f + line_slope * line_slope;
  float b = 2.0f * line_slope * constant;
  float c = constant * constant - projection_distance * projection_distance;
  float p1_delta_x = QuadraticEquation(true, a, b, c);
  float p1_delta_y =
      line_slope * (point.x() + p1_delta_x) + line_y_intercept - point.y();
  float p2_delta_x = QuadraticEquation(false, a, b, c);
  float p2_delta_y =
      line_slope * (point.x() + p2_delta_x) + line_y_intercept - point.y();
  *first_projection =
      gfx::PointF(point.x() + round(p1_delta_x), point.y() + round(p1_delta_y));
  *second_projection =
      gfx::PointF(point.x() + round(p2_delta_x), point.y() + round(p2_delta_y));
}

bool IsFirstPointSmallerAngle(const gfx::PointF& start_point,
                              const gfx::PointF& end_point,
                              const gfx::PointF& first_point,
                              const gfx::PointF& second_point) {
  gfx::PointF new_origin(
      start_point.x() + (end_point.x() - start_point.x()) / 2.0f,
      start_point.y() + (end_point.y() - start_point.y()) / 2.0f);
  gfx::Vector2dF direction = end_point - start_point;

  // Compute the angles of the projections relative to the the new origin and
  // direction.
  float end_first_projection_angle =
      AngleOfPointInNewCoordinates(new_origin, direction, first_point);
  float end_second_projection_angle =
      AngleOfPointInNewCoordinates(new_origin, direction, second_point);

  // We want to always have the smaller angle come first.
  return end_first_projection_angle < end_second_projection_angle;
}

}  // namespace ash
