// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/keyframe_model.h"

#include <limits>

#include "base/strings/stringprintf.h"
#include "cc/test/animation_test_common.h"
#include "cc/trees/target_property.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {


static base::TimeTicks TicksFromSecondsF(double seconds) {
  return base::TimeTicks() + base::Seconds(seconds);
}

std::unique_ptr<KeyframeModel> CreateKeyframeModel(
    double iterations,
    double duration,
    double playback_rate,
    base::TimeTicks start_time = base::TimeTicks()) {
  std::unique_ptr<KeyframeModel> to_return(KeyframeModel::Create(
      std::make_unique<FakeFloatAnimationCurve>(duration), 0, 1,
      KeyframeModel::TargetPropertyId(TargetProperty::OPACITY)));
  to_return->set_iterations(iterations);
  to_return->set_playback_rate(playback_rate);
  to_return->set_start_time(start_time);
  return to_return;
}

std::unique_ptr<KeyframeModel> CreateKeyframeModel(double iterations,
                                                   double duration) {
  return CreateKeyframeModel(iterations, duration, 1);
}

std::unique_ptr<KeyframeModel> CreateKeyframeModel(double iterations) {
  return CreateKeyframeModel(iterations, 1, 1);
}

TEST(KeyframeModelTest, TrimTimeZeroIterations) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(0));
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeOneIteration) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(1,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(1,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeOneHalfIteration) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1.5));
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(0.9,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.9))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeInfiniteIterations) {
  std::unique_ptr<KeyframeModel> keyframe_model(
      CreateKeyframeModel(std::numeric_limits<double>::infinity()));
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(
      CreateKeyframeModel(std::numeric_limits<double>::infinity()));
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0))
                .InSecondsF());
  EXPECT_EQ(0.75,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(0.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0.75,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeAlternateInfiniteIterations) {
  std::unique_ptr<KeyframeModel> keyframe_model(
      CreateKeyframeModel(std::numeric_limits<double>::infinity()));
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_NORMAL);
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(0.75,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0.75,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeAlternateOneIteration) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_NORMAL);
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(0.75,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeAlternateTwoIterations) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(2));
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_NORMAL);
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(0.75,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0.75,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                .InSecondsF());
  EXPECT_EQ(0.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeAlternateTwoHalfIterations) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(2.5));
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_NORMAL);
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(0.75,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0.75,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                .InSecondsF());
  EXPECT_EQ(0.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
  EXPECT_EQ(0.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.50))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.75))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeAlternateReverseInfiniteIterations) {
  std::unique_ptr<KeyframeModel> keyframe_model(
      CreateKeyframeModel(std::numeric_limits<double>::infinity()));
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_REVERSE);
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.75,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(0.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeAlternateReverseOneIteration) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_REVERSE);
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.75,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(0.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeAlternateReverseTwoIterations) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(2));
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_REVERSE);
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.75,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(0.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                .InSecondsF());
  EXPECT_EQ(0.75,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeStartTime) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_start_time(TicksFromSecondsF(4));
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(4.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(4.5))
                .InSecondsF());
  EXPECT_EQ(1,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(5.0))
                .InSecondsF());
  EXPECT_EQ(1,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(6.0))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeStartTimeReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_start_time(TicksFromSecondsF(4));
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(4.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(4.5))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(5.0))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(6.0))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeNegativeStartDelay) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_start_delay(base::Milliseconds(-4000));
  keyframe_model->set_start_time(TicksFromSecondsF(4));
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(1,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(1,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeNegativeStartDelayReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_start_delay(base::Milliseconds(-4000));
  keyframe_model->set_start_time(TicksFromSecondsF(4));
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);

  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeStartDelay) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_start_delay(base::Milliseconds(4000));
  keyframe_model->set_start_time(TicksFromSecondsF(4));
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(4.0))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(8.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(8.5))
                .InSecondsF());
  EXPECT_EQ(1,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(9.0))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeStartDelayReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_start_delay(base::Milliseconds(4000));
  keyframe_model->set_start_time(TicksFromSecondsF(4));
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(4.0))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(8.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(8.5))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(9.0))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePauseBasic) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);

  keyframe_model->Pause(base::Seconds(0.5));
  // When paused, the time returned is always the hold time
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.2))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePauseAffectedByDelay) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);
  // Delay of 0.2s
  keyframe_model->set_start_delay(base::Seconds(0.2));
  keyframe_model->Pause(base::Seconds(0.5));
  EXPECT_EQ(0.3,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.1))
                .InSecondsF());

  // Negative delay (seek) of 0.2s
  keyframe_model->set_start_delay(base::Seconds(-0.2));
  EXPECT_EQ(0.7,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.1))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePauseNotAffectedByStartTime) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);
  // Pause time is in local time so start time should not affect it.
  keyframe_model->set_start_time(TicksFromSecondsF(0.2));
  keyframe_model->Pause(base::Seconds(0.5));
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.1))
                .InSecondsF());

  keyframe_model->UnpauseForTesting(TicksFromSecondsF(0.7));
  keyframe_model->Pause(base::Seconds(0.7));
  EXPECT_EQ(0.7,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.1))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePauseResume) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.4,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.4))
                .InSecondsF());
  keyframe_model->Pause(base::Seconds(0.6));
  EXPECT_EQ(0.6,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0.6,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                .InSecondsF());
  keyframe_model->Pause(base::Seconds(0.7));
  EXPECT_EQ(0.7,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  // Resuming at monotonic time 1.0 with a hold time of 0.7 means start time is
  // 0.3.
  keyframe_model->UnpauseForTesting(TicksFromSecondsF(1.0));
  EXPECT_EQ(0, keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0))
                   .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.3))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.8))
                .InSecondsF());
  EXPECT_EQ(1,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.3))
                .InSecondsF());
  EXPECT_EQ(1,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePauseResumeReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  keyframe_model->Pause(base::Seconds(0.4));
  EXPECT_EQ(0.6,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  // Resuming at monotonic time 1.0 with a hold time of 0.4 means start time is
  // 0.6.
  keyframe_model->UnpauseForTesting(TicksFromSecondsF(1.0));
  EXPECT_EQ(0.6,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.6))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeZeroDuration) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(0, 0));
  keyframe_model->SetRunState(KeyframeModel::RUNNING);
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
}

