// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/math_util.h"

#include <algorithm>
#include <cmath>
#include <limits>
#if defined(ARCH_CPU_X86_FAMILY)
#include <xmmintrin.h>
#endif

#include "base/numerics/angle_conversions.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "ui/gfx/geometry/linear_gradient.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace cc {

static HomogeneousCoordinate ProjectHomogeneousPoint(
    const gfx::Transform& transform,
    const gfx::PointF& p) {
  SkScalar m22 = transform.rc(2, 2);
  // In this case, the layer we are trying to project onto is perpendicular to
  // ray (point p and z-axis direction) that we are trying to project. This
  // happens when the layer is rotated so that it is infinitesimally thin, or
  // when it is co-planar with the camera origin -- i.e. when the layer is
  // invisible anyway.
  if (!std::isnormal(m22))
    return HomogeneousCoordinate(0.0, 0.0, 0.0, 1.0);
  SkScalar z = -(transform.rc(2, 0) * p.x() + transform.rc(2, 1) * p.y() +
                 transform.rc(2, 3)) /
               m22;
  // Same underlying condition as the previous early return.
  if (!std::isfinite(z))
    return HomogeneousCoordinate(0.0, 0.0, 0.0, 1.0);

  HomogeneousCoordinate result(p.x(), p.y(), z, 1.0);
  transform.TransformVector4(result.vec.data());
  return result;
}

static HomogeneousCoordinate ProjectHomogeneousPoint(
    const gfx::Transform& transform,
    const gfx::PointF& p,
    bool* clipped) {
  HomogeneousCoordinate h = ProjectHomogeneousPoint(transform, p);
  *clipped = h.w() <= 0;
  return h;
}

static HomogeneousCoordinate MapHomogeneousPoint(
    const gfx::Transform& transform,
    const gfx::PointF& p) {
  HomogeneousCoordinate result(p.x(), p.y(), 0.0, 1.0);
  transform.TransformVector4(result.vec.data());
  return result;
}

namespace {

// This is the tolerance for detecting an eyepoint-aligned edge.
const float kStationaryPointEpsilon = 0.00001f;

}  // namespace

static void homogeneousLimitAtZero(SkScalar a1,
                                   SkScalar w1,
                                   SkScalar a2,
                                   SkScalar w2,
                                   float t,
                                   float* limit) {
  if (std::abs(a1 * w2 / w1 / a2 - 1.0f) > kStationaryPointEpsilon) {
    // We are going to explode towards an infinity, but we choose the one that
    // corresponds to the one on the positive side of w.
    if (((1.0f - t) * a1 + t * a2) > 0) {
      *limit = HomogeneousCoordinate::kInfiniteCoordinate;
    } else {
      *limit = -HomogeneousCoordinate::kInfiniteCoordinate;
    }
  } else {
    *limit = a1 / w1;  // (== a2 / w2) && == (1.0f - t) * a1 / w1 + t * a2 / w2
  }
}

static gfx::PointF ComputeClippedCartesianPoint2dForEdge(
    const HomogeneousCoordinate& h1,
    const HomogeneousCoordinate& h2) {
  // Points h1 and h2 form a line in 4d, and any point on that line can be
  // represented as an interpolation between h1 and h2:
  //    p = (1-t) h1 + (t) h2
  //
  // We want to compute the limit in 2 space of
  //    x = ((1-t) h1.x + (t) h2.x) / ((1-t) h1.w + (t) h2.w)
  //    y = ((1-t) h1.y + (t) h2.y) / ((1-t) h1.w + (t) h2.w)
  // as ((1-t) h1.w + (t) h2.w) -> 0+

  // The only answers to this are h1.x/h1.w == h2.x/h2.w, +/- infinity
  // i.e., either the coordinate is not moving, or is trending to one
  // infinity or the other.

  // This assertion isn't really as strong as it looks because
  // std::isfinite(h1.w()) or std::isfinite(h2.w()) might not be true
  // (and they could be NaN).
  // TODO(crbug.com/40186138): We should be able to assert something
  // stronger here, and avoid dependencies on undefined floating point
  // behavior.
  DCHECK_NE(h1.w() <= 0, h2.w() <= 0);

  float t = h1.w() / (h1.w() - h2.w());
  float x;
  float y;

  homogeneousLimitAtZero(h1.x(), h1.w(), h2.x(), h2.w(), t, &x);
  homogeneousLimitAtZero(h1.y(), h1.w(), h2.y(), h2.w(), t, &y);

  return gfx::PointF(x, y);
}

static void homogeneousLimitNearZero(SkScalar a1,
                                     SkScalar w1,
                                     SkScalar a2,
                                     SkScalar w2,
                                     float t,
                                     float* limit) {
  if (std::abs(a1 * w2 / w1 / a2 - 1.0f) > kStationaryPointEpsilon) {
    // t has been computed so that w is near but not at zero.
    *limit = ((1.0f - t) * a1 + t * a2) / ((1.0f - t) * w1 + t * w2);
    // std::abs(*limit) should now be somewhere near
    // HomogeneousCoordinate::kInfiniteCoordinate, preferably smaller than it,
    // but there are edge cases where it will be larger (for example, if the
    // point where a crosses 0 is very close to the point where w crosses 0),
    // so it's hard to DCHECK() that this is the case.
  } else {
    *limit = a1 / w1;  // (== a2 / w2) && == (1.0f - t) * a1 / w1 + t * a2 / w2
  }
}

