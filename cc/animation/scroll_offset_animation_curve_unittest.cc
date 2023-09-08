// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_offset_animation_curve.h"

#include "base/time/time.h"
#include "cc/animation/scroll_offset_animation_curve_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/keyframe/timing_function.h"
#include "ui/gfx/geometry/test/geometry_util.h"

using DurationBehavior = cc::ScrollOffsetAnimationCurve::DurationBehavior;

const double kConstantDuration = 9.0;
const double kDurationDivisor = 60.0;
const double kInverseDeltaMaxDuration = 12.0;

namespace cc {
namespace {

// This is the value of the default Impulse bezier curve when t = 0.5
constexpr double halfway_through_default_impulse_curve = 0.874246;

}  // namespace

TEST(ScrollOffsetAnimationCurveTest, DeltaBasedDuration) {
  gfx::PointF target_value(100.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
          target_value));

  curve->SetInitialValue(target_value);
  EXPECT_DOUBLE_EQ(0.0, curve->Duration().InSecondsF());

  // x decreases, y stays the same.
  curve->SetInitialValue(gfx::PointF(136.f, 200.f));
  EXPECT_DOUBLE_EQ(0.1, curve->Duration().InSecondsF());

  // x increases, y stays the same.
  curve->SetInitialValue(gfx::PointF(19.f, 200.f));
  EXPECT_DOUBLE_EQ(0.15, curve->Duration().InSecondsF());

  // x stays the same, y decreases.
  curve->SetInitialValue(gfx::PointF(100.f, 344.f));
  EXPECT_DOUBLE_EQ(0.2, curve->Duration().InSecondsF());

  // x stays the same, y increases.
  curve->SetInitialValue(gfx::PointF(100.f, 191.f));
  EXPECT_DOUBLE_EQ(0.05, curve->Duration().InSecondsF());

  // x decreases, y decreases.
  curve->SetInitialValue(gfx::PointF(32500.f, 500.f));
  EXPECT_DOUBLE_EQ(0.7, curve->Duration().InSecondsF());

  // x decreases, y increases.
  curve->SetInitialValue(gfx::PointF(150.f, 119.f));
  EXPECT_DOUBLE_EQ(0.15, curve->Duration().InSecondsF());

  // x increases, y decreases.
  curve->SetInitialValue(gfx::PointF(0.f, 14600.f));
  EXPECT_DOUBLE_EQ(0.7, curve->Duration().InSecondsF());

  // x increases, y increases.
  curve->SetInitialValue(gfx::PointF(95.f, 191.f));
  EXPECT_DOUBLE_EQ(0.05, curve->Duration().InSecondsF());
}

TEST(ScrollOffsetAnimationCurveTest, GetValue) {
  gfx::PointF initial_value(2.f, 40.f);
  gfx::PointF target_value(10.f, 20.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
          target_value));
  curve->SetInitialValue(initial_value);

  base::TimeDelta duration = curve->Duration();
  EXPECT_GT(curve->Duration().InSecondsF(), 0);
  EXPECT_LT(curve->Duration().InSecondsF(), 0.1);

  EXPECT_EQ(duration, curve->Duration());

  EXPECT_POINTF_EQ(initial_value, curve->GetValue(base::Seconds(-1.0)));
  EXPECT_POINTF_EQ(initial_value, curve->GetValue(base::TimeDelta()));
  EXPECT_POINTF_NEAR(gfx::PointF(6.f, 30.f), curve->GetValue(duration * 0.5f),
                     0.00025);
  EXPECT_POINTF_EQ(target_value, curve->GetValue(duration));
  EXPECT_POINTF_EQ(target_value,
                   curve->GetValue(duration + base::Seconds(1.0)));

  // Verify that GetValue takes the timing function into account.
  gfx::PointF value = curve->GetValue(duration * 0.25f);
  EXPECT_NEAR(3.0333f, value.x(), 0.0002f);
  EXPECT_NEAR(37.4168f, value.y(), 0.0002f);
}