// TODO(crbug.com/497867796): This is testing the special treatment of
// run_state_ == STARTING && start_time_ == null. This diverges from blink which
// would consider the currentTime null instead of zero. We should update any
// code relying on this behavior and delete this test.
TEST(KeyframeModelTest, TrimTimeStarting) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1, 5.0));
  keyframe_model->ResetStartTimeForTesting();
  keyframe_model->SetRunState(KeyframeModel::STARTING);
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  keyframe_model->ResetStartTimeForTesting();
  keyframe_model->set_hold_time(base::Milliseconds(2000));
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  // Transition to RUNNING computes start_time = 0s - 2s = -2s.
  keyframe_model->UnpauseForTesting(TicksFromSecondsF(0.0));
  keyframe_model->SetRunState(KeyframeModel::RUNNING);
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(3.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(4.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeNeedsSynchronizedStartTime) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1, 5.0));
  keyframe_model->SetRunState(KeyframeModel::RUNNING);
  keyframe_model->set_needs_synchronized_start_time(true);
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  keyframe_model->ResetStartTimeForTesting();
  keyframe_model->set_hold_time(base::Milliseconds(2000));
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  keyframe_model->set_needs_synchronized_start_time(false);
  // hold_time of 2s above means start_time = -1
  keyframe_model->UnpauseForTesting(TicksFromSecondsF(1.0));
  // With start_time = -1s, at 0s local_time is 1s.
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-2.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(3.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
}

