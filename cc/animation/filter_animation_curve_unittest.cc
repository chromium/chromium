// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/filter_animation_curve.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/transform_operations.h"

namespace cc {
namespace {

#define SAMPLE(curve, time)                                                  \
  curve->GetTransformedValue(time,                                           \
                             time < base::TimeDelta()                        \
                                 ? gfx::TimingFunction::LimitDirection::LEFT \
                                 : gfx::TimingFunction::LimitDirection::RIGHT)

void ExpectBrightness(double brightness, const FilterOperations& filter) {
  EXPECT_EQ(1u, filter.size());
  EXPECT_EQ(FilterOperation::BRIGHTNESS, filter.at(0).type());
  EXPECT_FLOAT_EQ(brightness, filter.at(0).amount());
}

// Tests that a filter animation with one keyframe works as expected.
TEST(FilterAnimationCurveTest, OneFilterKeyframe) {
  std::unique_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());
  FilterOperations operations;
  operations.Append(FilterOperation::CreateBrightnessFilter(2.f));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::TimeDelta(), operations, nullptr));

  ExpectBrightness(2.f, SAMPLE(curve, base::Seconds(-1.f)));
  ExpectBrightness(2.f, SAMPLE(curve, base::Seconds(0.f)));
  ExpectBrightness(2.f, SAMPLE(curve, base::Seconds(0.5f)));
  ExpectBrightness(2.f, SAMPLE(curve, base::Seconds(1.f)));
  ExpectBrightness(2.f, SAMPLE(curve, base::Seconds(2.f)));
}

// Tests that a filter animation with two keyframes works as expected.
TEST(FilterAnimationCurveTest, TwoFilterKeyframe) {
  std::unique_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());
  FilterOperations operations1;
  operations1.Append(FilterOperation::CreateBrightnessFilter(2.f));
  FilterOperations operations2;
  operations2.Append(FilterOperation::CreateBrightnessFilter(4.f));

  curve->AddKeyframe(
      FilterKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::Seconds(1.f), operations2, nullptr));
  ExpectBrightness(2.f, SAMPLE(curve, base::Seconds(-1.f)));
  ExpectBrightness(2.f, SAMPLE(curve, base::Seconds(0.f)));
  ExpectBrightness(3.f, SAMPLE(curve, base::Seconds(0.5f)));
  ExpectBrightness(4.f, SAMPLE(curve, base::Seconds(1.f)));
  ExpectBrightness(4.f, SAMPLE(curve, base::Seconds(2.f)));
}

// Tests that a filter animation with three keyframes works as expected.
TEST(FilterAnimationCurveTest, ThreeFilterKeyframe) {
  std::unique_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());
  FilterOperations operations1;
  operations1.Append(FilterOperation::CreateBrightnessFilter(2.f));
  FilterOperations operations2;
  operations2.Append(FilterOperation::CreateBrightnessFilter(4.f));
  FilterOperations operations3;
  operations3.Append(FilterOperation::CreateBrightnessFilter(8.f));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::Seconds(1.f), operations2, nullptr));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::Seconds(2.f), operations3, nullptr));
  ExpectBrightness(2.f, SAMPLE(curve, base::Seconds(-1.f)));
  ExpectBrightness(2.f, SAMPLE(curve, base::Seconds(0.f)));
  ExpectBrightness(3.f, SAMPLE(curve, base::Seconds(0.5f)));
  ExpectBrightness(4.f, SAMPLE(curve, base::Seconds(1.f)));
  ExpectBrightness(6.f, SAMPLE(curve, base::Seconds(1.5f)));
  ExpectBrightness(8.f, SAMPLE(curve, base::Seconds(2.f)));
  ExpectBrightness(8.f, SAMPLE(curve, base::Seconds(3.f)));
}

// Tests that a filter animation with multiple keys at a given time works
// sanely.
TEST(FilterAnimationCurveTest, RepeatedFilterKeyTimes) {
  std::unique_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());
  // A step function.
  FilterOperations operations1;
  operations1.Append(FilterOperation::CreateBrightnessFilter(4.f));
  FilterOperations operations2;
  operations2.Append(FilterOperation::CreateBrightnessFilter(4.f));
  FilterOperations operations3;
  operations3.Append(FilterOperation::CreateBrightnessFilter(6.f));
  FilterOperations operations4;
  operations4.Append(FilterOperation::CreateBrightnessFilter(6.f));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::Seconds(1.f), operations2, nullptr));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::Seconds(1.f), operations3, nullptr));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::Seconds(2.f), operations4, nullptr));

  ExpectBrightness(4.f, SAMPLE(curve, base::Seconds(-1.f)));
  ExpectBrightness(4.f, SAMPLE(curve, base::Seconds(0.f)));
  ExpectBrightness(4.f, SAMPLE(curve, base::Seconds(0.5f)));

  // There is a discontinuity at 1. Any value between 4 and 6 is valid.
  FilterOperations value = SAMPLE(curve, base::Seconds(1.f));
  EXPECT_EQ(1u, value.size());
  EXPECT_EQ(FilterOperation::BRIGHTNESS, value.at(0).type());
  EXPECT_GE(value.at(0).amount(), 4);
  EXPECT_LE(value.at(0).amount(), 6);

  ExpectBrightness(6.f, SAMPLE(curve, base::Seconds(1.5f)));
  ExpectBrightness(6.f, SAMPLE(curve, base::Seconds(2.f)));
  ExpectBrightness(6.f, SAMPLE(curve, base::Seconds(3.f)));
}

}  // namespace
}  // namespace cc