// Verify that a clone behaves exactly like the original.
TEST(ScrollOffsetAnimationCurveTest, Clone) {
  gfx::PointF initial_value(2.f, 40.f);
  gfx::PointF target_value(10.f, 20.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
          target_value));
  curve->SetInitialValue(initial_value);
  base::TimeDelta duration = curve->Duration();

  std::unique_ptr<gfx::AnimationCurve> clone(curve->Clone());

  EXPECT_EQ(duration, clone->Duration());

  ScrollOffsetAnimationCurve* cloned_curve =
      ScrollOffsetAnimationCurve::ToScrollOffsetAnimationCurve(clone.get());

  EXPECT_POINTF_EQ(initial_value, cloned_curve->GetValue(base::Seconds(-1.0)));
  EXPECT_POINTF_EQ(initial_value, cloned_curve->GetValue(base::TimeDelta()));
  EXPECT_POINTF_NEAR(gfx::PointF(6.f, 30.f),
                     cloned_curve->GetValue(duration * 0.5f), 0.00025);
  EXPECT_POINTF_EQ(target_value, cloned_curve->GetValue(duration));
  EXPECT_POINTF_EQ(target_value,
                   cloned_curve->GetValue(duration + base::Seconds(1.f)));

  // Verify that the timing function was cloned correctly.
  gfx::PointF value = cloned_curve->GetValue(duration * 0.25f);
  EXPECT_NEAR(3.0333f, value.x(), 0.0002f);
  EXPECT_NEAR(37.4168f, value.y(), 0.0002f);
}

TEST(ScrollOffsetAnimationCurveTest, EaseInOutUpdateTarget) {
  gfx::PointF initial_value(0.f, 0.f);
  gfx::PointF target_value(0.f, 3600.f);
  double duration = kConstantDuration / kDurationDivisor;
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
          target_value, DurationBehavior::kConstant));
  curve->SetInitialValue(initial_value);
  EXPECT_NEAR(duration, curve->Duration().InSecondsF(), 0.0002f);
  EXPECT_NEAR(1800.0, curve->GetValue(base::Seconds(duration / 2.0)).y(),
              0.0002f);
  EXPECT_NEAR(3600.0, curve->GetValue(base::Seconds(duration)).y(), 0.0002f);

  curve->UpdateTarget(base::Seconds(duration / 2), gfx::PointF(0.0, 9900.0));

  EXPECT_NEAR(duration * 1.5, curve->Duration().InSecondsF(), 0.0002f);
  EXPECT_NEAR(1800.0, curve->GetValue(base::Seconds(duration / 2.0)).y(),
              0.0002f);
  EXPECT_NEAR(6827.6, curve->GetValue(base::Seconds(duration)).y(), 0.1f);
  EXPECT_NEAR(9900.0, curve->GetValue(base::Seconds(duration * 1.5)).y(),
              0.0002f);

  curve->UpdateTarget(base::Seconds(duration), gfx::PointF(0.0, 7200.0));

  // A closer target at high velocity reduces the duration.
  EXPECT_NEAR(duration * 1.0794, curve->Duration().InSecondsF(), 0.0002f);
  EXPECT_NEAR(6827.6, curve->GetValue(base::Seconds(duration)).y(), 0.1f);
  EXPECT_NEAR(7200.0, curve->GetValue(base::Seconds(duration * 1.08)).y(),
              0.0002f);
}

