// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/keyframed_animation_curve.h"

#include "cc/animation/transform_operations.h"
#include "cc/test/geometry_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/test/gfx_util.h"

namespace cc {
namespace {

void ExpectTranslateX(SkMScalar translate_x,
                      const TransformOperations& operations) {
  EXPECT_FLOAT_EQ(translate_x, operations.Apply().matrix().get(0, 3));
}

void ExpectBrightness(double brightness, const FilterOperations& filter) {
  EXPECT_EQ(1u, filter.size());
  EXPECT_EQ(FilterOperation::BRIGHTNESS, filter.at(0).type());
  EXPECT_FLOAT_EQ(brightness, filter.at(0).amount());
}

// Tests that a color animation with one keyframe works as expected.
TEST(KeyframedAnimationCurveTest, OneColorKeyFrame) {
  SkColor color = SkColorSetARGB(255, 255, 255, 255);
  std::unique_ptr<KeyframedColorAnimationCurve> curve(
      KeyframedColorAnimationCurve::Create());
  curve->AddKeyframe(ColorKeyframe::Create(base::TimeDelta(), color, nullptr));

  EXPECT_SKCOLOR_EQ(color,
                    curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  EXPECT_SKCOLOR_EQ(color, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_SKCOLOR_EQ(color,
                    curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  EXPECT_SKCOLOR_EQ(color, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  EXPECT_SKCOLOR_EQ(color, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
}

// Tests that a color animation with two keyframes works as expected.
TEST(KeyframedAnimationCurveTest, TwoColorKeyFrame) {
  SkColor color_a = SkColorSetARGB(255, 255, 0, 0);
  SkColor color_b = SkColorSetARGB(255, 0, 255, 0);
  SkColor color_midpoint = gfx::Tween::ColorValueBetween(0.5, color_a, color_b);
  std::unique_ptr<KeyframedColorAnimationCurve> curve(
      KeyframedColorAnimationCurve::Create());
  curve->AddKeyframe(
      ColorKeyframe::Create(base::TimeDelta(), color_a, nullptr));
  curve->AddKeyframe(ColorKeyframe::Create(base::TimeDelta::FromSecondsD(1.0),
                                           color_b, nullptr));

  EXPECT_SKCOLOR_EQ(color_a,
                    curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  EXPECT_SKCOLOR_EQ(color_a,
                    curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_SKCOLOR_EQ(color_midpoint,
                    curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  EXPECT_SKCOLOR_EQ(color_b,
                    curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  EXPECT_SKCOLOR_EQ(color_b,
                    curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
}

// Tests that a color animation with three keyframes works as expected.
TEST(KeyframedAnimationCurveTest, ThreeColorKeyFrame) {
  SkColor color_a = SkColorSetARGB(255, 255, 0, 0);
  SkColor color_b = SkColorSetARGB(255, 0, 255, 0);
  SkColor color_c = SkColorSetARGB(255, 0, 0, 255);
  SkColor color_midpoint1 =
      gfx::Tween::ColorValueBetween(0.5, color_a, color_b);
  SkColor color_midpoint2 =
      gfx::Tween::ColorValueBetween(0.5, color_b, color_c);
  std::unique_ptr<KeyframedColorAnimationCurve> curve(
      KeyframedColorAnimationCurve::Create());
  curve->AddKeyframe(
      ColorKeyframe::Create(base::TimeDelta(), color_a, nullptr));
  curve->AddKeyframe(ColorKeyframe::Create(base::TimeDelta::FromSecondsD(1.0),
                                           color_b, nullptr));
  curve->AddKeyframe(ColorKeyframe::Create(base::TimeDelta::FromSecondsD(2.0),
                                           color_c, nullptr));

  EXPECT_SKCOLOR_EQ(color_a,
                    curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  EXPECT_SKCOLOR_EQ(color_a,
                    curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_SKCOLOR_EQ(color_midpoint1,
                    curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  EXPECT_SKCOLOR_EQ(color_b,
                    curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  EXPECT_SKCOLOR_EQ(color_midpoint2,
                    curve->GetValue(base::TimeDelta::FromSecondsD(1.5f)));
  EXPECT_SKCOLOR_EQ(color_c,
                    curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
  EXPECT_SKCOLOR_EQ(color_c,
                    curve->GetValue(base::TimeDelta::FromSecondsD(3.f)));
}

// Tests that a color animation with multiple keys at a given time works sanely.
TEST(KeyframedAnimationCurveTest, RepeatedColorKeyFrame) {
  SkColor color_a = SkColorSetARGB(255, 64, 0, 0);
  SkColor color_b = SkColorSetARGB(255, 192, 0, 0);

  std::unique_ptr<KeyframedColorAnimationCurve> curve(
      KeyframedColorAnimationCurve::Create());
  curve->AddKeyframe(
      ColorKeyframe::Create(base::TimeDelta(), color_a, nullptr));
  curve->AddKeyframe(ColorKeyframe::Create(base::TimeDelta::FromSecondsD(1.0),
                                           color_a, nullptr));
  curve->AddKeyframe(ColorKeyframe::Create(base::TimeDelta::FromSecondsD(1.0),
                                           color_b, nullptr));
  curve->AddKeyframe(ColorKeyframe::Create(base::TimeDelta::FromSecondsD(2.0),
                                           color_b, nullptr));

  EXPECT_SKCOLOR_EQ(color_a,
                    curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  EXPECT_SKCOLOR_EQ(color_a,
                    curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_SKCOLOR_EQ(color_a,
                    curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));

  SkColor value = curve->GetValue(base::TimeDelta::FromSecondsD(1.0f));
  EXPECT_EQ(255u, SkColorGetA(value));
  int red_value = SkColorGetR(value);
  EXPECT_LE(64, red_value);
  EXPECT_GE(192, red_value);

  EXPECT_SKCOLOR_EQ(color_b,
                    curve->GetValue(base::TimeDelta::FromSecondsD(1.5f)));
  EXPECT_SKCOLOR_EQ(color_b,
                    curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
  EXPECT_SKCOLOR_EQ(color_b,
                    curve->GetValue(base::TimeDelta::FromSecondsD(3.f)));
}

// Tests that a float animation with one keyframe works as expected.
TEST(KeyframedAnimationCurveTest, OneFloatKeyframe) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(FloatKeyframe::Create(base::TimeDelta(), 2.f, nullptr));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
}

// Tests that a float animation with two keyframes works as expected.
TEST(KeyframedAnimationCurveTest, TwoFloatKeyframe) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(FloatKeyframe::Create(base::TimeDelta(), 2.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.0), 4.f, nullptr));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_FLOAT_EQ(3.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  EXPECT_FLOAT_EQ(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  EXPECT_FLOAT_EQ(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
}

// Tests that a float animation with three keyframes works as expected.
TEST(KeyframedAnimationCurveTest, ThreeFloatKeyframe) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(FloatKeyframe::Create(base::TimeDelta(), 2.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.0), 4.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(2.0), 8.f, nullptr));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_FLOAT_EQ(3.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  EXPECT_FLOAT_EQ(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  EXPECT_FLOAT_EQ(6.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.5f)));
  EXPECT_FLOAT_EQ(8.f, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
  EXPECT_FLOAT_EQ(8.f, curve->GetValue(base::TimeDelta::FromSecondsD(3.f)));
}

// Tests that a float animation with multiple keys at a given time works sanely.
TEST(KeyframedAnimationCurveTest, RepeatedFloatKeyTimes) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(FloatKeyframe::Create(base::TimeDelta(), 4.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.0), 4.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.0), 6.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(2.0), 6.f, nullptr));

  EXPECT_FLOAT_EQ(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  EXPECT_FLOAT_EQ(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_FLOAT_EQ(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));

  // There is a discontinuity at 1. Any value between 4 and 6 is valid.
  float value = curve->GetValue(base::TimeDelta::FromSecondsD(1.f));
  EXPECT_TRUE(value >= 4 && value <= 6);

  EXPECT_FLOAT_EQ(6.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.5f)));
  EXPECT_FLOAT_EQ(6.f, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
  EXPECT_FLOAT_EQ(6.f, curve->GetValue(base::TimeDelta::FromSecondsD(3.f)));
}

// Tests that a transform animation with one keyframe works as expected.
TEST(KeyframedAnimationCurveTest, OneTransformKeyframe) {
  std::unique_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());
  TransformOperations operations;
  operations.AppendTranslate(2.f, 0.f, 0.f);
  curve->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations, nullptr));

  ExpectTranslateX(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  ExpectTranslateX(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  ExpectTranslateX(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  ExpectTranslateX(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  ExpectTranslateX(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
}

// Tests that a transform animation with two keyframes works as expected.
TEST(KeyframedAnimationCurveTest, TwoTransformKeyframe) {
  std::unique_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());
  TransformOperations operations1;
  operations1.AppendTranslate(2.f, 0.f, 0.f);
  TransformOperations operations2;
  operations2.AppendTranslate(4.f, 0.f, 0.f);

  curve->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations2, nullptr));
  ExpectTranslateX(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  ExpectTranslateX(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  ExpectTranslateX(3.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  ExpectTranslateX(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  ExpectTranslateX(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
}

// Tests that a transform animation with three keyframes works as expected.
TEST(KeyframedAnimationCurveTest, ThreeTransformKeyframe) {
  std::unique_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());
  TransformOperations operations1;
  operations1.AppendTranslate(2.f, 0.f, 0.f);
  TransformOperations operations2;
  operations2.AppendTranslate(4.f, 0.f, 0.f);
  TransformOperations operations3;
  operations3.AppendTranslate(8.f, 0.f, 0.f);
  curve->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations2, nullptr));
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(2.0), operations3, nullptr));
  ExpectTranslateX(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  ExpectTranslateX(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  ExpectTranslateX(3.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  ExpectTranslateX(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  ExpectTranslateX(6.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.5f)));
  ExpectTranslateX(8.f, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
  ExpectTranslateX(8.f, curve->GetValue(base::TimeDelta::FromSecondsD(3.f)));
}

// Tests that a transform animation with multiple keys at a given time works
// sanely.
TEST(KeyframedAnimationCurveTest, RepeatedTransformKeyTimes) {
  std::unique_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());
  // A step function.
  TransformOperations operations1;
  operations1.AppendTranslate(4.f, 0.f, 0.f);
  TransformOperations operations2;
  operations2.AppendTranslate(4.f, 0.f, 0.f);
  TransformOperations operations3;
  operations3.AppendTranslate(6.f, 0.f, 0.f);
  TransformOperations operations4;
  operations4.AppendTranslate(6.f, 0.f, 0.f);
  curve->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations2, nullptr));
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations3, nullptr));
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(2.0), operations4, nullptr));

  ExpectTranslateX(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  ExpectTranslateX(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  ExpectTranslateX(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));

  // There is a discontinuity at 1. Any value between 4 and 6 is valid.
  gfx::Transform value =
      curve->GetValue(base::TimeDelta::FromSecondsD(1.f)).Apply();
  EXPECT_GE(value.matrix().get(0, 3), 4.f);
  EXPECT_LE(value.matrix().get(0, 3), 6.f);

  ExpectTranslateX(6.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.5f)));
  ExpectTranslateX(6.f, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
  ExpectTranslateX(6.f, curve->GetValue(base::TimeDelta::FromSecondsD(3.f)));
}

// Tests that a discrete transform animation (e.g. where one or more keyframes
// is a non-invertible matrix) works as expected.
TEST(KeyframedAnimationCurveTest, DiscreteLinearTransformAnimation) {
  gfx::Transform non_invertible_matrix(0, 0, 0, 0, 0, 0);
  gfx::Transform identity_matrix;

  std::unique_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());
  TransformOperations operations1;
  operations1.AppendMatrix(non_invertible_matrix);
  TransformOperations operations2;
  operations2.AppendMatrix(identity_matrix);
  TransformOperations operations3;
  operations3.AppendMatrix(non_invertible_matrix);

  curve->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations2, nullptr));
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(2.0), operations3, nullptr));

  TransformOperations result;

  // Between 0 and 0.5 seconds, the first keyframe should be returned.
  result = curve->GetValue(base::TimeDelta::FromSecondsD(0.01f));
  EXPECT_TRANSFORMATION_MATRIX_EQ(non_invertible_matrix, result.Apply());

  result = curve->GetValue(base::TimeDelta::FromSecondsD(0.49f));
  EXPECT_TRANSFORMATION_MATRIX_EQ(non_invertible_matrix, result.Apply());

  // Between 0.5 and 1.5 seconds, the middle keyframe should be returned.
  result = curve->GetValue(base::TimeDelta::FromSecondsD(0.5f));
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, result.Apply());

  result = curve->GetValue(base::TimeDelta::FromSecondsD(1.49f));
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, result.Apply());

  // Between 1.5 and 2.0 seconds, the last keyframe should be returned.
  result = curve->GetValue(base::TimeDelta::FromSecondsD(1.5f));
  EXPECT_TRANSFORMATION_MATRIX_EQ(non_invertible_matrix, result.Apply());

  result = curve->GetValue(base::TimeDelta::FromSecondsD(2.0f));
  EXPECT_TRANSFORMATION_MATRIX_EQ(non_invertible_matrix, result.Apply());
}

TEST(KeyframedAnimationCurveTest, DiscreteCubicBezierTransformAnimation) {
  gfx::Transform non_invertible_matrix(0, 0, 0, 0, 0, 0);
  gfx::Transform identity_matrix;

  std::unique_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());
  TransformOperations operations1;
  operations1.AppendMatrix(non_invertible_matrix);
  TransformOperations operations2;
  operations2.AppendMatrix(identity_matrix);
  TransformOperations operations3;
  operations3.AppendMatrix(non_invertible_matrix);

  // The cubic-bezier here is a nice fairly strong ease-in curve, where 50%
  // progression is at approximately 85% of the time.
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta(), operations1,
      CubicBezierTimingFunction::Create(0.75f, 0.25f, 0.9f, 0.4f)));
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations2,
      CubicBezierTimingFunction::Create(0.75f, 0.25f, 0.9f, 0.4f)));
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(2.0), operations3,
      CubicBezierTimingFunction::Create(0.75f, 0.25f, 0.9f, 0.4f)));

  TransformOperations result;

  // Due to the cubic-bezier, the first keyframe is returned almost all the way
  // to 1 second.
  result = curve->GetValue(base::TimeDelta::FromSecondsD(0.01f));
  EXPECT_TRANSFORMATION_MATRIX_EQ(non_invertible_matrix, result.Apply());

  result = curve->GetValue(base::TimeDelta::FromSecondsD(0.8f));
  EXPECT_TRANSFORMATION_MATRIX_EQ(non_invertible_matrix, result.Apply());

  // Between ~0.85 and ~1.85 seconds, the middle keyframe should be returned.
  result = curve->GetValue(base::TimeDelta::FromSecondsD(0.85f));
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, result.Apply());

  result = curve->GetValue(base::TimeDelta::FromSecondsD(1.8f));
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, result.Apply());

  // Finally the last keyframe only takes effect after ~1.85 seconds.
  result = curve->GetValue(base::TimeDelta::FromSecondsD(1.85f));
  EXPECT_TRANSFORMATION_MATRIX_EQ(non_invertible_matrix, result.Apply());

  result = curve->GetValue(base::TimeDelta::FromSecondsD(2.0f));
  EXPECT_TRANSFORMATION_MATRIX_EQ(non_invertible_matrix, result.Apply());
}