static gfx::Point3F ComputeClippedCartesianPoint3dForEdge(
    const HomogeneousCoordinate& h1,
    const HomogeneousCoordinate& h2) {
  // Points h1 and h2 form a line in 4d, and any point on that line can be
  // represented as an interpolation between h1 and h2:
  //    p = (1-t) h1 + (t) h2
  //
  // We want to compute the limit in 3 space of
  //    x = ((1-t) h1.x + (t) h2.x) / ((1-t) h1.w + (t) h2.w)
  //    y = ((1-t) h1.y + (t) h2.y) / ((1-t) h1.w + (t) h2.w)
  //    z = ((1-t) h1.z + (t) h2.z) / ((1-t) h1.w + (t) h2.w)
  // as ((1-t) h1.w + (t) h2.w) -> 0+

  // The only answers to this are h1.x/h1.w == h2.x/h2.w, +/- infinity
  // i.e., either the coordinate is not moving, or is trending to one
  // infinity or the other.

  // When we clamp to HomogeneousCoordinate::kInfiniteCoordinate we want
  // to keep the result in the correct plane, which we do by computing
  // a t that will result in the largest (in absolute value) of x, y, or
  // z being HomogeneousCoordinate::kInfiniteCoordinate

  // This assertion isn't really as strong as it looks because
  // std::isfinite(h1.w()) or std::isfinite(h2.w()) might not be true
  // (and they could be NaN).
  // TODO(crbug.com/40186138): We should be able to assert something
  // stronger here, and avoid dependencies on undefined floating point
  // behavior.
  DCHECK_NE(h1.w() <= 0, h2.w() <= 0);

  float w_diff = h1.w() - h2.w();
  float t = h1.w() / w_diff;
  float max_numerator = std::max({std::abs((1.0f - t) * h1.x() + t * h2.x()),
                                  std::abs((1.0f - t) * h1.y() + t * h2.y()),
                                  std::abs((1.0f - t) * h1.z() + t * h2.z())});

  // Shift t away from the point where w is zero, far enough so that the
  // largest of the resulting x, y, and z will be about
  // kInfiniteCoordinate.  Add an extra epsilon() / 2.0 so that there's
  // always enough movement (in case t_shift is very small, which it
  // often is).
  const float t_shift =
      max_numerator / w_diff / HomogeneousCoordinate::kInfiniteCoordinate;
  constexpr float half_epsilon = std::numeric_limits<float>::epsilon() / 2.0f;
  DCHECK_EQ(w_diff > 0, t_shift > 0);
  if (w_diff > 0) {
    t = std::max(0.0f, t - (t_shift + half_epsilon));
  } else {
    t = std::min(1.0f, t - (t_shift - half_epsilon));
  }

  float x;
  float y;
  float z;

  homogeneousLimitNearZero(h1.x(), h1.w(), h2.x(), h2.w(), t, &x);
  homogeneousLimitNearZero(h1.y(), h1.w(), h2.y(), h2.w(), t, &y);
  homogeneousLimitNearZero(h1.z(), h1.w(), h2.z(), h2.w(), t, &z);

  return gfx::Point3F(x, y, z);
}

static inline void ExpandBoundsToIncludePoint(float* xmin,
                                              float* xmax,
                                              float* ymin,
                                              float* ymax,
                                              const gfx::PointF& p) {
  *xmin = std::min(p.x(), *xmin);
  *xmax = std::max(p.x(), *xmax);
  *ymin = std::min(p.y(), *ymin);
  *ymax = std::max(p.y(), *ymax);
}

static inline bool IsNearlyTheSame(float f, float g) {
  // The idea behind this is to use this fraction of the larger of the
  // two numbers as the limit of the difference.  This breaks down near
  // zero, so we reuse this as the minimum absolute size we will use
  // for the base of the scale too.
  static const float epsilon_scale = 0.00001f;
  return std::abs(f - g) <
         epsilon_scale * std::max({std::abs(f), std::abs(g), epsilon_scale});
}

static inline bool IsNearlyTheSame(const gfx::PointF& lhs,
                                   const gfx::PointF& rhs) {
  return IsNearlyTheSame(lhs.x(), rhs.x()) && IsNearlyTheSame(lhs.y(), rhs.y());
}

static inline bool IsNearlyTheSame(const gfx::Point3F& lhs,
                                   const gfx::Point3F& rhs) {
  return IsNearlyTheSame(lhs.x(), rhs.x()) &&
         IsNearlyTheSame(lhs.y(), rhs.y()) && IsNearlyTheSame(lhs.z(), rhs.z());
}

static inline void AddVertexToClippedQuad3d(
    const gfx::Point3F& new_vertex,
    base::span<gfx::Point3F, 6> clipped_quad,
    int* num_vertices_in_clipped_quad,
    bool* need_to_clamp) {
  if (*num_vertices_in_clipped_quad > 0 &&
      IsNearlyTheSame(clipped_quad[*num_vertices_in_clipped_quad - 1],
                      new_vertex))
    return;

  DCHECK_LT(*num_vertices_in_clipped_quad, 6);
  clipped_quad[*num_vertices_in_clipped_quad] = new_vertex;
  (*num_vertices_in_clipped_quad)++;
  if (new_vertex.x() < -HomogeneousCoordinate::kInfiniteCoordinate ||
      new_vertex.x() > HomogeneousCoordinate::kInfiniteCoordinate ||
      new_vertex.y() < -HomogeneousCoordinate::kInfiniteCoordinate ||
      new_vertex.y() > HomogeneousCoordinate::kInfiniteCoordinate ||
      new_vertex.z() < -HomogeneousCoordinate::kInfiniteCoordinate ||
      new_vertex.z() > HomogeneousCoordinate::kInfiniteCoordinate) {
    *need_to_clamp = true;
  }
}

gfx::Rect MathUtil::MapEnclosingClippedRect(const gfx::Transform& transform,
                                            const gfx::Rect& src_rect) {
  return MapEnclosingClippedRectIgnoringError(transform, src_rect, 0.f);
}

