// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_MATH_UTIL_H_
#define CC_BASE_MATH_UTIL_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "base/check.h"
#include "base/containers/span.h"
#include "build/build_config.h"
#include "cc/base/base_export.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"

namespace base {
class Value;
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace gfx {
class QuadF;
class Rect;
class RectF;
class RRectF;
class Size;
class SizeF;
class Transform;
class Vector2dF;
class Vector2d;
class Vector3dF;
class LinearGradient;
}  // namespace gfx

namespace cc {

struct HomogeneousCoordinate {
  // This needs to be big enough that it does not incorrectly clip the projected
  // coordinate. For local to device projection, this must be bigger than the
  // expected size of a display. For inverse projection, this is hopefully
  // larger than the required local layer size of page content. If it is made
  // too big then bounding box calculations based on projected coordinates can
  // lose precision and lead to incorrect page rendering.
  static constexpr float kInfiniteCoordinate = 1000000.0f;

  HomogeneousCoordinate(SkScalar x, SkScalar y, SkScalar z, SkScalar w) {
    vec[0] = x;
    vec[1] = y;
    vec[2] = z;
    vec[3] = w;
  }

  bool ShouldBeClipped() const { return w() <= 0.0; }

  gfx::PointF CartesianPoint2d() const {
    if (w() == SK_Scalar1)
      return gfx::PointF(x(), y());

    // For now, because this code is used privately only by MathUtil, it should
    // never be called when w == 0, and we do not yet need to handle that case.
    DCHECK(w());
    SkScalar inv_w = SK_Scalar1 / w();
    // However, w may be close to 0 and we lose precision on our geometry
    // calculations if we allow scaling to extremely large values.
    return gfx::PointF(std::clamp(x() * inv_w, -kInfiniteCoordinate,
                                  float{kInfiniteCoordinate}),
                       std::clamp(y() * inv_w, -kInfiniteCoordinate,
                                  float{kInfiniteCoordinate}));
  }

  gfx::Point3F CartesianPoint3d() const {
    if (w() == SK_Scalar1)
      return gfx::Point3F(x(), y(), z());

    // For now, because this code is used privately only by MathUtil, it should
    // never be called when w == 0, and we do not yet need to handle that case.
    DCHECK(w());
    SkScalar inv_w = SK_Scalar1 / w();
    // However, w may be close to 0 and we lose precision on our geometry
    // calculations if we allow scaling to extremely large values.
    return gfx::Point3F(std::clamp(x() * inv_w, -kInfiniteCoordinate,
                                   float{kInfiniteCoordinate}),
                        std::clamp(y() * inv_w, -kInfiniteCoordinate,
                                   float{kInfiniteCoordinate}),
                        std::clamp(z() * inv_w, -kInfiniteCoordinate,
                                   float{kInfiniteCoordinate}));
  }

  gfx::Point3F CartesianPoint3dUnclamped() const {
    if (w() == SK_Scalar1)
      return gfx::Point3F(x(), y(), z());

    // For now, because this code is used privately only by MathUtil, it should
    // never be called when w == 0, and we do not yet need to handle that case.
    DCHECK(w());
    SkScalar inv_w = SK_Scalar1 / w();
    // However, w may be close to 0 and we lose precision on our geometry
    // calculations if we allow scaling to extremely large values.
    return gfx::Point3F(x() * inv_w, y() * inv_w, z() * inv_w);
  }

  SkScalar x() const { return vec[0]; }
  SkScalar y() const { return vec[1]; }
  SkScalar z() const { return vec[2]; }
  SkScalar w() const { return vec[3]; }

  std::array<SkScalar, 4> vec;
};

class CC_BASE_EXPORT MathUtil {
 public:
  // Returns true if rounded up value does not overflow, false otherwise.
  template <typename T>
    requires std::integral<T>
  static constexpr bool VerifyRoundup(T n, T mul) {
    return mul && (n <= (std::numeric_limits<T>::max() -
                         (std::numeric_limits<T>::max() % mul)));
  }

  // Rounds up a given |n| to be a multiple of |mul|, but may overflow.
  // Examples:
  //    - RoundUp(123, 50) returns 150.
  //    - RoundUp(-123, 50) returns -100.
  template <typename T>
    requires std::integral<T>
  static constexpr T UncheckedRoundUp(T n, T mul) {
    return RoundUpInternal(n, mul);
  }

  // Similar to UncheckedRoundUp(), but dies with a CRASH() if rounding up a
  // given |n| overflows T.
  template <typename T>
    requires std::integral<T>
  static constexpr T CheckedRoundUp(T n, T mul) {
    CHECK(VerifyRoundup(n, mul));
    return RoundUpInternal(n, mul);
  }

