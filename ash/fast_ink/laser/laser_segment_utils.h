// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_LASER_LASER_SEGMENT_UTILS_H_
#define ASH_FAST_INK_LASER_LASER_SEGMENT_UTILS_H_

#include "ash/ash_export.h"

namespace gfx {
class PointF;
class Vector2dF;
}  // namespace gfx

namespace ash {

// Compute the angle in radians of |point|, with |origin| as the origin and
// |direction| as the x-axis. In other words, computes the angle in radians
// between |direction| and |point| - |origin|.
float ASH_EXPORT AngleOfPointInNewCoordinates(const gfx::PointF& origin,
                                              const gfx::Vector2dF& direction,
                                              const gfx::PointF& point);

// Compute the variables for the equation of the lines normal to the line
// segment formed by |start_point| and |end_point|, and which run through the
// endpoints of that line segment. The outputs will be returned as NaN if the
// line segment is parallel to the x-axis (undefined normal lines).
void ASH_EXPORT ComputeNormalLineVariables(const gfx::PointF& start_point,
                                           const gfx::PointF& end_point,
                                           float* normal_slope,
                                           float* start_y_intercept,
                                           float* end_y_intercept);

// Compute the two the projections of |point| along the line defined by
// |line_slope| and |line_y_intercept|. The distance of each projection is
// |projection_distance| from |point|.
void ASH_EXPORT ComputeProjectedPoints(const gfx::PointF& point,
                                       float line_slope,
                                       float line_y_intercept,
                                       float projection_distance,
                                       gfx::PointF* first_projection,
                                       gfx::PointF* second_projection);

// Checks if the angle of |first_point| is smaller than the angle of
// |second_point|. These angles are computed relative to the coordinate system
// defined by the midpoint of |start_point| and |end_point|, with the x-axis
// as |end_point| - |start_point|.
bool ASH_EXPORT IsFirstPointSmallerAngle(const gfx::PointF& start_point,
                                         const gfx::PointF& end_point,
                                         const gfx::PointF& first_point,
                                         const gfx::PointF& second_point);

}  // namespace ash

#endif  // ASH_FAST_INK_LASER_LASER_SEGMENT_UTILS_H_