gfx::Rect MathUtil::MapEnclosingClippedRectIgnoringError(
    const gfx::Transform& transform,
    const gfx::Rect& src_rect,
    float ignore_error) {
  if (transform.IsIdentityOrIntegerTranslation())
    return src_rect + gfx::ToFlooredVector2d(transform.To2dTranslation());

  gfx::RectF mapped_rect = MapClippedRect(transform, gfx::RectF(src_rect));
  return gfx::ToEnclosingRectIgnoringError(mapped_rect, ignore_error);
}

gfx::RectF MathUtil::MapClippedRect(const gfx::Transform& transform,
                                    const gfx::RectF& src_rect) {
  if (transform.IsIdentityOrTranslation())
    return src_rect + transform.To2dTranslation();

  // Apply the transform, but retain the result in homogeneous coordinates.
  HomogeneousCoordinate hc0 = MapHomogeneousPoint(transform, src_rect.origin());
  HomogeneousCoordinate hc1 =
      MapHomogeneousPoint(transform, src_rect.top_right());
  HomogeneousCoordinate hc2 =
      MapHomogeneousPoint(transform, src_rect.bottom_right());
  HomogeneousCoordinate hc3 =
      MapHomogeneousPoint(transform, src_rect.bottom_left());

  return ComputeEnclosingClippedRect(hc0, hc1, hc2, hc3);
}

gfx::Rect MathUtil::ProjectEnclosingClippedRect(const gfx::Transform& transform,
                                                const gfx::Rect& src_rect) {
  if (transform.IsIdentityOrIntegerTranslation())
    return src_rect + gfx::ToFlooredVector2d(transform.To2dTranslation());

  gfx::RectF projected_rect =
      ProjectClippedRect(transform, gfx::RectF(src_rect));

  // gfx::ToEnclosingRect crashes if called on a RectF with any NaN coordinate.
  if (std::isnan(projected_rect.x()) || std::isnan(projected_rect.y()) ||
      std::isnan(projected_rect.right()) || std::isnan(projected_rect.bottom()))
    return gfx::Rect();

  return gfx::ToEnclosingRect(projected_rect);
}

gfx::RectF MathUtil::ProjectClippedRect(const gfx::Transform& transform,
                                        const gfx::RectF& src_rect) {
  if (transform.IsIdentityOrTranslation())
    return src_rect + transform.To2dTranslation();

  // Perform the projection, but retain the result in homogeneous coordinates.
  gfx::QuadF q = gfx::QuadF(src_rect);
  HomogeneousCoordinate h1 = ProjectHomogeneousPoint(transform, q.p1());
  HomogeneousCoordinate h2 = ProjectHomogeneousPoint(transform, q.p2());
  HomogeneousCoordinate h3 = ProjectHomogeneousPoint(transform, q.p3());
  HomogeneousCoordinate h4 = ProjectHomogeneousPoint(transform, q.p4());

  return ComputeEnclosingClippedRect(h1, h2, h3, h4);
}

gfx::QuadF MathUtil::InverseMapQuadToLocalSpace(
    const gfx::Transform& device_transform,
    const gfx::QuadF& device_quad) {
  DCHECK(device_transform.IsFlat());
  gfx::Transform inverse_device_transform =
      device_transform.GetCheckedInverse();
  bool clipped = false;
  gfx::QuadF local_quad =
      MathUtil::MapQuad(inverse_device_transform, device_quad, &clipped);
  // We should not DCHECK(!clipped) here, because anti-aliasing inflation may
  // cause device_quad to become clipped. To our knowledge this scenario does
  // not need to be handled differently than the unclipped case.
  return local_quad;
}

gfx::Rect MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
    const gfx::Transform& transform,
    const gfx::Rect& rect) {
  DCHECK(transform.Preserves2dAxisAlignment());
  DCHECK_GT(transform.rc(3, 3), 0);
  DCHECK(std::isnormal(transform.rc(3, 3)));

  if (transform.IsIdentityOrIntegerTranslation())
    return rect + gfx::ToFlooredVector2d(transform.To2dTranslation());
  if (transform.IsIdentityOrTranslation()) {
    gfx::Vector2dF offset = transform.To2dTranslation();
    return gfx::ToEnclosedRect(gfx::RectF(rect) + offset);
  }

  HomogeneousCoordinate hc0 =
      MapHomogeneousPoint(transform, gfx::PointF(rect.origin()));
  HomogeneousCoordinate hc1 =
      MapHomogeneousPoint(transform, gfx::PointF(rect.bottom_right()));
  DCHECK(!hc0.ShouldBeClipped());
  DCHECK(!hc1.ShouldBeClipped());

  gfx::PointF top_left(hc0.CartesianPoint2d());
  gfx::PointF bottom_right(hc1.CartesianPoint2d());
  return gfx::ToEnclosedRect(gfx::BoundingRect(top_left, bottom_right));
}

