// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/snap_fling_curve.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace test {

TEST(SnapFlingCurveTest, CurveInitialization) {
  SnapFlingCurve active_curve(gfx::PointF(100, 100), gfx::PointF(500, 500),
                              base::TimeTicks());
  EXPECT_FALSE(active_curve.IsFinished());

  SnapFlingCurve finished_curve(gfx::PointF(100, 100), gfx::PointF(100, 100),
                                base::TimeTicks());
  EXPECT_TRUE(finished_curve.IsFinished());
}

TEST(SnapFlingCurveTest, AdvanceHalfwayThrough) {
  SnapFlingCurve curve(gfx::PointF(100, 100), gfx::PointF(500, 500),
                       base::TimeTicks());
  base::TimeDelta duration = curve.duration();
  gfx::Vector2dF delta1 =
      curve.GetScrollDelta(base::TimeTicks() + duration / 2);
  EXPECT_LT(0, delta1.x());
  EXPECT_LT(0, delta1.y());
  EXPECT_FALSE(curve.IsFinished());

  // Repeated offset computations at the same timestamp before applying the
  // scrolled delta should yield identical results.
  gfx::Vector2dF delta2 =
      curve.GetScrollDelta(base::TimeTicks() + duration / 2);
  EXPECT_EQ(delta1, delta2);
  EXPECT_FALSE(curve.IsFinished());

  curve.UpdateCurrentOffset(gfx::PointF(100, 100) + delta1);
  EXPECT_FALSE(curve.IsFinished());
}

TEST(SnapFlingCurveTest, AdvanceFullyThrough) {
  SnapFlingCurve curve(gfx::PointF(100, 100), gfx::PointF(500, 500),
                       base::TimeTicks());
  gfx::Vector2dF delta =
      curve.GetScrollDelta(base::TimeTicks() + curve.duration());
  EXPECT_EQ(gfx::Vector2dF(400, 400), delta);
  EXPECT_TRUE(curve.IsFinished());
}

TEST(SnapFlingCurveTest, ReturnsZeroAfterFinished) {
  SnapFlingCurve curve(gfx::PointF(100, 100), gfx::PointF(500, 500),
                       base::TimeTicks());
  curve.UpdateCurrentOffset(gfx::PointF(500, 500));
  gfx::Vector2dF delta = curve.GetScrollDelta(base::TimeTicks());
  EXPECT_EQ(gfx::Vector2dF(), delta);
  EXPECT_TRUE(curve.IsFinished());

  delta = curve.GetScrollDelta(base::TimeTicks() + curve.duration());
  EXPECT_EQ(gfx::Vector2dF(), delta);
  EXPECT_TRUE(curve.IsFinished());
}

TEST(SnapFlingCurveTest, FlingFinishesWithinOnePixel) {
  SnapFlingCurve curve(gfx::PointF(0, 0), gfx::PointF(100.5, 99.5),
                       base::TimeTicks());
  EXPECT_FALSE(curve.IsFinished());

  curve.UpdateCurrentOffset(gfx::PointF(99, 101));
  // IsFinished() is updated in GetScrollDelta().
  curve.GetScrollDelta(base::TimeTicks());
  EXPECT_FALSE(curve.IsFinished());

  curve.UpdateCurrentOffset(gfx::PointF(100, 100));
  curve.GetScrollDelta(base::TimeTicks());
  EXPECT_TRUE(curve.IsFinished());
}

TEST(SnapFlingCurveTest, EstimateDisplacementFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kSnapFlingDecayPrediction);

  gfx::Vector2dF current_delta(0, -10);
  std::optional<gfx::Vector2dF> previous_delta;

  // Should use constant factor prediction, ignoring previous_delta.
  std::optional<gfx::Vector2dF> displacement =
      SnapFlingCurve::EstimateDisplacement(current_delta, previous_delta,
                                           /*allow_slow_decay=*/false);
  EXPECT_TRUE(displacement.has_value());
  // Default scalar is 25 on non-Android, 40 on Android.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(gfx::Vector2dF(0, -400), *displacement);
#else
  EXPECT_EQ(gfx::Vector2dF(0, -250), *displacement);
#endif

  previous_delta = gfx::Vector2dF(0, -20);
  displacement =
      SnapFlingCurve::EstimateDisplacement(current_delta, previous_delta,
                                           /*allow_slow_decay=*/false);
  EXPECT_TRUE(displacement.has_value());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(gfx::Vector2dF(0, -400), *displacement);
#else
  EXPECT_EQ(gfx::Vector2dF(0, -250), *displacement);
#endif
}

TEST(SnapFlingCurveTest, EstimateDisplacementFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSnapFlingDecayPrediction);

  gfx::Vector2dF current_delta(0, -8);
  std::optional<gfx::Vector2dF> previous_delta;

  // No previous delta -> unknown.
  std::optional<gfx::Vector2dF> displacement =
      SnapFlingCurve::EstimateDisplacement(current_delta, previous_delta,
                                           /*allow_slow_decay=*/false);
  EXPECT_FALSE(displacement.has_value());

  // Previous delta exists but not decaying (current >= prev) -> unknown.
  previous_delta = gfx::Vector2dF(0, -8);
  displacement =
      SnapFlingCurve::EstimateDisplacement(current_delta, previous_delta,
                                           /*allow_slow_decay=*/false);
  EXPECT_FALSE(displacement.has_value());

  previous_delta = gfx::Vector2dF(0, -6);  // current (8) > prev (6)
  displacement =
      SnapFlingCurve::EstimateDisplacement(current_delta, previous_delta,
                                           /*allow_slow_decay=*/false);
  EXPECT_FALSE(displacement.has_value());

  // Decaying (current < prev).
  previous_delta = gfx::Vector2dF(0, -10);  // 8 < 10
  displacement =
      SnapFlingCurve::EstimateDisplacement(current_delta, previous_delta,
                                           /*allow_slow_decay=*/false);
  EXPECT_TRUE(displacement.has_value());
  // Decay = 8/10 = 0.8. Est = 8 / (1 - 0.8) = 40.
  EXPECT_FLOAT_EQ(0.f, displacement->x());
  EXPECT_FLOAT_EQ(-40.f, displacement->y());

  // Decaying but decay factor is too large (current / prev >= 0.96).
  previous_delta = gfx::Vector2dF(0, -8.1f);  // 8 / 8.1 = 0.987 >= 0.96
  displacement =
      SnapFlingCurve::EstimateDisplacement(current_delta, previous_delta,
                                           /*allow_slow_decay=*/false);
  EXPECT_FALSE(displacement.has_value());
}

}  // namespace test
}  // namespace cc