// Tests that a filter animation with one keyframe works as expected.
TEST(KeyframedAnimationCurveTest, OneFilterKeyframe) {
  std::unique_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());
  FilterOperations operations;
  operations.Append(FilterOperation::CreateBrightnessFilter(2.f));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::TimeDelta(), operations, nullptr));

  ExpectBrightness(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  ExpectBrightness(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  ExpectBrightness(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  ExpectBrightness(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  ExpectBrightness(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
}

// Tests that a filter animation with two keyframes works as expected.
TEST(KeyframedAnimationCurveTest, TwoFilterKeyframe) {
  std::unique_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());
  FilterOperations operations1;
  operations1.Append(FilterOperation::CreateBrightnessFilter(2.f));
  FilterOperations operations2;
  operations2.Append(FilterOperation::CreateBrightnessFilter(4.f));

  curve->AddKeyframe(
      FilterKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  curve->AddKeyframe(FilterKeyframe::Create(base::TimeDelta::FromSecondsD(1.f),
                                            operations2, nullptr));
  ExpectBrightness(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  ExpectBrightness(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  ExpectBrightness(3.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  ExpectBrightness(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  ExpectBrightness(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
}

// Tests that a filter animation with three keyframes works as expected.
TEST(KeyframedAnimationCurveTest, ThreeFilterKeyframe) {
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
  curve->AddKeyframe(FilterKeyframe::Create(base::TimeDelta::FromSecondsD(1.f),
                                            operations2, nullptr));
  curve->AddKeyframe(FilterKeyframe::Create(base::TimeDelta::FromSecondsD(2.f),
                                            operations3, nullptr));
  ExpectBrightness(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  ExpectBrightness(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  ExpectBrightness(3.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  ExpectBrightness(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  ExpectBrightness(6.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.5f)));
  ExpectBrightness(8.f, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
  ExpectBrightness(8.f, curve->GetValue(base::TimeDelta::FromSecondsD(3.f)));
}

// Tests that a filter animation with multiple keys at a given time works
// sanely.
TEST(KeyframedAnimationCurveTest, RepeatedFilterKeyTimes) {
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
  curve->AddKeyframe(FilterKeyframe::Create(base::TimeDelta::FromSecondsD(1.f),
                                            operations2, nullptr));
  curve->AddKeyframe(FilterKeyframe::Create(base::TimeDelta::FromSecondsD(1.f),
                                            operations3, nullptr));
  curve->AddKeyframe(FilterKeyframe::Create(base::TimeDelta::FromSecondsD(2.f),
                                            operations4, nullptr));

  ExpectBrightness(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  ExpectBrightness(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  ExpectBrightness(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));

  // There is a discontinuity at 1. Any value between 4 and 6 is valid.
  FilterOperations value = curve->GetValue(base::TimeDelta::FromSecondsD(1.f));
  EXPECT_EQ(1u, value.size());
  EXPECT_EQ(FilterOperation::BRIGHTNESS, value.at(0).type());
  EXPECT_GE(value.at(0).amount(), 4);
  EXPECT_LE(value.at(0).amount(), 6);

  ExpectBrightness(6.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.5f)));
  ExpectBrightness(6.f, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
  ExpectBrightness(6.f, curve->GetValue(base::TimeDelta::FromSecondsD(3.f)));
}

// Tests that the keyframes may be added out of order.
TEST(KeyframedAnimationCurveTest, UnsortedKeyframes) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(2.f), 8.f, nullptr));
  curve->AddKeyframe(FloatKeyframe::Create(base::TimeDelta(), 2.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.f), 4.f, nullptr));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_FLOAT_EQ(3.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  EXPECT_FLOAT_EQ(4.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  EXPECT_FLOAT_EQ(6.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.5f)));
  EXPECT_FLOAT_EQ(8.f, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
  EXPECT_FLOAT_EQ(8.f, curve->GetValue(base::TimeDelta::FromSecondsD(3.f)));
}

// Tests that a linear timing function works as expected.
TEST(KeyframedAnimationCurveTest, LinearTimingFunction) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(FloatKeyframe::Create(base::TimeDelta(), 0.f,
                                           LinearTimingFunction::Create()));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.0), 1.f, nullptr));

  EXPECT_FLOAT_EQ(0.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_FLOAT_EQ(0.75f, curve->GetValue(base::TimeDelta::FromSecondsD(0.75f)));
}

// Tests that a cubic bezier timing function works as expected.
TEST(KeyframedAnimationCurveTest, CubicBezierTimingFunction) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(FloatKeyframe::Create(
      base::TimeDelta(), 0.f,
      CubicBezierTimingFunction::Create(0.25f, 0.f, 0.75f, 1.f)));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.0), 1.f, nullptr));

  EXPECT_FLOAT_EQ(0.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_LT(0.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.25f)));
  EXPECT_GT(0.25f, curve->GetValue(base::TimeDelta::FromSecondsD(0.25f)));
  EXPECT_NEAR(curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)), 0.5f,
              0.00015f);
  EXPECT_LT(0.75f, curve->GetValue(base::TimeDelta::FromSecondsD(0.75f)));
  EXPECT_GT(1.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.75f)));
  EXPECT_FLOAT_EQ(1.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
}