TEST(KeyframeModelTest, IsFinishedAtZeroIterations) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(0));
  keyframe_model->SetRunState(KeyframeModel::RUNNING);
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(1.0)));
}

TEST(KeyframeModelTest, IsFinishedAtOneIteration) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->SetRunState(KeyframeModel::RUNNING);
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(-1.0)));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(1.0)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(2.0)));
}

TEST(KeyframeModelTest, IsFinishedAtInfiniteIterations) {
  std::unique_ptr<KeyframeModel> keyframe_model(
      CreateKeyframeModel(std::numeric_limits<double>::infinity()));
  keyframe_model->SetRunState(KeyframeModel::RUNNING);
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.5)));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(1.0)));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(1.5)));
}

TEST(KeyframeModelTest, IsFinishedStartDelay) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_start_delay(base::Milliseconds(500));
  keyframe_model->SetRunState(KeyframeModel::RUNNING);

  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(-1.0)));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.5)));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(1.0)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(1.5)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(2.0)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(2.5)));
}

TEST(KeyframeModelTest, IsFinishedNegativeStartDelay) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_start_delay(base::Milliseconds(-500));
  keyframe_model->SetRunState(KeyframeModel::RUNNING);

  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(-1.0)));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.5)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(1.0)));
}

TEST(KeyframeModelTest, IsFinishedAtNotRunning) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(0));
  keyframe_model->SetRunState(KeyframeModel::RUNNING);
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  keyframe_model->SetRunState(KeyframeModel::PAUSED);
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  keyframe_model->SetRunState(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY);
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  keyframe_model->SetRunState(KeyframeModel::FINISHED);
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  keyframe_model->SetRunState(KeyframeModel::ABORTED);
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
}

TEST(KeyframeModelTest, IsFinished) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->SetRunState(KeyframeModel::RUNNING);
  EXPECT_FALSE(keyframe_model->is_finished());
  keyframe_model->SetRunState(KeyframeModel::PAUSED);
  EXPECT_FALSE(keyframe_model->is_finished());
  keyframe_model->SetRunState(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY);
  EXPECT_FALSE(keyframe_model->is_finished());
  keyframe_model->SetRunState(KeyframeModel::FINISHED);
  EXPECT_TRUE(keyframe_model->is_finished());
  keyframe_model->SetRunState(KeyframeModel::ABORTED);
  EXPECT_TRUE(keyframe_model->is_finished());
}

TEST(KeyframeModelTest, IsFinishedNeedsSynchronizedStartTime) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->SetRunState(KeyframeModel::RUNNING);
  EXPECT_FALSE(keyframe_model->is_finished());
  keyframe_model->SetRunState(KeyframeModel::PAUSED);
  EXPECT_FALSE(keyframe_model->is_finished());
  keyframe_model->SetRunState(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY);
  EXPECT_FALSE(keyframe_model->is_finished());
  keyframe_model->SetRunState(KeyframeModel::FINISHED);
  EXPECT_TRUE(keyframe_model->is_finished());
  keyframe_model->SetRunState(KeyframeModel::ABORTED);
  EXPECT_TRUE(keyframe_model->is_finished());
}