bool MathUtil::MapClippedQuad3d(const gfx::Transform& transform,
                                const gfx::QuadF& src_quad,
                                base::span<gfx::Point3F, 6> clipped_quad,
                                int* num_vertices_in_clipped_quad) {
  // This is different from the 2D version because, when we clamp
  // coordinates to [-HomogeneousCoordinate::kInfiniteCoordinate,
  // HomogeneousCoordinate::kInfiniteCoordinate], we need to do the
  // clamping while keeping the points coplanar.

  HomogeneousCoordinate h1 = MapHomogeneousPoint(transform, src_quad.p1());
  HomogeneousCoordinate h2 = MapHomogeneousPoint(transform, src_quad.p2());
  HomogeneousCoordinate h3 = MapHomogeneousPoint(transform, src_quad.p3());
  HomogeneousCoordinate h4 = MapHomogeneousPoint(transform, src_quad.p4());

  // The order of adding the vertices to the array is chosen so that
  // clockwise / counter-clockwise orientation is retained.

  *num_vertices_in_clipped_quad = 0;
  bool need_to_clamp = false;

  if (!h1.ShouldBeClipped()) {
    AddVertexToClippedQuad3d(h1.CartesianPoint3dUnclamped(), clipped_quad,
                             num_vertices_in_clipped_quad, &need_to_clamp);
  }

  if (h1.ShouldBeClipped() ^ h2.ShouldBeClipped()) {
    AddVertexToClippedQuad3d(ComputeClippedCartesianPoint3dForEdge(h1, h2),
                             clipped_quad, num_vertices_in_clipped_quad,
                             &need_to_clamp);
  }

  if (!h2.ShouldBeClipped()) {
    AddVertexToClippedQuad3d(h2.CartesianPoint3dUnclamped(), clipped_quad,
                             num_vertices_in_clipped_quad, &need_to_clamp);
  }

  if (h2.ShouldBeClipped() ^ h3.ShouldBeClipped()) {
    AddVertexToClippedQuad3d(ComputeClippedCartesianPoint3dForEdge(h2, h3),
                             clipped_quad, num_vertices_in_clipped_quad,
                             &need_to_clamp);
  }

  if (!h3.ShouldBeClipped()) {
    AddVertexToClippedQuad3d(h3.CartesianPoint3dUnclamped(), clipped_quad,
                             num_vertices_in_clipped_quad, &need_to_clamp);
  }

  if (h3.ShouldBeClipped() ^ h4.ShouldBeClipped()) {
    AddVertexToClippedQuad3d(ComputeClippedCartesianPoint3dForEdge(h3, h4),
                             clipped_quad, num_vertices_in_clipped_quad,
                             &need_to_clamp);
  }

  if (!h4.ShouldBeClipped()) {
    AddVertexToClippedQuad3d(h4.CartesianPoint3dUnclamped(), clipped_quad,
                             num_vertices_in_clipped_quad, &need_to_clamp);
  }

  if (h4.ShouldBeClipped() ^ h1.ShouldBeClipped()) {
    AddVertexToClippedQuad3d(ComputeClippedCartesianPoint3dForEdge(h4, h1),
                             clipped_quad, num_vertices_in_clipped_quad,
                             &need_to_clamp);
  }

  if (*num_vertices_in_clipped_quad > 2 &&
      IsNearlyTheSame(clipped_quad[0],
                      clipped_quad[*num_vertices_in_clipped_quad - 1]))
    *num_vertices_in_clipped_quad -= 1;

  if (need_to_clamp) {
    // Some of the values need to be clamped, but we need to keep them
    // coplanar while doing so.

    // First, build a normal vector to the plane by averaging the
    // cross products of adjacent edges.
    gfx::Vector3dF normal(0.0f, 0.0f, 0.0f);
    if (*num_vertices_in_clipped_quad > 2) {
      gfx::Vector3dF loop_vector =
          clipped_quad[0] - clipped_quad[*num_vertices_in_clipped_quad - 1];
      gfx::Vector3dF prev_vector(loop_vector);
      for (int i = 1; i < *num_vertices_in_clipped_quad; ++i) {
        gfx::Vector3dF cur_vector = clipped_quad[i] - clipped_quad[i - 1];
        normal += CrossProduct(prev_vector, cur_vector);
        prev_vector = cur_vector;
      }
      normal += CrossProduct(prev_vector, loop_vector);
    }

    bool clamp_by_points = false;
    float length = normal.Length();
    if (std::isnormal(length)) {  // exclude 0, denormals, +/- inf, NaN
      normal.InvScale(length);

      // Find the vector to the point in the plane closest to (0,0,0).
      gfx::Vector3dF shortest_from_zero(normal);
      shortest_from_zero.Scale(
          DotProduct(normal, clipped_quad[0] - gfx::Point3F(0.0f, 0.0f, 0.0f)));

      // Find the the point in the plane that is at x=0 and y=0
      float z_at_xy_zero = 0.0f;
      if (shortest_from_zero.x() == 0.0f && shortest_from_zero.y() == 0.0f) {
        z_at_xy_zero = shortest_from_zero.z();
      } else if (shortest_from_zero.z() != 0) {
        // Compute the vector v pointing from the shortest_from_zero
        // point to the point with x=0 and y=0.  If both v and normal
        // are projected into the x/y plane, they should point in
        // opposite directions.
        gfx::Vector3dF v = CrossProduct(
            normal, CrossProduct(gfx::Vector3dF(0.0f, 0.0f, 1.0f), normal));
        DCHECK(std::abs(normal.x() * v.y() - normal.y() * v.x()) < 0.00001f);
        // It doesn't matter whether we use x or y, unless one of them
        // is zero or very close to it.
        float r = std::abs(v.x()) > std::abs(v.y())
                      ? shortest_from_zero.x() / v.x()
                      : shortest_from_zero.y() / v.y();
        z_at_xy_zero = shortest_from_zero.z() - v.z() * r;
      } else {
        // Plane is parallel to the z axis.  This means it's not
        // visible, so just fall back to clamping by points.
        clamp_by_points = true;
      }

      if (!clamp_by_points) {
        // If z_at_xy_zero is more than 3/4 of kInfiniteCoordinate
        // distance from zero, move everything in the z axis so
        // z_at_xy_zero is that distance from zero, so that we don't end
        // up clamping away the parts that fit within what's likely to
        // be the visible area.
        constexpr float max_distance =
            0.75 * HomogeneousCoordinate::kInfiniteCoordinate;
        if (std::abs(z_at_xy_zero) > max_distance) {
          float z_delta;
          if (z_at_xy_zero > 0) {
            z_delta = max_distance - z_at_xy_zero;
          } else {
            z_delta = -max_distance - z_at_xy_zero;
          }
          for (int i = 0; i < *num_vertices_in_clipped_quad; ++i) {
            clipped_quad[i].set_z(clipped_quad[i].z() + z_delta);
          }
          z_at_xy_zero += z_delta;
        }

        // Move all the points towards (0, 0, z_at_xy_zero) until all
        // their coordinates are less than kInfiniteCoordinate.
        for (int i = 0; i < *num_vertices_in_clipped_quad; ++i) {
          gfx::Point3F& point = clipped_quad[i];
          float t = 1.0f;

          float x_abs = std::abs(point.x());
          if (x_abs > HomogeneousCoordinate::kInfiniteCoordinate) {
            t = std::min(t, HomogeneousCoordinate::kInfiniteCoordinate / x_abs);
          }

          float y_abs = std::abs(point.y());
          if (y_abs > HomogeneousCoordinate::kInfiniteCoordinate) {
            t = std::min(t, HomogeneousCoordinate::kInfiniteCoordinate / y_abs);
          }

          float z = point.z();
          if (std::abs(z) > HomogeneousCoordinate::kInfiniteCoordinate) {
            // From the clamping to max_distance above, we should have
            // made std::abs(z_at_xy_zero) < kInfiniteCoordinate.
            // However, if it started off very large we might not have.
            float z_at_xy_zero_clamped =
                std::min(float{HomogeneousCoordinate::kInfiniteCoordinate},
                         std::max(-HomogeneousCoordinate::kInfiniteCoordinate,
                                  z_at_xy_zero));
            float z_offset = z - z_at_xy_zero_clamped;
            float z_space =
                (z > 0 ? HomogeneousCoordinate::kInfiniteCoordinate
                       : -HomogeneousCoordinate::kInfiniteCoordinate) -
                z_at_xy_zero_clamped;
            DCHECK_NE(z_offset, 0.0f);
            DCHECK_NE(z_space, 0.0f);
            DCHECK_EQ(z_offset > 0, z_space > 0);
            t = std::min(t, z_space / z_offset);
          }

          if (t != 1.0f) {
            DCHECK(0.0f <= t && t < 1.0f);
            point.set_x(t * point.x());
            point.set_y(t * point.y());
            point.set_z((1.0f - t) * z_at_xy_zero + t * point.z());
          }
        }
      }
    } else {
      // Our points were colinear, so there's no plane to maintain.
      clamp_by_points = true;
    }

    if (clamp_by_points) {
      // Just clamp each point separately in each axis, just like we do
      // for 2D.
      for (int i = 0; i < *num_vertices_in_clipped_quad; ++i) {
        gfx::Point3F& point = clipped_quad[i];
        point.set_x(
            std::clamp(point.x(), -HomogeneousCoordinate::kInfiniteCoordinate,
                       float{HomogeneousCoordinate::kInfiniteCoordinate}));
        point.set_y(
            std::clamp(point.y(), -HomogeneousCoordinate::kInfiniteCoordinate,
                       float{HomogeneousCoordinate::kInfiniteCoordinate}));
        point.set_z(
            std::clamp(point.z(), -HomogeneousCoordinate::kInfiniteCoordinate,
                       float{HomogeneousCoordinate::kInfiniteCoordinate}));
      }
    }
  }

  DCHECK_LE(*num_vertices_in_clipped_quad, 6);
  return (*num_vertices_in_clipped_quad >= 4);
}