// Tests a step timing function if the change of values occur at the start.
TEST(KeyframedAnimationCurveTest, StepsTimingFunctionStepAtStart) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  const int num_steps = 36;
  curve->AddKeyframe(FloatKeyframe::Create(
      base::TimeDelta(), 0.f,
      StepsTimingFunction::Create(num_steps,
                                  StepsTimingFunction::StepPosition::START)));
  curve->AddKeyframe(FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.0),
                                           num_steps, nullptr));

  const float time_threshold = 0.0001f;

  for (float i = 0.f; i < num_steps; i += 1.f) {
    const base::TimeDelta time1 =
        base::TimeDelta::FromSecondsD(i / num_steps - time_threshold);
    const base::TimeDelta time2 =
        base::TimeDelta::FromSecondsD(i / num_steps + time_threshold);
    EXPECT_FLOAT_EQ(std::ceil(i), curve->GetValue(time1));
    EXPECT_FLOAT_EQ(std::ceil(i) + 1.f, curve->GetValue(time2));
  }
  EXPECT_FLOAT_EQ(num_steps,
                  curve->GetValue(base::TimeDelta::FromSecondsD(1.0)));

  for (float i = 0.5f; i <= num_steps; i += 1.0f) {
    const base::TimeDelta time = base::TimeDelta::FromSecondsD(i / num_steps);
    EXPECT_FLOAT_EQ(std::ceil(i), curve->GetValue(time));
  }
}