TEST(KeyframeModelTest, TrimTimePlaybackNormal) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1, 1, 1));
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(1,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(1,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePlaybackSlow) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1, 1, 0.5));
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(1,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
  EXPECT_EQ(1,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(3.0))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePlaybackFast) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1, 4, 2));
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(1,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(2,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(3,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                .InSecondsF());
  EXPECT_EQ(4,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
  EXPECT_EQ(4,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePlaybackNormalReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1, 2, -1));
  // Total active duration is 2s.
  // With playback_rate = -1 and start_time = 0s, local_time = -monotonic_time.
  // The animation runs backward from end to start over the monotonic time
  // interval [-2s, 0s].
  //
  // Diagram:
  // monotonic_time: -2.5     -2.0     -1.5     -1.0     -0.5      0.0      0.5
  // local_time:      2.5      2.0      1.5      1.0      0.5      0.0     -0.5
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-2.5))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-2.0))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.5))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-0.5))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePlaybackSlowReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(
      CreateKeyframeModel(1, 2, -0.5));
  // Total active duration is 2s.
  // With playback_rate = -0.5 and start_time = 0s, local_time = -0.5 *
  // monotonic_time. The animation runs backward from end to start over the
  // monotonic time interval [-4s, 0s].
  //
  // Diagram:
  // monotonic_time:
  // -4.5   -4.0   -3.5   -3.0   -2.5   -2.0   -1.5   -1.0   -0.5    0.0    0.5
  // local_time:
  // 2.25   2.0    1.75   1.5    1.25   1.0    0.75   0.5    0.25    0.0   -0.25
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-4.5))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-4.0))
                .InSecondsF());
  EXPECT_EQ(1.75,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-3.5))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-3.0))
                .InSecondsF());
  EXPECT_EQ(1.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-2.5))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-2.0))
                .InSecondsF());
  EXPECT_EQ(0.75,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.5))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-0.5))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePlaybackFastReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1, 2, -2));
  // Total active duration is 2s.
  // With playback_rate = -2 and start_time = 0s, local_time = -2.0 *
  // monotonic_time. The animation runs backward from end to start over the
  // monotonic time interval [-1s, 0s].
  //
  // Diagram:
  // monotonic_time: -1.5   -1.25  -1.0   -0.75  -0.5   -0.25   0.0    0.25 0.5
  // local_time:      3.0    2.5    2.0    1.5    1.0    0.5    0.0   -0.5 -1.0
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.5))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.25))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-0.75))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-0.5))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-0.25))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePlaybackFastInfiniteIterations) {
  std::unique_ptr<KeyframeModel> keyframe_model(
      CreateKeyframeModel(std::numeric_limits<double>::infinity(), 4, 4));
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(2,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(2,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                .InSecondsF());
  EXPECT_EQ(
      0, keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1000.0))
             .InSecondsF());
  EXPECT_EQ(
      2, keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1000.5))
             .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePlaybackNormalDoubleReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1, 1, -1));
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  // Total active duration is 1s.
  // With playback_rate = -1 and start_time = 0s, local_time = -1.0 *
  // monotonic_time. The animation runs backward from end to start over the
  // monotonic time interval [-1s, 0s]. Since direction is REVERSE, the
  // iteration progress is flipped again, making it run from 0 to 1 over the
  // active interval.
  //
  // Diagram:
  // monotonic_time: -1.5   -1.0   -0.5    0.0    0.5
  // local_time:      1.5    1.0    0.5    0.0   -0.5
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.5))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-0.5))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePlaybackFastDoubleReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1, 4, -2));
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  // Total active duration is 4s.
  // With playback_rate = -2 and start_time = 0s, local_time = -2.0 *
  // monotonic_time. The animation runs backward from end to start over the
  // monotonic time interval [-2s, 0s]. Since direction is REVERSE, the
  // iteration progress is flipped again, making it run from 0 to 4 over the
  // active interval.
  //
  // Diagram:
  // monotonic_time: -2.5   -2.0   -1.5   -1.0   -0.5    0.0    0.5
  // local_time:      5.0    4.0    3.0    2.0    1.0    0.0   -1.0
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-2.5))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-2.0))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.5))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(3.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-0.5))
                .InSecondsF());
  EXPECT_EQ(4.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(4.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeAlternateTwoIterationsPlaybackFast) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(2, 2, 2));
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_NORMAL);
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeAlternateTwoIterationsPlaybackFastReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(2, 2, 2));
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_REVERSE);
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25))
                .InSecondsF());
}