gfx::RectF MathUtil::ComputeEnclosingRectOfVertices(
    base::span<const gfx::PointF> vertices) {
  if (vertices.size() < 2) {
    return gfx::RectF();
  }

  float xmin = std::numeric_limits<float>::max();
  float xmax = -std::numeric_limits<float>::max();
  float ymin = std::numeric_limits<float>::max();
  float ymax = -std::numeric_limits<float>::max();

  for (auto& vertex : vertices) {
    ExpandBoundsToIncludePoint(&xmin, &xmax, &ymin, &ymax, vertex);
  }

  return gfx::RectF(gfx::PointF(xmin, ymin),
                    gfx::SizeF(xmax - xmin, ymax - ymin));
}

gfx::RectF MathUtil::ComputeEnclosingClippedRect(
    const HomogeneousCoordinate& h1,
    const HomogeneousCoordinate& h2,
    const HomogeneousCoordinate& h3,
    const HomogeneousCoordinate& h4) {
  // This function performs clipping as necessary and computes the enclosing 2d
  // gfx::RectF of the vertices. Doing these two steps simultaneously allows us
  // to avoid the overhead of storing an unknown number of clipped vertices.

  // If no vertices on the quad are clipped, then we can simply return the
  // enclosing rect directly.
  bool something_clipped = h1.ShouldBeClipped() || h2.ShouldBeClipped() ||
                           h3.ShouldBeClipped() || h4.ShouldBeClipped();
  if (!something_clipped) {
    gfx::QuadF mapped_quad = gfx::QuadF(h1.CartesianPoint2d(),
                                        h2.CartesianPoint2d(),
                                        h3.CartesianPoint2d(),
                                        h4.CartesianPoint2d());
    return mapped_quad.BoundingBox();
  }

  bool everything_clipped = h1.ShouldBeClipped() && h2.ShouldBeClipped() &&
                            h3.ShouldBeClipped() && h4.ShouldBeClipped();
  if (everything_clipped)
    return gfx::RectF();

  float xmin = std::numeric_limits<float>::max();
  float xmax = -std::numeric_limits<float>::max();
  float ymin = std::numeric_limits<float>::max();
  float ymax = -std::numeric_limits<float>::max();

  if (!h1.ShouldBeClipped())
    ExpandBoundsToIncludePoint(&xmin, &xmax, &ymin, &ymax,
                               h1.CartesianPoint2d());

  if (h1.ShouldBeClipped() ^ h2.ShouldBeClipped())
    ExpandBoundsToIncludePoint(&xmin, &xmax, &ymin, &ymax,
                               ComputeClippedCartesianPoint2dForEdge(h1, h2));

  if (!h2.ShouldBeClipped())
    ExpandBoundsToIncludePoint(&xmin, &xmax, &ymin, &ymax,
                               h2.CartesianPoint2d());

  if (h2.ShouldBeClipped() ^ h3.ShouldBeClipped())
    ExpandBoundsToIncludePoint(&xmin, &xmax, &ymin, &ymax,
                               ComputeClippedCartesianPoint2dForEdge(h2, h3));

  if (!h3.ShouldBeClipped())
    ExpandBoundsToIncludePoint(&xmin, &xmax, &ymin, &ymax,
                               h3.CartesianPoint2d());

  if (h3.ShouldBeClipped() ^ h4.ShouldBeClipped())
    ExpandBoundsToIncludePoint(&xmin, &xmax, &ymin, &ymax,
                               ComputeClippedCartesianPoint2dForEdge(h3, h4));

  if (!h4.ShouldBeClipped())
    ExpandBoundsToIncludePoint(&xmin, &xmax, &ymin, &ymax,
                               h4.CartesianPoint2d());

  if (h4.ShouldBeClipped() ^ h1.ShouldBeClipped())
    ExpandBoundsToIncludePoint(&xmin, &xmax, &ymin, &ymax,
                               ComputeClippedCartesianPoint2dForEdge(h4, h1));

  return gfx::RectF(gfx::PointF(xmin, ymin),
                    gfx::SizeF(xmax - xmin, ymax - ymin));
}