// Tests a step timing function if the change of values occur at the end.
TEST(KeyframedAnimationCurveTest, StepsTimingFunctionStepAtEnd) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  const int num_steps = 36;
  curve->AddKeyframe(FloatKeyframe::Create(
      base::TimeDelta(), 0.f,
      StepsTimingFunction::Create(num_steps,
                                  StepsTimingFunction::StepPosition::END)));
  curve->AddKeyframe(FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.0),
                                           num_steps, nullptr));

  const float time_threshold = 0.0001f;

  EXPECT_FLOAT_EQ(0.f, curve->GetValue(base::TimeDelta()));
  for (float i = 1.f; i <= num_steps; i += 1.f) {
    const base::TimeDelta time1 =
        base::TimeDelta::FromSecondsD(i / num_steps - time_threshold);
    const base::TimeDelta time2 =
        base::TimeDelta::FromSecondsD(i / num_steps + time_threshold);
    EXPECT_FLOAT_EQ(std::floor(i) - 1.f, curve->GetValue(time1));
    EXPECT_FLOAT_EQ(std::floor(i), curve->GetValue(time2));
  }
  EXPECT_FLOAT_EQ(num_steps,
                  curve->GetValue(base::TimeDelta::FromSecondsD(1.0)));

  for (float i = 0.5f; i <= num_steps; i += 1.0f) {
    const base::TimeDelta time = base::TimeDelta::FromSecondsD(i / num_steps);
    EXPECT_FLOAT_EQ(std::floor(i), curve->GetValue(time));
  }
}