TEST(ScrollOffsetAnimationCurveTest, ImpulseUpdateTarget) {
  gfx::PointF initial_value(0.f, 0.f);
  gfx::PointF initial_target_value(0.f, 3600.f);
  gfx::Vector2dF initial_delta = initial_target_value - initial_value;
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateImpulseAnimationForTesting(
          initial_target_value));
  curve->SetInitialValue(initial_value);

  base::TimeDelta initial_duration =
      ScrollOffsetAnimationCurve::ImpulseSegmentDuration(initial_delta,
                                                         base::TimeDelta());
  EXPECT_NEAR(initial_duration.InSecondsF(), curve->Duration().InSecondsF(),
              0.0002f);
  EXPECT_NEAR(initial_delta.y() * halfway_through_default_impulse_curve,
              curve->GetValue(initial_duration / 2).y(), 0.01f);
  EXPECT_NEAR(initial_delta.y(), curve->GetValue(initial_duration).y(),
              0.0002f);

  base::TimeDelta time_of_update = initial_duration / 2;
  gfx::PointF distance_halfway_through_initial_animation =
      curve->GetValue(time_of_update);

  gfx::PointF new_target_value(0.f, 9900.f);
  curve->UpdateTarget(time_of_update, new_target_value);

  gfx::Vector2dF new_delta =
      new_target_value - distance_halfway_through_initial_animation;
  base::TimeDelta updated_segment_duration =
      ScrollOffsetAnimationCurve::ImpulseSegmentDuration(new_delta,
                                                         base::TimeDelta());

  base::TimeDelta overall_duration = time_of_update + updated_segment_duration;
  EXPECT_NEAR(overall_duration.InSecondsF(), curve->Duration().InSecondsF(),
              0.0002f);
  EXPECT_NEAR(distance_halfway_through_initial_animation.y(),
              curve->GetValue(time_of_update).y(), 0.01f);
  EXPECT_NEAR(new_target_value.y(), curve->GetValue(overall_duration).y(),
              0.0002f);

  // Ensure that UpdateTarget increases the initial slope of the generated curve
  // (for velocity matching). To test this, we check if the value is greater
  // than the default value would be half way through.
  // Also - to ensure it isn't passing just due to floating point imprecision,
  // some epsilon is added to the default amount.
  EXPECT_LT(
      new_delta.y() * halfway_through_default_impulse_curve + 0.01f,
      curve->GetValue(time_of_update + (updated_segment_duration / 2)).y());
}

TEST(ScrollOffsetAnimationCurveTest, ImpulseUpdateTargetSwitchDirections) {
  gfx::PointF initial_value(0.f, 0.f);
  gfx::PointF initial_target_value(0.f, 200.f);
  double initial_duration =
      ScrollOffsetAnimationCurve::ImpulseSegmentDuration(
          initial_target_value.OffsetFromOrigin(), base::TimeDelta())
          .InSecondsF();

  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateImpulseAnimationForTesting(
          initial_target_value));
  curve->SetInitialValue(initial_value);
  EXPECT_NEAR(initial_duration, curve->Duration().InSecondsF(), 0.0002f);
  EXPECT_NEAR(initial_target_value.y() * halfway_through_default_impulse_curve,
              curve->GetValue(base::Seconds(initial_duration / 2.0)).y(),
              0.01f);

  // Animate back to 0. This should force the new curve's initial velocity to be
  // 0, so the default curve will be generated.
  gfx::PointF updated_initial_value(
      0, initial_target_value.y() * halfway_through_default_impulse_curve);
  gfx::PointF updated_target(0.f, 0.f);
  curve->UpdateTarget(base::Seconds(initial_duration / 2), updated_target);

  EXPECT_NEAR(initial_target_value.y() * halfway_through_default_impulse_curve,
              curve->GetValue(base::Seconds(initial_duration / 2.0)).y(),
              0.01f);

  double updated_duration =
      ScrollOffsetAnimationCurve::ImpulseSegmentDuration(
          gfx::Vector2dF(updated_initial_value.x(), updated_initial_value.y()),
          base::TimeDelta())
          .InSecondsF();
  EXPECT_NEAR(
      updated_initial_value.y() * (1.0 - halfway_through_default_impulse_curve),
      curve
          ->GetValue(
              base::Seconds(initial_duration / 2.0 + updated_duration / 2.0))
          .y(),
      0.01f);
  EXPECT_NEAR(
      0.0,
      curve->GetValue(base::Seconds(initial_duration / 2.0 + updated_duration))
          .y(),
      0.0002f);
}

