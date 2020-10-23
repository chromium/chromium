// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/math_util.h"

#include <algorithm>
#include <cmath>
#include <limits>
#if defined(ARCH_CPU_X86_FAMILY)
#include <xmmintrin.h>
#endif

#include "base/numerics/ranges.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/rrect_f.h"
#include "ui/gfx/transform.h"

namespace cc {

static HomogeneousCoordinate ProjectHomogeneousPoint(
    const gfx::Transform& transform,
    const gfx::PointF& p) {
  SkScalar z =
      -(transform.matrix().get(2, 0) * p.x() +
        transform.matrix().get(2, 1) * p.y() + transform.matrix().get(2, 3)) /
      transform.matrix().get(2, 2);

  // In this case, the layer we are trying to project onto is perpendicular to
  // ray (point p and z-axis direction) that we are trying to project. This
  // happens when the layer is rotated so that it is infinitesimally thin, or
  // when it is co-planar with the camera origin -- i.e. when the layer is
  // invisible anyway.
  if (!std::isfinite(z))
    return HomogeneousCoordinate(0.0, 0.0, 0.0, 1.0);

  HomogeneousCoordinate result(p.x(), p.y(), z, 1.0);
  transform.matrix().mapScalars(result.vec, result.vec);
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
    const gfx::Point3F& p) {
  HomogeneousCoordinate result(p.x(), p.y(), p.z(), 1.0);
  transform.matrix().mapScalars(result.vec, result.vec);
  return result;
}

static void homogenousLimitAtZero(SkScalar a1,
                                  SkScalar w1,
                                  SkScalar a2,
                                  SkScalar w2,
                                  float t,
                                  float* limit) {
  // This is the tolerance for detecting an eyepoint-aligned edge.
  static const float kStationaryPointEplison = 0.00001f;

  if (std::abs(a1 * w2 / w1 / a2 - 1.0f) > kStationaryPointEplison) {
    // We are going to explode towards an infity, but we choose the one that
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

  float t = h1.w() / (h1.w() - h2.w());
  float x;
  float y;

  homogenousLimitAtZero(h1.x(), h1.w(), h2.x(), h2.w(), t, &x);
  homogenousLimitAtZero(h1.y(), h1.w(), h2.y(), h2.w(), t, &y);

  return gfx::PointF(x, y);
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

  float t = h1.w() / (h1.w() - h2.w());
  float x;
  float y;
  float z;

  homogenousLimitAtZero(h1.x(), h1.w(), h2.x(), h2.w(), t, &x);
  homogenousLimitAtZero(h1.y(), h1.w(), h2.y(), h2.w(), t, &y);
  homogenousLimitAtZero(h1.z(), h1.w(), h2.z(), h2.w(), t, &z);

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

static inline void AddVertexToClippedQuad3d(const gfx::Point3F& new_vertex,
                                            gfx::Point3F clipped_quad[6],
                                            int* num_vertices_in_clipped_quad) {
  if (*num_vertices_in_clipped_quad > 0 &&
      IsNearlyTheSame(clipped_quad[*num_vertices_in_clipped_quad - 1],
                      new_vertex))
    return;

  clipped_quad[*num_vertices_in_clipped_quad] = new_vertex;
  (*num_vertices_in_clipped_quad)++;
}

gfx::Rect MathUtil::MapEnclosingClippedRect(const gfx::Transform& transform,
                                            const gfx::Rect& src_rect) {
  return MapEnclosingClippedRectIgnoringError(transform, src_rect, 0.f);
}

gfx::Rect MathUtil::MapEnclosingClippedRectIgnoringError(
    const gfx::Transform& transform,
    const gfx::Rect& src_rect,
    float ignore_error) {
  if (transform.IsIdentityOrIntegerTranslation()) {
    gfx::Vector2d offset(static_cast<int>(transform.matrix().getFloat(0, 3)),
                         static_cast<int>(transform.matrix().getFloat(1, 3)));
    return src_rect + offset;
  }
  gfx::RectF mapped_rect = MapClippedRect(transform, gfx::RectF(src_rect));

  // gfx::ToEnclosingRect crashes if called on a RectF with any NaN coordinate.
  if (std::isnan(mapped_rect.x()) || std::isnan(mapped_rect.y()) ||
      std::isnan(mapped_rect.right()) || std::isnan(mapped_rect.bottom()))
    return gfx::Rect();

  return gfx::ToEnclosingRectIgnoringError(mapped_rect, ignore_error);
}

gfx::RectF MathUtil::MapClippedRect(const gfx::Transform& transform,
                                    const gfx::RectF& src_rect) {
  if (transform.IsIdentityOrTranslation()) {
    gfx::Vector2dF offset(transform.matrix().getFloat(0, 3),
                          transform.matrix().getFloat(1, 3));
    return src_rect + offset;
  }

  // Apply the transform, but retain the result in homogeneous coordinates.

  SkScalar quad[4 * 2];  // input: 4 x 2D points
  quad[0] = src_rect.x();
  quad[1] = src_rect.y();
  quad[2] = src_rect.right();
  quad[3] = src_rect.y();
  quad[4] = src_rect.right();
  quad[5] = src_rect.bottom();
  quad[6] = src_rect.x();
  quad[7] = src_rect.bottom();

  SkScalar result[4 * 4];  // output: 4 x 4D homogeneous points
  transform.matrix().map2(quad, 4, result);

  HomogeneousCoordinate hc0(result[0], result[1], result[2], result[3]);
  HomogeneousCoordinate hc1(result[4], result[5], result[6], result[7]);
  HomogeneousCoordinate hc2(result[8], result[9], result[10], result[11]);
  HomogeneousCoordinate hc3(result[12], result[13], result[14], result[15]);
  return ComputeEnclosingClippedRect(hc0, hc1, hc2, hc3);
}

gfx::Rect MathUtil::ProjectEnclosingClippedRect(const gfx::Transform& transform,
                                                const gfx::Rect& src_rect) {
  if (transform.IsIdentityOrIntegerTranslation()) {
    gfx::Vector2d offset(static_cast<int>(transform.matrix().getFloat(0, 3)),
                         static_cast<int>(transform.matrix().getFloat(1, 3)));
    return src_rect + offset;
  }
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
  if (transform.IsIdentityOrTranslation()) {
    gfx::Vector2dF offset(transform.matrix().getFloat(0, 3),
                          transform.matrix().getFloat(1, 3));
    return src_rect + offset;
  }

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
  gfx::Transform inverse_device_transform(gfx::Transform::kSkipInitialization);
  DCHECK(device_transform.IsInvertible());
  bool did_invert = device_transform.GetInverse(&inverse_device_transform);
  DCHECK(did_invert);
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

  if (transform.IsIdentityOrIntegerTranslation()) {
    gfx::Vector2d offset(static_cast<int>(transform.matrix().getFloat(0, 3)),
                         static_cast<int>(transform.matrix().getFloat(1, 3)));
    return rect + offset;
  }
  if (transform.IsIdentityOrTranslation()) {
    gfx::Vector2dF offset(transform.matrix().getFloat(0, 3),
                          transform.matrix().getFloat(1, 3));
    return gfx::ToEnclosedRect(gfx::RectF(rect) + offset);
  }

  SkScalar quad[2 * 2];  // input: 2 x 2D points
  quad[0] = rect.x();
  quad[1] = rect.y();
  quad[2] = rect.right();
  quad[3] = rect.bottom();

  SkScalar result[4 * 2];  // output: 2 x 4D homogeneous points
  transform.matrix().map2(quad, 2, result);

  HomogeneousCoordinate hc0(result[0], result[1], result[2], result[3]);
  HomogeneousCoordinate hc1(result[4], result[5], result[6], result[7]);
  DCHECK(!hc0.ShouldBeClipped());
  DCHECK(!hc1.ShouldBeClipped());

  gfx::PointF top_left(hc0.CartesianPoint2d());
  gfx::PointF bottom_right(hc1.CartesianPoint2d());
  return gfx::ToEnclosedRect(gfx::BoundingRect(top_left, bottom_right));
}

bool MathUtil::MapClippedQuad3d(const gfx::Transform& transform,
                                const gfx::QuadF& src_quad,
                                gfx::Point3F clipped_quad[6],
                                int* num_vertices_in_clipped_quad) {
  HomogeneousCoordinate h1 =
      MapHomogeneousPoint(transform, gfx::Point3F(src_quad.p1()));
  HomogeneousCoordinate h2 =
      MapHomogeneousPoint(transform, gfx::Point3F(src_quad.p2()));
  HomogeneousCoordinate h3 =
      MapHomogeneousPoint(transform, gfx::Point3F(src_quad.p3()));
  HomogeneousCoordinate h4 =
      MapHomogeneousPoint(transform, gfx::Point3F(src_quad.p4()));

  // The order of adding the vertices to the array is chosen so that
  // clockwise / counter-clockwise orientation is retained.

  *num_vertices_in_clipped_quad = 0;

  if (!h1.ShouldBeClipped()) {
    AddVertexToClippedQuad3d(
        h1.CartesianPoint3d(), clipped_quad, num_vertices_in_clipped_quad);
  }

  if (h1.ShouldBeClipped() ^ h2.ShouldBeClipped()) {
    AddVertexToClippedQuad3d(ComputeClippedCartesianPoint3dForEdge(h1, h2),
                             clipped_quad, num_vertices_in_clipped_quad);
  }

  if (!h2.ShouldBeClipped()) {
    AddVertexToClippedQuad3d(
        h2.CartesianPoint3d(), clipped_quad, num_vertices_in_clipped_quad);
  }

  if (h2.ShouldBeClipped() ^ h3.ShouldBeClipped()) {
    AddVertexToClippedQuad3d(ComputeClippedCartesianPoint3dForEdge(h2, h3),
                             clipped_quad, num_vertices_in_clipped_quad);
  }

  if (!h3.ShouldBeClipped()) {
    AddVertexToClippedQuad3d(
        h3.CartesianPoint3d(), clipped_quad, num_vertices_in_clipped_quad);
  }

  if (h3.ShouldBeClipped() ^ h4.ShouldBeClipped()) {
    AddVertexToClippedQuad3d(ComputeClippedCartesianPoint3dForEdge(h3, h4),
                             clipped_quad, num_vertices_in_clipped_quad);
  }

  if (!h4.ShouldBeClipped()) {
    AddVertexToClippedQuad3d(
        h4.CartesianPoint3d(), clipped_quad, num_vertices_in_clipped_quad);
  }

  if (h4.ShouldBeClipped() ^ h1.ShouldBeClipped()) {
    AddVertexToClippedQuad3d(ComputeClippedCartesianPoint3dForEdge(h4, h1),
                             clipped_quad, num_vertices_in_clipped_quad);
  }

  if (*num_vertices_in_clipped_quad > 2 &&
      IsNearlyTheSame(clipped_quad[0],
                      clipped_quad[*num_vertices_in_clipped_quad - 1]))
    *num_vertices_in_clipped_quad -= 1;

  DCHECK_LE(*num_vertices_in_clipped_quad, 6);
  return (*num_vertices_in_clipped_quad >= 4);
}

gfx::RectF MathUtil::ComputeEnclosingRectOfVertices(
    const gfx::PointF vertices[],
    int num_vertices) {
  if (num_vertices < 2)
    return gfx::RectF();

  float xmin = std::numeric_limits<float>::max();
  float xmax = -std::numeric_limits<float>::max();
  float ymin = std::numeric_limits<float>::max();
  float ymax = -std::numeric_limits<float>::max();

  for (int i = 0; i < num_vertices; ++i)
    ExpandBoundsToIncludePoint(&xmin, &xmax, &ymin, &ymax, vertices[i]);

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
    mapped_quad += gfx::Vector2dF(transform.matrix().getFloat(0, 3),
                                  transform.matrix().getFloat(1, 3));
    *clipped = false;
    return mapped_quad;
  }

  HomogeneousCoordinate h1 =
      MapHomogeneousPoint(transform, gfx::Point3F(q.p1()));
  HomogeneousCoordinate h2 =
      MapHomogeneousPoint(transform, gfx::Point3F(q.p2()));
  HomogeneousCoordinate h3 =
      MapHomogeneousPoint(transform, gfx::Point3F(q.p3()));
  HomogeneousCoordinate h4 =
      MapHomogeneousPoint(transform, gfx::Point3F(q.p4()));

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
  HomogeneousCoordinate h = MapHomogeneousPoint(transform, gfx::Point3F(p));

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
  output_inner_rect.Inset(top_left_diff.x() / scale_rect_to_input_scale_x,
                          top_left_diff.y() / scale_rect_to_input_scale_y,
                          -bottom_right_diff.x() / scale_rect_to_input_scale_x,
                          -bottom_right_diff.y() / scale_rect_to_input_scale_y);
  return output_inner_rect;
}

static inline bool NearlyZero(double value) {
  return std::abs(value) < std::numeric_limits<double>::epsilon();
}

static inline float ScaleOnAxis(double a, double b, double c) {
  if (NearlyZero(b) && NearlyZero(c))
    return std::abs(a);
  if (NearlyZero(a) && NearlyZero(c))
    return std::abs(b);
  if (NearlyZero(a) && NearlyZero(b))
    return std::abs(c);

  // Do the sqrt as a double to not lose precision.
  return static_cast<float>(std::sqrt(a * a + b * b + c * c));
}

gfx::Vector2dF MathUtil::ComputeTransform2dScaleComponents(
    const gfx::Transform& transform,
    float fallback_value) {
  if (transform.HasPerspective())
    return gfx::Vector2dF(fallback_value, fallback_value);
  float x_scale = ScaleOnAxis(transform.matrix().getDouble(0, 0),
                              transform.matrix().getDouble(1, 0),
                              transform.matrix().getDouble(2, 0));
  float y_scale = ScaleOnAxis(transform.matrix().getDouble(0, 1),
                              transform.matrix().getDouble(1, 1),
                              transform.matrix().getDouble(2, 1));
  return gfx::Vector2dF(x_scale, y_scale);
}

float MathUtil::ComputeApproximateMaxScale(const gfx::Transform& transform) {
  gfx::RectF unit(0.f, 0.f, 1.f, 1.f);
  transform.TransformRect(&unit);
  return std::max(unit.width(), unit.height());
}

float MathUtil::SmallestAngleBetweenVectors(const gfx::Vector2dF& v1,
                                            const gfx::Vector2dF& v2) {
  double dot_product = gfx::DotProduct(v1, v2) / v1.Length() / v2.Length();
  // Clamp to compensate for rounding errors.
  dot_product = base::ClampToRange(dot_product, -1.0, 1.0);
  return static_cast<float>(gfx::RadToDeg(std::acos(dot_product)));
}

gfx::Vector2dF MathUtil::ProjectVector(const gfx::Vector2dF& source,
                                       const gfx::Vector2dF& destination) {
  float projected_length =
      gfx::DotProduct(source, destination) / destination.LengthSquared();
  return gfx::Vector2dF(projected_length * destination.x(),
                        projected_length * destination.y());
}

bool MathUtil::FromValue(const base::Value* raw_value, gfx::Rect* out_rect) {
  const base::ListValue* value = nullptr;
  if (!raw_value->GetAsList(&value))
    return false;

  if (value->GetSize() != 4)
    return false;

  int x, y, w, h;
  bool ok = true;
  ok &= value->GetInteger(0, &x);
  ok &= value->GetInteger(1, &y);
  ok &= value->GetInteger(2, &w);
  ok &= value->GetInteger(3, &h);
  if (!ok)
    return false;

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
                                const gfx::ScrollOffset& v,
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
  const SkMatrix44& m = transform.matrix();
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col)
      res->AppendDouble(m.getDouble(row, col));
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

double MathUtil::AsDoubleSafely(double value) {
  return std::min(value, std::numeric_limits<double>::max());
}

float MathUtil::AsFloatSafely(float value) {
  return std::min(value, std::numeric_limits<float>::max());
}

gfx::Vector3dF MathUtil::GetXAxis(const gfx::Transform& transform) {
  return gfx::Vector3dF(transform.matrix().getFloat(0, 0),
                        transform.matrix().getFloat(1, 0),
                        transform.matrix().getFloat(2, 0));
}

gfx::Vector3dF MathUtil::GetYAxis(const gfx::Transform& transform) {
  return gfx::Vector3dF(transform.matrix().getFloat(0, 1),
                        transform.matrix().getFloat(1, 1),
                        transform.matrix().getFloat(2, 1));
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

}  // namespace cc