// Tests that animations that are translations are correctly identified.
TEST(KeyframedAnimationCurveTest, IsTranslation) {
  std::unique_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations1;
  curve->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  operations1.AppendTranslate(2.0, 3.0, -1.0);
  TransformOperations operations2;
  operations2.AppendTranslate(4.0, 1.0, 2.0);
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.f), operations2, nullptr));

  EXPECT_TRUE(curve->IsTranslation());

  TransformOperations operations3;
  operations3.AppendScale(2.f, 2.f, 2.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(2.f), operations3, nullptr));

  EXPECT_FALSE(curve->IsTranslation());

  TransformOperations operations4;
  operations3.AppendTranslate(2.f, 2.f, 2.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(3.f), operations4, nullptr));

  EXPECT_FALSE(curve->IsTranslation());
}

// Tests that maximum target scale is computed as expected.
TEST(KeyframedAnimationCurveTest, MaximumTargetScale) {
  std::unique_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations1;
  curve->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  operations1.AppendScale(2.f, -3.f, 1.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.f), operations1,
      CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE)));

  float maximum_scale = 0.f;
  EXPECT_TRUE(curve->MaximumTargetScale(true, &maximum_scale));
  EXPECT_EQ(3.f, maximum_scale);

  TransformOperations operations2;
  operations2.AppendScale(6.f, 3.f, 2.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(2.f), operations2,
      CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE)));

  EXPECT_TRUE(curve->MaximumTargetScale(true, &maximum_scale));
  EXPECT_EQ(6.f, maximum_scale);

  TransformOperations operations3;
  operations3.AppendRotate(1.f, 0.f, 0.f, 90.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(3.f), operations3,
      CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE)));

  EXPECT_TRUE(curve->MaximumTargetScale(true, &maximum_scale));
  EXPECT_EQ(6.f, maximum_scale);

  TransformOperations operations4;
  operations4.AppendPerspective(3.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(4.f), operations4,
      CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE)));

  EXPECT_FALSE(curve->MaximumTargetScale(true, &maximum_scale));

  // The original scale is not used in computing the max.
  std::unique_ptr<KeyframedTransformAnimationCurve> curve2(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations5;
  operations5.AppendScale(0.4f, 0.2f, 0.6f);
  curve2->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta(), operations5,
      CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE)));
  TransformOperations operations6;
  operations6.AppendScale(0.5f, 0.3f, -0.8f);
  curve2->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.f), operations6,
      CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE)));

  EXPECT_TRUE(curve2->MaximumTargetScale(true, &maximum_scale));
  EXPECT_EQ(0.8f, maximum_scale);

  EXPECT_TRUE(curve2->MaximumTargetScale(false, &maximum_scale));
  EXPECT_EQ(0.6f, maximum_scale);
}

// Tests that starting animation scale is computed as expected.
TEST(KeyframedAnimationCurveTest, AnimationStartScale) {
  std::unique_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations1;
  curve->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  operations1.AppendScale(2.f, -3.f, 1.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.f), operations1,
      CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE)));

  float start_scale = 0.f;

  // Forward direction
  EXPECT_TRUE(curve->AnimationStartScale(true, &start_scale));
  EXPECT_EQ(1.f, start_scale);

  // Backward direction
  EXPECT_TRUE(curve->AnimationStartScale(false, &start_scale));
  EXPECT_EQ(3.f, start_scale);

  TransformOperations operations2;
  operations2.AppendScale(6.f, 3.f, 2.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(2.f), operations2,
      CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE)));

  // Forward direction
  EXPECT_TRUE(curve->AnimationStartScale(true, &start_scale));
  EXPECT_EQ(1.f, start_scale);

  // Backward direction
  EXPECT_TRUE(curve->AnimationStartScale(false, &start_scale));
  EXPECT_EQ(6.f, start_scale);

  TransformOperations operations3;
  operations3.AppendRotate(1.f, 0.f, 0.f, 90.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(3.f), operations3,
      CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE)));

  EXPECT_TRUE(curve->AnimationStartScale(false, &start_scale));
  EXPECT_EQ(1.f, start_scale);

  TransformOperations operations4;
  operations4.AppendPerspective(90.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(4.f), operations4,
      CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE)));

  EXPECT_FALSE(curve->AnimationStartScale(false, &start_scale));
  EXPECT_EQ(0.f, start_scale);
}