TEST(KeyframeModelTest,
     TrimTimeAlternateTwoIterationsPlaybackFastDoubleReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(2, 2, -2));
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_REVERSE);
  // Total active duration is 4s (2 iterations of 2s).
  // With playback_rate = -2 and start_time = 0s, the animation runs backward
  // over the monotonic time interval [-2s, 0s].
  //
  // - Negative playback rate makes active time move backwards from active
  //   duration (4s) down to 0s. This means we process iterations in reverse
  //   order: iteration index 1 first, then iteration index 0.
  // - For Direction::ALTERNATE_REVERSE:
  //   - Even indices (0) are reversed.
  //   - Odd indices (1) are normal.
  // - So as monotonic time goes from -2.0s to 0.0s:
  //   - [-2.0s, -1.0s] we are in iteration index 1 (Normal). Negative playback
  //     rate means it plays backwards relative to monotonic time (2.0 to 0.0).
  //   - [-1.0s,  0.0s] we are in iteration index 0 (Reverse). Negative playback
  //     rate means it plays forwards relative to monotonic time (0.0 to 2.0).
  //
  // Diagram:
  // monotonic_time:
  // -2.5  -2.0  -1.75  -1.5  -1.25  -1.0  -0.75  -0.5  -0.25  0.0  0.25
  // local_time:
  //  5.0   4.0   3.5    3.0   2.5    2.0   1.5    1.0   0.5   0.0 -0.5
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-2.5))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-2.0))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.75))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.5))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.25))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-0.75))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-0.5))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-0.25))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                .InSecondsF());
}

TEST(KeyframeModelTest,
     TrimTimeAlternateReverseThreeIterationsPlaybackFastAlternateReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(3, 2, -2));
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_REVERSE);
  // Total active duration is 6s (3 iterations of 2s).
  // With playback_rate = -2 and start_time = 0s, the animation runs backward
  // over the monotonic time interval [-3s, 0s].
  //
  // - Negative playback rate makes time move backwards from active duration
  //   (6s) down to 0s. This means we process iterations in reverse order:
  //   iteration index 2 first, then 1, then 0.
  // - For Direction::ALTERNATE_REVERSE:
  //   - Even indices (0, 2) are reversed.
  //   - Odd indices (1) are normal.
  // - So as monotonic time goes from -3.0s to 0.0s:
  //   - [-3.0s, -2.0s] we are in iteration index 2 (Reversed). Negative
  //     playback rate means it plays forwards relative to monotonic time (0.0
  //     to 2.0).
  //   - [-2.0s, -1.0s] we are in iteration index 1 (Normal). Negative playback
  //     rate means it plays backwards relative to monotonic time (2.0 to 0.0).
  //   - [-1.0s,  0.0s] we are in iteration index 0 (Reversed). Negative
  //     playback rate means it plays forwards relative to monotonic time (0.0
  //     to 2.0).
  //
  // Diagram:
  // monotonic_time:
  // -3.0  -2.5  -2.25  -2.0  -1.75  -1.5  -1.25 -1.0  -0.75  -0.5  -0.25  0.0
  // local_time:
  // 6.0    5.0   4.5    4.0   3.5    3.0   2.5    2.0   1.5    1.0   0.5  0.0
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-3.5))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-3.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-2.75))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-2.5))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-2.25))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-2.0))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.75))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.5))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.25))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-0.75))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-0.5))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-0.25))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                .InSecondsF());
}