gfx::QuadF MathUtil::MapQuad(const gfx::Transform& transform,
                             const gfx::QuadF& q,
                             bool* clipped) {
  if (transform.IsIdentityOrTranslation()) {
    gfx::QuadF mapped_quad(q);
    mapped_quad += transform.To2dTranslation();
    *clipped = false;
    return mapped_quad;
  }

  HomogeneousCoordinate h1 = MapHomogeneousPoint(transform, q.p1());
  HomogeneousCoordinate h2 = MapHomogeneousPoint(transform, q.p2());
  HomogeneousCoordinate h3 = MapHomogeneousPoint(transform, q.p3());
  HomogeneousCoordinate h4 = MapHomogeneousPoint(transform, q.p4());

  *clipped = h1.ShouldBeClipped() || h2.ShouldBeClipped() ||
            h3.ShouldBeClipped() || h4.ShouldBeClipped();

  // Result will be invalid if clipped == true. But, compute it anyway just in
  // case, to emulate existing behavior.
  return gfx::QuadF(h1.CartesianPoint2d(),
                    h2.CartesianPoint2d(),
                    h3.CartesianPoint2d(),
                    h4.CartesianPoint2d());
}

gfx::PointF MathUtil::MapPoint(const gfx::Transform& transform,
                               const gfx::PointF& p,
                               bool* clipped) {
  HomogeneousCoordinate h = MapHomogeneousPoint(transform, p);

  if (h.w() > 0) {
    *clipped = false;
    return h.CartesianPoint2d();
  }

  // The cartesian coordinates will be invalid after dividing by w.
  *clipped = true;

  // Avoid dividing by w if w == 0.
  if (!h.w())
    return gfx::PointF();

  // This return value will be invalid because clipped == true, but (1) users of
  // this code should be ignoring the return value when clipped == true anyway,
  // and (2) this behavior is more consistent with existing behavior of WebKit
  // transforms if the user really does not ignore the return value.
  return h.CartesianPoint2d();
}

gfx::PointF MathUtil::ProjectPoint(const gfx::Transform& transform,
                                   const gfx::PointF& p,
                                   bool* clipped) {
  HomogeneousCoordinate h = ProjectHomogeneousPoint(transform, p, clipped);
  // Avoid dividing by w if w == 0.
  if (!h.w())
    return gfx::PointF();

  // This return value will be invalid if clipped == true, but (1) users of
  // this code should be ignoring the return value when clipped == true anyway,
  // and (2) this behavior is more consistent with existing behavior of WebKit
  // transforms if the user really does not ignore the return value.
  return h.CartesianPoint2d();
}

gfx::Point3F MathUtil::ProjectPoint3D(const gfx::Transform& transform,
                                      const gfx::PointF& p,
                                      bool* clipped) {
  HomogeneousCoordinate h = ProjectHomogeneousPoint(transform, p, clipped);
  if (!h.w())
    return gfx::Point3F();
  return h.CartesianPoint3d();
}

gfx::RectF MathUtil::ScaleRectProportional(const gfx::RectF& input_outer_rect,
                                           const gfx::RectF& scale_outer_rect,
                                           const gfx::RectF& scale_inner_rect) {
  gfx::RectF output_inner_rect = input_outer_rect;
  float scale_rect_to_input_scale_x =
      scale_outer_rect.width() / input_outer_rect.width();
  float scale_rect_to_input_scale_y =
      scale_outer_rect.height() / input_outer_rect.height();

  gfx::Vector2dF top_left_diff =
      scale_inner_rect.origin() - scale_outer_rect.origin();
  gfx::Vector2dF bottom_right_diff =
      scale_inner_rect.bottom_right() - scale_outer_rect.bottom_right();
  output_inner_rect.Inset(
      gfx::InsetsF::TLBR(top_left_diff.y() / scale_rect_to_input_scale_y,
                         top_left_diff.x() / scale_rect_to_input_scale_x,
                         -bottom_right_diff.y() / scale_rect_to_input_scale_y,
                         -bottom_right_diff.x() / scale_rect_to_input_scale_x));
  return output_inner_rect;
}

float MathUtil::SmallestAngleBetweenVectors(const gfx::Vector2dF& v1,
                                            const gfx::Vector2dF& v2) {
  double dot_product = gfx::DotProduct(v1, v2) / v1.Length() / v2.Length();
  // Clamp to compensate for rounding errors.
  dot_product = std::clamp(dot_product, -1.0, 1.0);
  return static_cast<float>(base::RadToDeg(std::acos(dot_product)));
}