// Tests that an animation with a curve timing function works as expected.
TEST(KeyframedAnimationCurveTest, CurveTiming) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(FloatKeyframe::Create(base::TimeDelta(), 0.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.f), 1.f, nullptr));
  curve->SetTimingFunction(
      CubicBezierTimingFunction::Create(0.75f, 0.f, 0.25f, 1.f));
  EXPECT_FLOAT_EQ(0.f, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  EXPECT_FLOAT_EQ(0.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_NEAR(0.05f, curve->GetValue(base::TimeDelta::FromSecondsD(0.25f)),
              0.005f);
  EXPECT_FLOAT_EQ(0.5f, curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  EXPECT_NEAR(0.95f, curve->GetValue(base::TimeDelta::FromSecondsD(0.75f)),
              0.005f);
  EXPECT_FLOAT_EQ(1.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  EXPECT_FLOAT_EQ(1.f, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
}

// Tests that an animation with a curve and keyframe timing function works as
// expected.
TEST(KeyframedAnimationCurveTest, CurveAndKeyframeTiming) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(FloatKeyframe::Create(
      base::TimeDelta(), 0.f,
      CubicBezierTimingFunction::Create(0.35f, 0.f, 0.65f, 1.f)));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.f), 1.f, nullptr));
  // Curve timing function producing outputs outside of range [0,1].
  curve->SetTimingFunction(
      CubicBezierTimingFunction::Create(0.5f, -0.5f, 0.5f, 1.5f));
  EXPECT_FLOAT_EQ(0.f, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  EXPECT_FLOAT_EQ(0.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_FLOAT_EQ(0.f, curve->GetValue(base::TimeDelta::FromSecondsD(
                           0.25f)));  // Clamped. c(.25) < 0
  EXPECT_NEAR(0.17f, curve->GetValue(base::TimeDelta::FromSecondsD(0.42f)),
              0.005f);  // c(.42)=.27, k(.27)=.17
  EXPECT_FLOAT_EQ(0.5f, curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  EXPECT_NEAR(0.83f, curve->GetValue(base::TimeDelta::FromSecondsD(0.58f)),
              0.005f);  // c(.58)=.73, k(.73)=.83
  EXPECT_FLOAT_EQ(1.f, curve->GetValue(base::TimeDelta::FromSecondsD(
                           0.75f)));  // Clamped. c(.75) > 1
  EXPECT_FLOAT_EQ(1.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  EXPECT_FLOAT_EQ(1.f, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
}

// Tests that a linear timing function works as expected for inputs outside of
// range [0,1]
TEST(KeyframedAnimationCurveTest, LinearTimingInputsOutsideZeroOneRange) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(FloatKeyframe::Create(base::TimeDelta(), 0.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.0), 2.f, nullptr));
  // Curve timing function producing timing outputs outside of range [0,1].
  curve->SetTimingFunction(
      CubicBezierTimingFunction::Create(0.5f, -0.5f, 0.5f, 1.5f));

  EXPECT_NEAR(-0.076f, curve->GetValue(base::TimeDelta::FromSecondsD(0.25f)),
              0.001f);
  EXPECT_NEAR(2.076f, curve->GetValue(base::TimeDelta::FromSecondsD(0.75f)),
              0.001f);
}

// If a curve cubic-bezier timing function produces timing outputs outside
// the range [0, 1] then a keyframe cubic-bezier timing function
// should consume that input properly (using end-point gradients).
TEST(KeyframedAnimationCurveTest, CurveTimingInputsOutsideZeroOneRange) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  // Keyframe timing function with 0.5 gradients at each end.
  curve->AddKeyframe(FloatKeyframe::Create(
      base::TimeDelta(), 0.f,
      CubicBezierTimingFunction::Create(0.5f, 0.25f, 0.5f, 0.75f)));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.f), 1.f, nullptr));
  // Curve timing function producing timing outputs outside of range [0,1].
  curve->SetTimingFunction(
      CubicBezierTimingFunction::Create(0.5f, -0.5f, 0.5f, 1.5f));

  EXPECT_NEAR(-0.02f, curve->GetValue(base::TimeDelta::FromSecondsD(0.25f)),
              0.002f);  // c(.25)=-.04, -.04*0.5=-0.02
  EXPECT_NEAR(0.33f, curve->GetValue(base::TimeDelta::FromSecondsD(0.46f)),
              0.002f);  // c(.46)=.38, k(.38)=.33

  EXPECT_NEAR(0.67f, curve->GetValue(base::TimeDelta::FromSecondsD(0.54f)),
              0.002f);  // c(.54)=.62, k(.62)=.67
  EXPECT_NEAR(1.02f, curve->GetValue(base::TimeDelta::FromSecondsD(0.75f)),
              0.002f);  // c(.75)=1.04 1+.04*0.5=1.02
}