TEST(ScrollOffsetAnimationCurveTest, InverseDeltaDuration) {
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
          gfx::PointF(0.f, 100.f), DurationBehavior::kInverseDelta));

  curve->SetInitialValue(gfx::PointF());
  double smallDeltaDuration = curve->Duration().InSecondsF();

  curve->UpdateTarget(base::Seconds(0.01f), gfx::PointF(0.f, 300.f));
  double mediumDeltaDuration = curve->Duration().InSecondsF();

  curve->UpdateTarget(base::Seconds(0.01f), gfx::PointF(0.f, 500.f));
  double largeDeltaDuration = curve->Duration().InSecondsF();

  EXPECT_GT(smallDeltaDuration, mediumDeltaDuration);
  EXPECT_GT(mediumDeltaDuration, largeDeltaDuration);

  curve->UpdateTarget(base::Seconds(0.01f), gfx::PointF(0.f, 5000.f));
  EXPECT_EQ(largeDeltaDuration, curve->Duration().InSecondsF());
}

TEST(ScrollOffsetAnimationCurveTest, LinearAnimation) {
  // Testing autoscroll downwards for a scroller of length 1000px.
  gfx::PointF current_offset(0.f, 0.f);
  gfx::PointF target_offset(0.f, 1000.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateLinearAnimationForTesting(
          target_offset));

  const float autoscroll_velocity = 800.f;  // pixels per second.
  curve->SetInitialValue(current_offset, base::TimeDelta(),
                         autoscroll_velocity);
  EXPECT_FLOAT_EQ(1.25f, curve->Duration().InSecondsF());

  // Test scrolling down from half way.
  current_offset = gfx::PointF(0.f, 500.f);
  curve->SetInitialValue(current_offset, base::TimeDelta(),
                         autoscroll_velocity);
  EXPECT_FLOAT_EQ(0.625f, curve->Duration().InSecondsF());

  // Test scrolling down when max_offset is reached.
  current_offset = gfx::PointF(0.f, 1000.f);
  curve->SetInitialValue(current_offset, base::TimeDelta(),
                         autoscroll_velocity);
  EXPECT_FLOAT_EQ(0.f, curve->Duration().InSecondsF());
}

TEST(ScrollOffsetAnimationCurveTest, ImpulseDuration) {
  // The duration of an impulse-style curve in milliseconds is simply 1.5x the
  // scroll distance in physical pixels, with a minimum of 200ms and a maximum
  // of 500ms.
  gfx::Vector2dF small_delta(0.f, 100.f);
  gfx::Vector2dF moderate_delta(0.f, 250.f);
  gfx::Vector2dF large_delta(0.f, 400.f);

  base::TimeDelta duration = ScrollOffsetAnimationCurve::ImpulseSegmentDuration(
      small_delta, base::TimeDelta());
  EXPECT_FLOAT_EQ(duration.InMillisecondsF(), 200.f);

  duration = ScrollOffsetAnimationCurve::ImpulseSegmentDuration(
      moderate_delta, base::TimeDelta());
  EXPECT_NEAR(duration.InMillisecondsF(), moderate_delta.y() * 1.5f, 0.0002f);

  duration = ScrollOffsetAnimationCurve::ImpulseSegmentDuration(
      large_delta, base::TimeDelta());
  EXPECT_FLOAT_EQ(duration.InMillisecondsF(), 500.f);
}

TEST(ScrollOffsetAnimationCurveTest, CurveWithDelay) {
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
          gfx::PointF(0.f, 100.f), DurationBehavior::kInverseDelta));
  double duration_in_seconds = kInverseDeltaMaxDuration / kDurationDivisor;
  double delay_in_seconds = 0.02;
  double curve_duration = duration_in_seconds - delay_in_seconds;

  curve->SetInitialValue(gfx::PointF(), base::Seconds(delay_in_seconds));
  EXPECT_NEAR(curve_duration, curve->Duration().InSecondsF(), 0.0002f);

  curve->UpdateTarget(base::Seconds(0.01f), gfx::PointF(0.f, 500.f));
  EXPECT_GT(curve_duration, curve->Duration().InSecondsF());
  EXPECT_EQ(gfx::PointF(0.f, 500.f), curve->target_value());
}