  // Returns true if rounded down value does not underflow, false otherwise.
  template <typename T>
    requires std::integral<T>
  static constexpr bool VerifyRoundDown(T n, T mul) {
    return mul && (n >= (std::numeric_limits<T>::min() -
                         (std::numeric_limits<T>::min() % mul)));
  }

  // Rounds down a given |n| to be a multiple of |mul|, but may underflow.
  // Examples:
  //    - RoundDown(123, 50) returns 100.
  //    - RoundDown(-123, 50) returns -150.
  template <typename T>
    requires std::integral<T>
  static constexpr T UncheckedRoundDown(T n, T mul) {
    return RoundDownInternal(n, mul);
  }

  // Similar to UncheckedRoundDown(), but dies with a CRASH() if rounding down a
  // given |n| underflows T.
  template <typename T>
    requires std::integral<T>
  static constexpr T CheckedRoundDown(T n, T mul) {
    CHECK(VerifyRoundDown(n, mul));
    return RoundDownInternal(n, mul);
  }

  template <typename T>
  static constexpr bool IsWithinEpsilon(T a, T b) {
    return std::abs(a - b) < std::numeric_limits<T>::epsilon();
  }

  // Background: Existing transform code does not do the right thing in
  // MapRect / MapQuad / ProjectQuad when there is a perspective projection that
  // causes one of the transformed vertices to go to w < 0. In those cases, it
  // is necessary to perform clipping in homogeneous coordinates, after applying
  // the transform, before dividing-by-w to convert to cartesian coordinates.
  //
  // These functions return the axis-aligned rect that encloses the correctly
  // clipped, transformed polygon.
  static gfx::Rect MapEnclosingClippedRect(const gfx::Transform& transform,
                                           const gfx::Rect& rect);
  static gfx::Rect MapEnclosingClippedRectIgnoringError(
      const gfx::Transform& transform,
      const gfx::Rect& rect,
      float ignore_error);
  static gfx::RectF MapClippedRect(const gfx::Transform& transform,
                                   const gfx::RectF& rect);
  static gfx::Rect ProjectEnclosingClippedRect(const gfx::Transform& transform,
                                               const gfx::Rect& rect);
  static gfx::RectF ProjectClippedRect(const gfx::Transform& transform,
                                       const gfx::RectF& rect);

  // Map device space quad to local space. Device_transform has no 3d
  // component since it was flattened, so we don't need to project.  We should
  // have already checked that the transform was invertible before this call.
  static gfx::QuadF InverseMapQuadToLocalSpace(
      const gfx::Transform& device_transform,
      const gfx::QuadF& device_quad);

  // This function is only valid when the transform preserves 2d axis
  // alignment and the resulting rect will not be clipped.
  static gfx::Rect MapEnclosedRectWith2dAxisAlignedTransform(
      const gfx::Transform& transform,
      const gfx::Rect& rect);

  // Returns an array of vertices that represent the clipped polygon. After
  // returning, indexes from 0 to num_vertices_in_clipped_quad are valid in the
  // clipped_quad array. Note that num_vertices_in_clipped_quad may be zero,
  // which means the entire quad was clipped, and none of the vertices in the
  // array are valid.
  static bool MapClippedQuad3d(const gfx::Transform& transform,
                               const gfx::QuadF& src_quad,
                               base::span<gfx::Point3F, 6> clipped_quad,
                               int* num_vertices_in_clipped_quad);

  static gfx::RectF ComputeEnclosingRectOfVertices(
      base::span<const gfx::PointF> vertices);
  static gfx::RectF ComputeEnclosingClippedRect(
      const HomogeneousCoordinate& h1,
      const HomogeneousCoordinate& h2,
      const HomogeneousCoordinate& h3,
      const HomogeneousCoordinate& h4);

  // NOTE: These functions do not do correct clipping against w = 0 plane, but
  // they correctly detect the clipped condition via the boolean clipped.
  static gfx::QuadF MapQuad(const gfx::Transform& transform,
                            const gfx::QuadF& quad,
                            bool* clipped);
  static gfx::PointF MapPoint(const gfx::Transform& transform,
                              const gfx::PointF& point,
                              bool* clipped);
  static gfx::PointF ProjectPoint(const gfx::Transform& transform,
                                  const gfx::PointF& point,
                                  bool* clipped);
  // Identical to the above function, but coerces the homogeneous coordinate to
  // a 3d rather than a 2d point.
  static gfx::Point3F ProjectPoint3D(const gfx::Transform& transform,
                                     const gfx::PointF& point,
                                     bool* clipped);

  // Makes a rect that has the same relationship to input_outer_rect as
  // scale_inner_rect has to scale_outer_rect. scale_inner_rect should be
  // contained within scale_outer_rect, and likewise the rectangle that is
  // returned will be within input_outer_rect at a similar relative, scaled
  // position.
  static gfx::RectF ScaleRectProportional(const gfx::RectF& input_outer_rect,
                                          const gfx::RectF& scale_outer_rect,
                                          const gfx::RectF& scale_inner_rect);