// Tests that a step timing function works as expected for inputs outside of
// range [0,1]
TEST(KeyframedAnimationCurveTest, StepsTimingStartInputsOutsideZeroOneRange) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta(), 0.f,
                            StepsTimingFunction::Create(
                                4, StepsTimingFunction::StepPosition::START)));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.0), 2.f, nullptr));
  // Curve timing function producing timing outputs outside of range [0,1].
  curve->SetTimingFunction(
      CubicBezierTimingFunction::Create(0.5f, -0.5f, 0.5f, 1.5f));

  EXPECT_FLOAT_EQ(0.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.25f)));
  EXPECT_FLOAT_EQ(2.5f, curve->GetValue(base::TimeDelta::FromSecondsD(0.75f)));
}

TEST(KeyframedAnimationCurveTest, StepsTimingEndInputsOutsideZeroOneRange) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(FloatKeyframe::Create(
      base::TimeDelta(), 0.f,
      StepsTimingFunction::Create(4, StepsTimingFunction::StepPosition::END)));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.0), 2.f, nullptr));
  // Curve timing function producing timing outputs outside of range [0,1].
  curve->SetTimingFunction(
      CubicBezierTimingFunction::Create(0.5f, -0.5f, 0.5f, 1.5f));

  EXPECT_FLOAT_EQ(-0.5f, curve->GetValue(base::TimeDelta::FromSecondsD(0.25f)));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.75f)));
}

// Tests that an animation with a curve timing function and multiple keyframes
// works as expected.
TEST(KeyframedAnimationCurveTest, CurveTimingMultipleKeyframes) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(FloatKeyframe::Create(base::TimeDelta(), 0.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.f), 1.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(2.f), 3.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(3.f), 6.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(4.f), 9.f, nullptr));
  curve->SetTimingFunction(
      CubicBezierTimingFunction::Create(0.5f, 0.f, 0.5f, 1.f));
  EXPECT_FLOAT_EQ(0.f, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  EXPECT_FLOAT_EQ(0.f, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_NEAR(0.42f, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)),
              0.005f);
  EXPECT_NEAR(1.f, curve->GetValue(base::TimeDelta::FromSecondsD(1.455f)),
              0.005f);
  EXPECT_FLOAT_EQ(3.f, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
  EXPECT_NEAR(8.72f, curve->GetValue(base::TimeDelta::FromSecondsD(3.5f)),
              0.01f);
  EXPECT_FLOAT_EQ(9.f, curve->GetValue(base::TimeDelta::FromSecondsD(4.f)));
  EXPECT_FLOAT_EQ(9.f, curve->GetValue(base::TimeDelta::FromSecondsD(5.f)));
}

// Tests that an animation with a curve timing function that overshoots works as
// expected.
TEST(KeyframedAnimationCurveTest, CurveTimingOvershootMultipeKeyframes) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(FloatKeyframe::Create(base::TimeDelta(), 0.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.0), 1.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(2.0), 3.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(3.0), 6.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(4.0), 9.f, nullptr));
  // Curve timing function producing outputs outside of range [0,1].
  curve->SetTimingFunction(
      CubicBezierTimingFunction::Create(0.5f, -0.5f, 0.5f, 1.5f));
  EXPECT_LE(curve->GetValue(base::TimeDelta::FromSecondsD(1.f)),
            0.f);  // c(.25) < 0
  EXPECT_GE(curve->GetValue(base::TimeDelta::FromSecondsD(3.f)),
            9.f);  // c(.75) > 1
}

// Tests that a float animation with multiple keys works with scaled duration.
TEST(KeyframedAnimationCurveTest, ScaledDuration) {
  std::unique_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(FloatKeyframe::Create(base::TimeDelta(), 0.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(1.f), 1.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(2.f), 3.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(3.f), 6.f, nullptr));
  curve->AddKeyframe(
      FloatKeyframe::Create(base::TimeDelta::FromSecondsD(4.f), 9.f, nullptr));
  curve->SetTimingFunction(
      CubicBezierTimingFunction::Create(0.5f, 0.f, 0.5f, 1.f));

  const double scale = 1000.0;
  curve->set_scaled_duration(scale);

  EXPECT_DOUBLE_EQ(scale * 4, curve->Duration().InSecondsF());

  EXPECT_FLOAT_EQ(0.f,
                  curve->GetValue(base::TimeDelta::FromSecondsD(scale * -1.f)));
  EXPECT_FLOAT_EQ(0.f,
                  curve->GetValue(base::TimeDelta::FromSecondsD(scale * 0.f)));
  EXPECT_NEAR(0.42f,
              curve->GetValue(base::TimeDelta::FromSecondsD(scale * 1.f)),
              0.005f);
  EXPECT_NEAR(1.f,
              curve->GetValue(base::TimeDelta::FromSecondsD(scale * 1.455f)),
              0.005f);
  EXPECT_FLOAT_EQ(3.f,
                  curve->GetValue(base::TimeDelta::FromSecondsD(scale * 2.f)));
  EXPECT_NEAR(8.72f,
              curve->GetValue(base::TimeDelta::FromSecondsD(scale * 3.5f)),
              0.01f);
  EXPECT_FLOAT_EQ(9.f,
                  curve->GetValue(base::TimeDelta::FromSecondsD(scale * 4.f)));
  EXPECT_FLOAT_EQ(9.f,
                  curve->GetValue(base::TimeDelta::FromSecondsD(scale * 5.f)));
}