TEST(KeyframeModelTest,
     TrimTimeAlternateReverseTwoIterationsPlaybackNormalAlternate) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(2, 2, -1));
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_NORMAL);
  // Total active duration is 4s (2 iterations of 2s).
  // With playback_rate = -1 and start_time = 0s, the animation runs backward
  // over the monotonic time interval [-4s, 0s].
  //
  // - Negative playback rate makes time move backwards from active duration
  //   (4s) down to 0s. This means we process iterations in reverse
  //   order: iteration index 1 first, then 0.
  // - For Direction::ALTERNATE_NORMAL:
  //   - Even indices (0) are normal.
  //   - Odd indices (1) are reversed.
  // - So as monotonic time goes from -4.0s to 0.0s:
  //   - [-4.0s, -2.0s] we are in iteration index 1 (reversed). Negative
  //   playback rate means it plays forwards relative to monotonic time (0.0 to
  //   2.0).
  //   - [-2.0s,  0.0s] we are in iteration index 0 (normal). Negative playback
  //   rate means it plays backwards relative to monotonic time (2.0 to 0.0).
  //
  // Diagram:
  // monotonic_time:
  // -4.5  -4.0  -3.5  -3.0  -2.5  -2.0  -1.5   -1.0  -0.5  0.0   0.5
  // local_time:
  //  4.5   4.0   3.5   3.0   2.5   2.0   1.5    1.0   0.5  0.0  -0.5
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-4.5))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-4.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-3.5))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-3.0))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-2.5))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-2.0))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.5))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-0.5))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeIterationStart) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(2, 1, 1));
  keyframe_model->set_iteration_start(0.5);
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeIterationStartAlternate) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(2, 1, 1));
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_NORMAL);
  keyframe_model->set_iteration_start(0.3);
  EXPECT_EQ(0.3,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0.3,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.8,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.7))
                .InSecondsF());
  EXPECT_EQ(0.7,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.2))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.7))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeIterationStartAlternateThreeIterations) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(3, 1, 1));
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_NORMAL);
  keyframe_model->set_iteration_start(1);
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(3.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(3.5))
                .InSecondsF());
}

TEST(KeyframeModelTest,
     TrimTimeIterationStartAlternateThreeIterationsPlaybackReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(3, 1, -1));
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_NORMAL);
  keyframe_model->set_iteration_start(1);
  // Total active duration is 3s (3 iterations of 1s).
  // With playback_rate = -1, start_time = 0s, and iteration_start = 1,
  // the animation runs backward over the monotonic time interval [-3s, 0s].
  // Note: iteration_start=1 shifts the iteration indices to [1, 2, 3].
  //
  // Relationship between Iteration Index, Playback Rate, and Direction:
  // - Negative playback rate makes time move backwards from active duration
  // (3s)
  //   down to 0s (in local time, 3s down to 0s). This means we process
  //   iterations in reverse chronological order: index 3 first, then 2, then 1.
  // - For Direction::ALTERNATE_NORMAL:
  //   - Even indices (2) are Normal.
  //   - Odd indices (1, 3) are Reverse.
  // - So as monotonic time goes from -3.0s to 0.0s:
  //   - [-3.0s, -2.0s] we are in iteration index 3 (Reverse). Negative playback
  //   rate
  //     means it plays forwards relative to monotonic time (0.0 to 1.0).
  //   - [-2.0s, -1.0s] we are in iteration index 2 (Normal). Negative playback
  //   rate
  //     means it plays backwards relative to monotonic time (1.0 to 0.0).
  //   - [-1.0s,  0.0s] we are in iteration index 1 (Reverse). Negative playback
  //   rate
  //     means it plays forwards relative to monotonic time (0.0 to 1.0).
  //
  // Diagram:
  // monotonic_time: -3.5  -3.0  -2.5  -2.0  -1.5  -1.0  -0.5   0.0   0.5
  // local_time:      3.5   3.0   2.5   2.0   1.5   1.0   0.5   0.0  -0.5
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-3.5))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-3.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-2.5))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-2.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.5))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-0.5))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
}

TEST(KeyframeModelTest, InEffectFillMode) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);
  // Effect before start is not InEffect
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  // Effect at start is InEffect
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  // Effect at end is not InEffect
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(1.0)));

  keyframe_model->set_fill_mode(KeyframeModel::FillMode::FORWARDS);
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(1.0)));

  keyframe_model->set_fill_mode(KeyframeModel::FillMode::BACKWARDS);
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(1.0)));

  keyframe_model->set_fill_mode(KeyframeModel::FillMode::BOTH);
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(1.0)));
}