gfx::Vector2dF MathUtil::ProjectVector(const gfx::Vector2dF& source,
                                       const gfx::Vector2dF& destination) {
  float projected_length =
      gfx::DotProduct(source, destination) / destination.LengthSquared();
  return gfx::Vector2dF(projected_length * destination.x(),
                        projected_length * destination.y());
}

bool MathUtil::FromValue(const base::Value* raw_value, gfx::Rect* out_rect) {
  if (!raw_value->is_list())
    return false;

  const base::Value::List& list = raw_value->GetList();

  if (list.size() != 4)
    return false;

  for (const auto& val : list) {
    if (!val.is_int()) {
      return false;
    }
  }

  int x = list[0].GetInt();
  int y = list[1].GetInt();
  int w = list[2].GetInt();
  int h = list[3].GetInt();

  *out_rect = gfx::Rect(x, y, w, h);
  return true;
}

void MathUtil::AddToTracedValue(const char* name,
                                const gfx::Size& s,
                                base::trace_event::TracedValue* res) {
  res->BeginDictionary(name);
  res->SetDouble("width", s.width());
  res->SetDouble("height", s.height());
  res->EndDictionary();
}

void MathUtil::AddToTracedValue(const char* name,
                                const gfx::SizeF& s,
                                base::trace_event::TracedValue* res) {
  res->BeginDictionary(name);
  res->SetDouble("width", s.width());
  res->SetDouble("height", s.height());
  res->EndDictionary();
}

void MathUtil::AddToTracedValue(const char* name,
                                const gfx::Rect& r,
                                base::trace_event::TracedValue* res) {
  res->BeginArray(name);
  res->AppendInteger(r.x());
  res->AppendInteger(r.y());
  res->AppendInteger(r.width());
  res->AppendInteger(r.height());
  res->EndArray();
}

void MathUtil::AddToTracedValue(const char* name,
                                const gfx::Point& pt,
                                base::trace_event::TracedValue* res) {
  res->BeginArray(name);
  res->AppendInteger(pt.x());
  res->AppendInteger(pt.y());
  res->EndArray();
}

void MathUtil::AddToTracedValue(const char* name,
                                const gfx::PointF& pt,
                                base::trace_event::TracedValue* res) {
  res->BeginArray(name);
  res->AppendDouble(pt.x());
  res->AppendDouble(pt.y());
  res->EndArray();
}

void MathUtil::AddToTracedValue(const char* name,
                                const gfx::Point3F& pt,
                                base::trace_event::TracedValue* res) {
  res->BeginArray(name);
  res->AppendDouble(pt.x());
  res->AppendDouble(pt.y());
  res->AppendDouble(pt.z());
  res->EndArray();
}

void MathUtil::AddToTracedValue(const char* name,
                                const gfx::Vector2d& v,
                                base::trace_event::TracedValue* res) {
  res->BeginArray(name);
  res->AppendInteger(v.x());
  res->AppendInteger(v.y());
  res->EndArray();
}

void MathUtil::AddToTracedValue(const char* name,
                                const gfx::Vector2dF& v,
                                base::trace_event::TracedValue* res) {
  res->BeginArray(name);
  res->AppendDouble(v.x());
  res->AppendDouble(v.y());
  res->EndArray();
}

void MathUtil::AddToTracedValue(const char* name,
                                const gfx::QuadF& q,
                                base::trace_event::TracedValue* res) {
  res->BeginArray(name);
  res->AppendDouble(q.p1().x());
  res->AppendDouble(q.p1().y());
  res->AppendDouble(q.p2().x());
  res->AppendDouble(q.p2().y());
  res->AppendDouble(q.p3().x());
  res->AppendDouble(q.p3().y());
  res->AppendDouble(q.p4().x());
  res->AppendDouble(q.p4().y());
  res->EndArray();
}

void MathUtil::AddToTracedValue(const char* name,
                                const gfx::RectF& rect,
                                base::trace_event::TracedValue* res) {
  res->BeginArray(name);
  res->AppendDouble(rect.x());
  res->AppendDouble(rect.y());
  res->AppendDouble(rect.width());
  res->AppendDouble(rect.height());
  res->EndArray();
}

void MathUtil::AddToTracedValue(const char* name,
                                const gfx::Transform& transform,
                                base::trace_event::TracedValue* res) {
  res->BeginArray(name);
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col)
      res->AppendDouble(transform.rc(row, col));
  }
  res->EndArray();
}

void MathUtil::AddToTracedValue(const char* name,
                                const gfx::BoxF& box,
                                base::trace_event::TracedValue* res) {
  res->BeginArray(name);
  res->AppendInteger(box.x());
  res->AppendInteger(box.y());
  res->AppendInteger(box.z());
  res->AppendInteger(box.width());
  res->AppendInteger(box.height());
  res->AppendInteger(box.depth());
  res->EndArray();
}

void MathUtil::AddToTracedValue(const char* name,
                                const gfx::RRectF& rect,
                                base::trace_event::TracedValue* res) {
  res->BeginArray(name);
  res->AppendDouble(rect.rect().x());
  res->AppendDouble(rect.rect().y());
  res->AppendDouble(rect.rect().width());
  res->AppendDouble(rect.rect().height());
  res->AppendDouble(rect.GetCornerRadii(gfx::RRectF::Corner::kUpperLeft).x());
  res->AppendDouble(rect.GetCornerRadii(gfx::RRectF::Corner::kUpperLeft).y());
  res->AppendDouble(rect.GetCornerRadii(gfx::RRectF::Corner::kUpperRight).x());
  res->AppendDouble(rect.GetCornerRadii(gfx::RRectF::Corner::kUpperRight).y());
  res->AppendDouble(rect.GetCornerRadii(gfx::RRectF::Corner::kLowerRight).x());
  res->AppendDouble(rect.GetCornerRadii(gfx::RRectF::Corner::kLowerRight).y());
  res->AppendDouble(rect.GetCornerRadii(gfx::RRectF::Corner::kLowerLeft).x());
  res->AppendDouble(rect.GetCornerRadii(gfx::RRectF::Corner::kLowerLeft).y());
  res->EndArray();
}