// Tests that a size animation with one keyframe works as expected.
TEST(KeyframedAnimationCurveTest, OneSizeKeyFrame) {
  gfx::SizeF size = gfx::SizeF(100, 100);
  std::unique_ptr<KeyframedSizeAnimationCurve> curve(
      KeyframedSizeAnimationCurve::Create());
  curve->AddKeyframe(SizeKeyframe::Create(base::TimeDelta(), size, nullptr));

  EXPECT_SIZEF_EQ(size, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  EXPECT_SIZEF_EQ(size, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_SIZEF_EQ(size, curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  EXPECT_SIZEF_EQ(size, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  EXPECT_SIZEF_EQ(size, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
}

// Tests that a size animation with two keyframes works as expected.
TEST(KeyframedAnimationCurveTest, TwoSizeKeyFrame) {
  gfx::SizeF size_a = gfx::SizeF(100, 100);
  gfx::SizeF size_b = gfx::SizeF(100, 0);
  gfx::SizeF size_midpoint = gfx::Tween::SizeFValueBetween(0.5, size_a, size_b);
  std::unique_ptr<KeyframedSizeAnimationCurve> curve(
      KeyframedSizeAnimationCurve::Create());
  curve->AddKeyframe(SizeKeyframe::Create(base::TimeDelta(), size_a, nullptr));
  curve->AddKeyframe(SizeKeyframe::Create(base::TimeDelta::FromSecondsD(1.0),
                                          size_b, nullptr));

  EXPECT_SIZEF_EQ(size_a, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  EXPECT_SIZEF_EQ(size_a, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_SIZEF_EQ(size_midpoint,
                  curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  EXPECT_SIZEF_EQ(size_b, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  EXPECT_SIZEF_EQ(size_b, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
}

// Tests that a size animation with three keyframes works as expected.
TEST(KeyframedAnimationCurveTest, ThreeSizeKeyFrame) {
  gfx::SizeF size_a = gfx::SizeF(100, 100);
  gfx::SizeF size_b = gfx::SizeF(100, 0);
  gfx::SizeF size_c = gfx::SizeF(200, 0);
  gfx::SizeF size_midpoint1 =
      gfx::Tween::SizeFValueBetween(0.5, size_a, size_b);
  gfx::SizeF size_midpoint2 =
      gfx::Tween::SizeFValueBetween(0.5, size_b, size_c);
  std::unique_ptr<KeyframedSizeAnimationCurve> curve(
      KeyframedSizeAnimationCurve::Create());
  curve->AddKeyframe(SizeKeyframe::Create(base::TimeDelta(), size_a, nullptr));
  curve->AddKeyframe(SizeKeyframe::Create(base::TimeDelta::FromSecondsD(1.0),
                                          size_b, nullptr));
  curve->AddKeyframe(SizeKeyframe::Create(base::TimeDelta::FromSecondsD(2.0),
                                          size_c, nullptr));

  EXPECT_SIZEF_EQ(size_a, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  EXPECT_SIZEF_EQ(size_a, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_SIZEF_EQ(size_midpoint1,
                  curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));
  EXPECT_SIZEF_EQ(size_b, curve->GetValue(base::TimeDelta::FromSecondsD(1.f)));
  EXPECT_SIZEF_EQ(size_midpoint2,
                  curve->GetValue(base::TimeDelta::FromSecondsD(1.5f)));
  EXPECT_SIZEF_EQ(size_c, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
  EXPECT_SIZEF_EQ(size_c, curve->GetValue(base::TimeDelta::FromSecondsD(3.f)));
}

// Tests that a size animation with multiple keys at a given time works sanely.
TEST(KeyframedAnimationCurveTest, RepeatedSizeKeyFrame) {
  gfx::SizeF size_a = gfx::SizeF(100, 64);
  gfx::SizeF size_b = gfx::SizeF(100, 192);

  std::unique_ptr<KeyframedSizeAnimationCurve> curve(
      KeyframedSizeAnimationCurve::Create());
  curve->AddKeyframe(SizeKeyframe::Create(base::TimeDelta(), size_a, nullptr));
  curve->AddKeyframe(SizeKeyframe::Create(base::TimeDelta::FromSecondsD(1.0),
                                          size_a, nullptr));
  curve->AddKeyframe(SizeKeyframe::Create(base::TimeDelta::FromSecondsD(1.0),
                                          size_b, nullptr));
  curve->AddKeyframe(SizeKeyframe::Create(base::TimeDelta::FromSecondsD(2.0),
                                          size_b, nullptr));

  EXPECT_SIZEF_EQ(size_a, curve->GetValue(base::TimeDelta::FromSecondsD(-1.f)));
  EXPECT_SIZEF_EQ(size_a, curve->GetValue(base::TimeDelta::FromSecondsD(0.f)));
  EXPECT_SIZEF_EQ(size_a, curve->GetValue(base::TimeDelta::FromSecondsD(0.5f)));

  gfx::SizeF value = curve->GetValue(base::TimeDelta::FromSecondsD(1.0f));
  EXPECT_FLOAT_EQ(100.0f, value.width());
  EXPECT_LE(64.0f, value.height());
  EXPECT_GE(192.0f, value.height());

  EXPECT_SIZEF_EQ(size_b, curve->GetValue(base::TimeDelta::FromSecondsD(1.5f)));
  EXPECT_SIZEF_EQ(size_b, curve->GetValue(base::TimeDelta::FromSecondsD(2.f)));
  EXPECT_SIZEF_EQ(size_b, curve->GetValue(base::TimeDelta::FromSecondsD(3.f)));
}

}  // namespace
}  // namespace cc