TEST(KeyframeModelTest, InEffectFillModeNoneWithNegativePlaybackRate) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1, 1, -1));
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(1.0)));

  keyframe_model->set_fill_mode(KeyframeModel::FillMode::FORWARDS);
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(1.0)));

  keyframe_model->set_fill_mode(KeyframeModel::FillMode::BACKWARDS);
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(1.0)));

  keyframe_model->set_fill_mode(KeyframeModel::FillMode::BOTH);
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(1.0)));
}

TEST(KeyframeModelTest, InEffectFillModeWithIterations) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(2, 1));
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(1.0)));
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(2.0)));

  keyframe_model->set_fill_mode(KeyframeModel::FillMode::FORWARDS);
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(2.0)));

  keyframe_model->set_fill_mode(KeyframeModel::FillMode::BACKWARDS);
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(2.0)));

  keyframe_model->set_fill_mode(KeyframeModel::FillMode::BOTH);
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(2.0)));
}

TEST(KeyframeModelTest, InEffectFillModeWithInfiniteIterations) {
  std::unique_ptr<KeyframeModel> keyframe_model(
      CreateKeyframeModel(std::numeric_limits<double>::infinity(), 1));
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(1.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(2.0)));

  keyframe_model->set_fill_mode(KeyframeModel::FillMode::FORWARDS);
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(2.0)));

  keyframe_model->set_fill_mode(KeyframeModel::FillMode::BACKWARDS);
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(2.0)));

  keyframe_model->set_fill_mode(KeyframeModel::FillMode::BOTH);
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(2.0)));
}

TEST(KeyframeModelTest, InEffectReverseWithIterations) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(2, 1));
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);
  // KeyframeModel::direction_ doesn't affect InEffect.
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(1.0)));
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(2.0)));
}

// CalculatePhase uses start_delay_ which may cause integer overflow if not
// handled correctly.
TEST(KeyframeModelTest, CalculatePhaseWithMaxStartDelay) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_start_delay(base::TimeDelta::Max());

  // Setting the start_delay_ to max implies that any local time will fall into
  // the BEFORE phase.
  EXPECT_EQ(keyframe_model->CalculatePhaseForTesting(base::Seconds(1.0)),
            KeyframeModel::Phase::BEFORE);
}

TEST(KeyframeModelTest, ToString) {
  std::unique_ptr<KeyframeModel> keyframe_model = KeyframeModel::Create(
      std::make_unique<FakeFloatAnimationCurve>(15), 42, 73,
      KeyframeModel::TargetPropertyId(TargetProperty::OPACITY));
  EXPECT_EQ(base::StringPrintf(
                "KeyframeModel{id=%d, group=73, target_property_type=4, "
                "custom_property_name=, native_property_type=2, "
                "run_state=WAITING_FOR_TARGET_AVAILABILITY, element_id=(0)}",
                keyframe_model->id()),
            keyframe_model->ToString());
}

TEST(KeyframeModelTest, CustomPropertyKeyframe) {
  std::unique_ptr<KeyframeModel> keyframe_model =
      KeyframeModel::Create(std::make_unique<FakeFloatAnimationCurve>(1), 1, 1,
                            KeyframeModel::TargetPropertyId(
                                TargetProperty::CSS_CUSTOM_PROPERTY, "foo"));
  EXPECT_EQ(keyframe_model->custom_property_name(), "foo");
}

TEST(KeyframeModelTest, NonCustomPropertyKeyframe) {
  std::unique_ptr<KeyframeModel> keyframe_model = KeyframeModel::Create(
      std::make_unique<FakeFloatAnimationCurve>(1), 1, 1,
      KeyframeModel::TargetPropertyId(TargetProperty::TRANSFORM));
  EXPECT_EQ(keyframe_model->custom_property_name(), "");
}

}  // namespace
}  // namespace cc