void MathUtil::AddCornerRadiiToTracedValue(
    const char* name,
    const gfx::RRectF& rect,
    base::trace_event::TracedValue* res) {
  res->BeginArray(name);
  res->AppendDouble(rect.GetCornerRadii(gfx::RRectF::Corner::kUpperLeft).x());
  res->AppendDouble(rect.GetCornerRadii(gfx::RRectF::Corner::kUpperLeft).y());
  res->AppendDouble(rect.GetCornerRadii(gfx::RRectF::Corner::kUpperRight).x());
  res->AppendDouble(rect.GetCornerRadii(gfx::RRectF::Corner::kUpperRight).y());
  res->AppendDouble(rect.GetCornerRadii(gfx::RRectF::Corner::kLowerRight).x());
  res->AppendDouble(rect.GetCornerRadii(gfx::RRectF::Corner::kLowerRight).y());
  res->AppendDouble(rect.GetCornerRadii(gfx::RRectF::Corner::kLowerLeft).x());
  res->AppendDouble(rect.GetCornerRadii(gfx::RRectF::Corner::kLowerLeft).y());
  res->EndArray();
}

void MathUtil::AddToTracedValue(const char* name,
                                const gfx::LinearGradient& gradient,
                                base::trace_event::TracedValue* res) {
  res->BeginArray(name);
  res->AppendInteger(gradient.angle());
  res->AppendInteger(gradient.step_count());
  for (size_t i = 0; i < gradient.step_count(); i++) {
    res->AppendDouble(gradient.steps()[i].fraction);
    res->AppendInteger(gradient.steps()[i].alpha);
  }
  res->EndArray();
}

double MathUtil::AsDoubleSafely(double value) {
  return std::min(value, std::numeric_limits<double>::max());
}

float MathUtil::AsFloatSafely(float value) {
  return std::min(value, std::numeric_limits<float>::max());
}

gfx::Vector3dF MathUtil::GetXAxis(const gfx::Transform& transform) {
  if (transform.IsScaleOrTranslation()) {
    return gfx::Vector3dF(transform.To2dScale().x(), 0, 0);
  }

  return gfx::Vector3dF(transform.rc(0, 0), transform.rc(1, 0),
                        transform.rc(2, 0));
}

gfx::Vector3dF MathUtil::GetYAxis(const gfx::Transform& transform) {
  if (transform.IsScaleOrTranslation()) {
    return gfx::Vector3dF(0, transform.To2dScale().y(), 0);
  }
  return gfx::Vector3dF(transform.rc(0, 1), transform.rc(1, 1),
                        transform.rc(2, 1));
}

ScopedSubnormalFloatDisabler::ScopedSubnormalFloatDisabler() {
#if defined(ARCH_CPU_X86_FAMILY)
  // Turn on "subnormals are zero" and "flush to zero" CSR flags.
  orig_state_ = _mm_getcsr();
  _mm_setcsr(orig_state_ | 0x8040);
#endif
}

ScopedSubnormalFloatDisabler::~ScopedSubnormalFloatDisabler() {
#if defined(ARCH_CPU_X86_FAMILY)
  _mm_setcsr(orig_state_);
#endif
}

bool MathUtil::IsFloatNearlyTheSame(float left, float right) {
  return IsNearlyTheSame(left, right);
}

bool MathUtil::IsNearlyTheSameForTesting(const gfx::PointF& left,
                                         const gfx::PointF& right) {
  return IsNearlyTheSame(left, right);
}

bool MathUtil::IsNearlyTheSameForTesting(const gfx::Point3F& left,
                                         const gfx::Point3F& right) {
  return IsNearlyTheSame(left, right);
}

// Equivalent to SkMatrix::HasPerspective
bool MathUtil::SkM44HasPerspective(const SkM44& m) {
  return (m.rc(3, 0) != 0 || m.rc(3, 1) != 0 || m.rc(3, 2) != 0 ||
          m.rc(3, 3) != 1);
}

// Since some operations assume a 2d transformation, check to make sure that
// is the case by seeing that the z-axis is identity
bool MathUtil::SkM44Is2D(const SkM44& m) {
  return (m.rc(0, 2) == 0 && m.rc(1, 2) == 0 && m.rc(2, 2) == 1 &&
          m.rc(2, 0) == 0 && m.rc(2, 1) == 0 && m.rc(2, 3) == 0 &&
          m.rc(3, 2) == 0);
}

// Equivalent to SkMatrix::PreservesAxisAlignment
// Checks if the transformation is a 90 degree rotation or scaling
// See SkMatrix::computeTypeMask
bool MathUtil::SkM44Preserves2DAxisAlignment(const SkM44& m) {
  // Conservatively assume that perspective transforms would not preserve
  // axis-alignment
  if (!SkM44Is2D(m) || SkM44HasPerspective(m))
    return false;

  // Does the matrix have skew components
  if (m.rc(0, 1) != 0 || m.rc(1, 0) != 0) {
    // Rects only map to rects if both skews are non-zero and both scale
    // components are zero (i.e. it's a +/-90-degree rotation)
    return (m.rc(0, 0) == 0 && m.rc(1, 1) == 0 && m.rc(0, 1) != 0 &&
            m.rc(1, 0) != 0);
  }
  // Since the matrix has no skewing, it maps to a rectangle so long as the
  // scale components are non-zero
  return (m.rc(0, 0) != 0 && m.rc(1, 1) != 0);
}

}  // namespace cc