  // Returns the smallest angle between the given two vectors in degrees.
  // Neither vector is assumed to be normalized.
  static float SmallestAngleBetweenVectors(const gfx::Vector2dF& v1,
                                           const gfx::Vector2dF& v2);

  // Projects the |source| vector onto |destination|. Neither vector is assumed
  // to be normalized.
  static gfx::Vector2dF ProjectVector(const gfx::Vector2dF& source,
                                      const gfx::Vector2dF& destination);

  static bool FromValue(const base::Value*, gfx::Rect* out_rect);

  static void AddToTracedValue(const char* name,
                               const gfx::Size& s,
                               base::trace_event::TracedValue* res);
  static void AddToTracedValue(const char* name,
                               const gfx::SizeF& s,
                               base::trace_event::TracedValue* res);
  static void AddToTracedValue(const char* name,
                               const gfx::Rect& r,
                               base::trace_event::TracedValue* res);
  static void AddToTracedValue(const char* name,
                               const gfx::Point& q,
                               base::trace_event::TracedValue* res);
  static void AddToTracedValue(const char* name,
                               const gfx::PointF& q,
                               base::trace_event::TracedValue* res);
  static void AddToTracedValue(const char* name,
                               const gfx::Point3F&,
                               base::trace_event::TracedValue* res);
  static void AddToTracedValue(const char* name,
                               const gfx::Vector2d& v,
                               base::trace_event::TracedValue* res);
  static void AddToTracedValue(const char* name,
                               const gfx::Vector2dF& v,
                               base::trace_event::TracedValue* res);
  static void AddToTracedValue(const char* name,
                               const gfx::QuadF& q,
                               base::trace_event::TracedValue* res);
  static void AddToTracedValue(const char* name,
                               const gfx::RectF& rect,
                               base::trace_event::TracedValue* res);
  static void AddToTracedValue(const char* name,
                               const gfx::Transform& transform,
                               base::trace_event::TracedValue* res);
  static void AddToTracedValue(const char* name,
                               const gfx::BoxF& box,
                               base::trace_event::TracedValue* res);
  static void AddToTracedValue(const char* name,
                               const gfx::RRectF& rect,
                               base::trace_event::TracedValue* res);
  static void AddCornerRadiiToTracedValue(const char* name,
                                          const gfx::RRectF& rect,
                                          base::trace_event::TracedValue* res);
  static void AddToTracedValue(const char* name,
                               const gfx::LinearGradient& gradient,
                               base::trace_event::TracedValue* res);

  // Returns a base::Value representation of the floating point value.
  // If the value is inf, returns max double/float representation.
  static double AsDoubleSafely(double value);
  static float AsFloatSafely(float value);

  // Returns vector that x axis (1,0,0) transforms to under given transform.
  static gfx::Vector3dF GetXAxis(const gfx::Transform& transform);

  // Returns vector that y axis (0,1,0) transforms to under given transform.
  static gfx::Vector3dF GetYAxis(const gfx::Transform& transform);

  static bool IsFloatNearlyTheSame(float left, float right);
  static bool IsNearlyTheSameForTesting(const gfx::PointF& l,
                                        const gfx::PointF& r);
  static bool IsNearlyTheSameForTesting(const gfx::Point3F& l,
                                        const gfx::Point3F& r);

  // Helper functions for migration from SkMatrix->SkM44. It may make sense to
  // move these to skia itself at some point.
  static bool SkM44HasPerspective(const SkM44& m);
  static bool SkM44Is2D(const SkM44& m);
  static bool SkM44Preserves2DAxisAlignment(const SkM44& m);

 private:
  template <typename T>
  static constexpr T RoundUpInternal(T n, T mul) {
    T remainder = n % mul;
    if (remainder == 0) {
      return n;
    }
    return (n > 0) ? n + mul - remainder : n - remainder;
  }

  template <typename T>
  static constexpr T RoundDownInternal(T n, T mul) {
    T remainder = n % mul;
    if (remainder == 0) {
      return n;
    }
    return (n > 0) ? n - remainder : n - mul - remainder;
  }
};

class CC_BASE_EXPORT ScopedSubnormalFloatDisabler {
 public:
  ScopedSubnormalFloatDisabler();
  ScopedSubnormalFloatDisabler(const ScopedSubnormalFloatDisabler&) = delete;
  ~ScopedSubnormalFloatDisabler();

  ScopedSubnormalFloatDisabler& operator=(const ScopedSubnormalFloatDisabler&) =
      delete;

#if defined(ARCH_CPU_X86_FAMILY)
 private:
  unsigned int orig_state_;
#endif
};

}  // namespace cc

#endif  // CC_BASE_MATH_UTIL_H_