TEST(ScrollOffsetAnimationCurveTest, CurveWithLargeDelay) {
  DurationBehavior duration_hint = DurationBehavior::kInverseDelta;
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
          gfx::PointF(0.f, 100.f), duration_hint));
  curve->SetInitialValue(gfx::PointF(), base::Seconds(0.2));
  EXPECT_EQ(0.f, curve->Duration().InSecondsF());

  // Re-targeting when animation duration is 0.
  curve->UpdateTarget(base::Seconds(-0.01), gfx::PointF(0.f, 300.f));
  double duration =
      ScrollOffsetAnimationCurve::EaseInOutSegmentDuration(
          gfx::Vector2dF(0.f, 200.f), duration_hint, base::Seconds(0.01))
          .InSecondsF();
  EXPECT_EQ(duration, curve->Duration().InSecondsF());

  // Re-targeting before last_retarget_, the  difference should be accounted for
  // in duration.
  curve->UpdateTarget(base::Seconds(-0.01), gfx::PointF(0.f, 500.f));
  duration = ScrollOffsetAnimationCurve::EaseInOutSegmentDuration(
                 gfx::Vector2dF(0.f, 500.f), duration_hint, base::Seconds(0.01))
                 .InSecondsF();
  EXPECT_EQ(duration, curve->Duration().InSecondsF());

  EXPECT_POINTF_EQ(gfx::PointF(0.f, 500.f),
                   curve->GetValue(base::Seconds(1.0)));
}

// This test verifies that if the last segment duration is zero, ::UpdateTarget
// simply updates the total animation duration see crbug.com/645317.
TEST(ScrollOffsetAnimationCurveTest, UpdateTargetZeroLastSegmentDuration) {
  DurationBehavior duration_hint = DurationBehavior::kInverseDelta;
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
          gfx::PointF(0.f, 100.f), duration_hint));
  double duration_in_seconds = kInverseDeltaMaxDuration / kDurationDivisor;
  double delay_in_seconds = 0.02;
  double curve_duration = duration_in_seconds - delay_in_seconds;

  curve->SetInitialValue(gfx::PointF(), base::Seconds(delay_in_seconds));
  EXPECT_NEAR(curve_duration, curve->Duration().InSecondsF(), 0.0002f);

  // Re-target 1, this should set last_retarget_ to 0.05.
  gfx::Vector2dF new_delta =
      gfx::PointF(0.f, 200.f) - curve->GetValue(base::Seconds(0.05));
  double expected_duration =
      ScrollOffsetAnimationCurve::EaseInOutSegmentDuration(
          new_delta, duration_hint, base::TimeDelta())
          .InSecondsF() +
      0.05;
  curve->UpdateTarget(base::Seconds(0.05), gfx::PointF(0.f, 200.f));
  EXPECT_NEAR(expected_duration, curve->Duration().InSecondsF(), 0.0002f);

  // Re-target 2, this should set total_animation_duration to t, which is
  // last_retarget_. This is what would cause the DCHECK failure in
  // crbug.com/645317.
  curve->UpdateTarget(base::Seconds(-0.145), gfx::PointF(0.f, 300.f));
  EXPECT_NEAR(0.05, curve->Duration().InSecondsF(), 0.0002f);

  // Re-target 3, this should set total_animation_duration based on new_delta.
  new_delta = gfx::PointF(0.f, 500.f) - curve->GetValue(base::Seconds(0.05));
  expected_duration = ScrollOffsetAnimationCurve::EaseInOutSegmentDuration(
                          new_delta, duration_hint, base::Seconds(0.15))
                          .InSecondsF();
  curve->UpdateTarget(base::Seconds(-0.1), gfx::PointF(0.f, 500.f));
  EXPECT_NEAR(expected_duration, curve->Duration().InSecondsF(), 0.0002f);
}
}  // namespace cc
