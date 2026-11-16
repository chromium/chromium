// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojom/browser_controls_animation_params_mojom_traits.h"

#include <limits>
#include <utility>

#include "base/time/time.h"
#include "cc/input/browser_controls_animation_params.h"
#include "cc/mojom/browser_controls_animation_params.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/cubic_bezier.h"

namespace cc {
namespace {

BrowserControlsAnimationParams ValidParams() {
  return BrowserControlsAnimationParams{
      .target_top_controls_visible_height = 25.0f,
      .target_bottom_controls_visible_height = 10.0f,
      .duration = base::Milliseconds(200),
      .curve = gfx::CubicBezier(0.25, -0.5, 0.75, 1.5),
  };
}

bool RoundTrips(const BrowserControlsAnimationParams& in,
                BrowserControlsAnimationParams* out) {
  return mojo::test::SerializeAndDeserialize<
      mojom::BrowserControlsAnimationParams>(in, *out);
}

// The traits CHECK when they serialize an invalid value, so the tests for
// Read() start from the wire-format struct instead.
mojom::BrowserControlsAnimationParamsPtr ValidWireParams() {
  return mojom::BrowserControlsAnimationParams::New(
      /*target_top_controls_visible_height=*/25.0f,
      /*target_bottom_controls_visible_height=*/10.0f,
      /*duration=*/base::Milliseconds(200), /*curve_x1=*/0.25,
      /*curve_y1=*/-0.5, /*curve_x2=*/0.75, /*curve_y2=*/1.5);
}

bool Deserializes(mojom::BrowserControlsAnimationParamsPtr in) {
  BrowserControlsAnimationParams out;
  return mojo::test::SerializeAndDeserialize<
      mojom::BrowserControlsAnimationParams>(in, out);
}

TEST(BrowserControlsAnimationParamsMojomTraitsTest, RoundTrip) {
  const BrowserControlsAnimationParams in = ValidParams();
  BrowserControlsAnimationParams out;
  ASSERT_TRUE(RoundTrips(in, &out));
  EXPECT_EQ(in.target_top_controls_visible_height,
            out.target_top_controls_visible_height);
  EXPECT_EQ(in.target_bottom_controls_visible_height,
            out.target_bottom_controls_visible_height);
  EXPECT_EQ(in.duration, out.duration);
  EXPECT_DOUBLE_EQ(in.curve.GetX1(), out.curve.GetX1());
  EXPECT_DOUBLE_EQ(in.curve.GetY1(), out.curve.GetY1());
  EXPECT_DOUBLE_EQ(in.curve.GetX2(), out.curve.GetX2());
  EXPECT_DOUBLE_EQ(in.curve.GetY2(), out.curve.GetY2());
}

TEST(BrowserControlsAnimationParamsMojomTraitsTest,
     AcceptsControlPointOnCurveEndPoint) {
  // gfx::CubicBezier reads x2 back as 1.0000000000000002 for this curve.
  BrowserControlsAnimationParams in = ValidParams();
  in.curve = gfx::CubicBezier(0.075, 0.0, 1.0, 1.0);
  ASSERT_GT(in.curve.GetX2(), 1.0);

  BrowserControlsAnimationParams out;
  EXPECT_TRUE(RoundTrips(in, &out));
}

TEST(BrowserControlsAnimationParamsMojomTraitsTest, RejectsNonFiniteHeights) {
  for (float height : {std::numeric_limits<float>::quiet_NaN(),
                       std::numeric_limits<float>::infinity()}) {
    mojom::BrowserControlsAnimationParamsPtr in = ValidWireParams();
    in->target_top_controls_visible_height = height;
    EXPECT_FALSE(Deserializes(std::move(in))) << "top=" << height;

    in = ValidWireParams();
    in->target_bottom_controls_visible_height = height;
    EXPECT_FALSE(Deserializes(std::move(in))) << "bottom=" << height;
  }
}

TEST(BrowserControlsAnimationParamsMojomTraitsTest, RejectsNegativeDuration) {
  mojom::BrowserControlsAnimationParamsPtr in = ValidWireParams();
  in->duration = base::Milliseconds(-1);
  EXPECT_FALSE(Deserializes(std::move(in)));
}

TEST(BrowserControlsAnimationParamsMojomTraitsTest, RejectsInvalidCurve) {
  // The curve runs from (0, 0) to (1, 1). Each control point must lie between
  // those two points along the x axis; the y position has no range limit.
  constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
  constexpr double kInfinity = std::numeric_limits<double>::infinity();

  for (double x : {-0.1, 1.5, kNaN, kInfinity}) {
    mojom::BrowserControlsAnimationParamsPtr in = ValidWireParams();
    in->curve_x1 = x;
    EXPECT_FALSE(Deserializes(std::move(in))) << "x1=" << x;

    in = ValidWireParams();
    in->curve_x2 = x;
    EXPECT_FALSE(Deserializes(std::move(in))) << "x2=" << x;
  }

  for (double y : {kNaN, kInfinity, -kInfinity}) {
    mojom::BrowserControlsAnimationParamsPtr in = ValidWireParams();
    in->curve_y1 = y;
    EXPECT_FALSE(Deserializes(std::move(in))) << "y1=" << y;

    in = ValidWireParams();
    in->curve_y2 = y;
    EXPECT_FALSE(Deserializes(std::move(in))) << "y2=" << y;
  }
}

}  // namespace
}  // namespace cc
