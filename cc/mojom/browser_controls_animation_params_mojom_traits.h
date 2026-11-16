// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_BROWSER_CONTROLS_ANIMATION_PARAMS_MOJOM_TRAITS_H_
#define CC_MOJOM_BROWSER_CONTROLS_ANIMATION_PARAMS_MOJOM_TRAITS_H_

#include <algorithm>
#include <cmath>

#include "base/check.h"
#include "base/numerics/ranges.h"
#include "base/time/time.h"
#include "cc/input/browser_controls_animation_params.h"
#include "cc/mojom/browser_controls_animation_params.mojom-shared.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/geometry/cubic_bezier.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::BrowserControlsAnimationParamsDataView,
                    cc::BrowserControlsAnimationParams> {
  // The getters CHECK the same conditions that Read() rejects. The sender is
  // the browser process, so a value that fails Read() is a browser bug.
  // Without the CHECKs, that bug closes the input message pipe in the
  // renderer and leaves no crash report.
  static float target_top_controls_visible_height(
      const cc::BrowserControlsAnimationParams& params) {
    CHECK(std::isfinite(params.target_top_controls_visible_height));
    return params.target_top_controls_visible_height;
  }
  static float target_bottom_controls_visible_height(
      const cc::BrowserControlsAnimationParams& params) {
    CHECK(std::isfinite(params.target_bottom_controls_visible_height));
    return params.target_bottom_controls_visible_height;
  }
  static base::TimeDelta duration(
      const cc::BrowserControlsAnimationParams& params) {
    CHECK(!params.duration.is_negative());
    return params.duration;
  }
  static double curve_x1(const cc::BrowserControlsAnimationParams& params) {
    return ValidatedControlPointX(params.curve.GetX1());
  }
  static double curve_y1(const cc::BrowserControlsAnimationParams& params) {
    CHECK(std::isfinite(params.curve.GetY1()));
    return params.curve.GetY1();
  }
  static double curve_x2(const cc::BrowserControlsAnimationParams& params) {
    return ValidatedControlPointX(params.curve.GetX2());
  }
  static double curve_y2(const cc::BrowserControlsAnimationParams& params) {
    CHECK(std::isfinite(params.curve.GetY2()));
    return params.curve.GetY2();
  }

  // gfx::CubicBezier stores polynomial coefficients, not control points, so
  // GetX1() and GetX2() can return a value that is a rounding error outside
  // [0, 1] for a valid curve (for example 1.0000000000000002 for x2 = 1).
  // Read() rejects such a value, so clamp it here. A value that is further
  // outside [0, 1] than the precision of the curve solver is a real error.
  static double ValidatedControlPointX(double x) {
    const double clamped = std::clamp(x, 0.0, 1.0);
    CHECK(base::IsApproximatelyEqual(x, clamped,
                                     gfx::CubicBezier::GetDefaultEpsilon()));
    return clamped;
  }

  static bool Read(cc::mojom::BrowserControlsAnimationParamsDataView data,
                   cc::BrowserControlsAnimationParams* out) {
    if (!std::isfinite(data.target_top_controls_visible_height()) ||
        !std::isfinite(data.target_bottom_controls_visible_height())) {
      return false;
    }
    // With a control point x outside [0, 1] the curve can double back along
    // the x axis, so one progress value maps to several outputs and the curve
    // cannot be solved deterministically.
    if (!(data.curve_x1() >= 0.0 && data.curve_x1() <= 1.0) ||
        !(data.curve_x2() >= 0.0 && data.curve_x2() <= 1.0) ||
        !std::isfinite(data.curve_y1()) || !std::isfinite(data.curve_y2())) {
      return false;
    }
    if (!data.ReadDuration(&out->duration) || out->duration.is_negative()) {
      return false;
    }
    out->target_top_controls_visible_height =
        data.target_top_controls_visible_height();
    out->target_bottom_controls_visible_height =
        data.target_bottom_controls_visible_height();
    out->curve = gfx::CubicBezier(data.curve_x1(), data.curve_y1(),
                                  data.curve_x2(), data.curve_y2());
    return true;
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_BROWSER_CONTROLS_ANIMATION_PARAMS_MOJOM_TRAITS_H_
