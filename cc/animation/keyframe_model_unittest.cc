// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/keyframe_model.h"

#include "base/strings/stringprintf.h"
#include "cc/test/animation_test_common.h"
#include "cc/trees/target_property.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

using base::TimeDelta;

static base::TimeTicks TicksFromSecondsF(double seconds) {
  return base::TimeTicks() + base::TimeDelta::FromSecondsD(seconds);
}

std::unique_ptr<KeyframeModel> CreateKeyframeModel(double iterations,
                                                   double duration,
                                                   double playback_rate) {
  std::unique_ptr<KeyframeModel> to_return(
      KeyframeModel::Create(std::make_unique<FakeFloatAnimationCurve>(duration),
                            0, 1, TargetProperty::OPACITY));
  to_return->set_iterations(iterations);
  to_return->set_playback_rate(playback_rate);
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
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(-1));
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
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(-1));
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
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(-1));
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
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(-1));
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

TEST(KeyframeModelTest, TrimTimeTimeOffset) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_time_offset(TimeDelta::FromMilliseconds(4000));
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

TEST(KeyframeModelTest, TrimTimeTimeOffsetReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_time_offset(TimeDelta::FromMilliseconds(4000));
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

TEST(KeyframeModelTest, TrimTimeNegativeTimeOffset) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_time_offset(TimeDelta::FromMilliseconds(-4000));

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
}

TEST(KeyframeModelTest, TrimTimeNegativeTimeOffsetReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_time_offset(TimeDelta::FromMilliseconds(-4000));
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
}

TEST(KeyframeModelTest, TrimTimePauseBasic) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);

  keyframe_model->Pause(base::TimeDelta::FromSecondsD(0.5));
  // When paused, the time returned is always the pause time
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
  // Pause time is in local time so delay should apply on top of it.
  keyframe_model->set_time_offset(TimeDelta::FromSecondsD(-0.2));
  keyframe_model->Pause(base::TimeDelta::FromSecondsD(0.5));
  EXPECT_EQ(0.3,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.1))
                .InSecondsF());

  keyframe_model->set_time_offset(TimeDelta::FromSecondsD(0.2));
  keyframe_model->Pause(base::TimeDelta::FromSecondsD(0.5));
  EXPECT_EQ(0.7,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.1))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePauseNotAffectedByStartTime) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);
  // Pause time is in local time so start time should not affect it.
  keyframe_model->set_start_time(TicksFromSecondsF(0.2));
  keyframe_model->Pause(base::TimeDelta::FromSecondsD(0.5));
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.1))
                .InSecondsF());

  keyframe_model->set_start_time(TicksFromSecondsF(0.4));
  keyframe_model->Pause(base::TimeDelta::FromSecondsD(0.5));
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.1))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePauseResume) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->SetRunState(KeyframeModel::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.4,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.4))
                .InSecondsF());
  keyframe_model->Pause(base::TimeDelta::FromSecondsD(0.5));
  EXPECT_EQ(
      0.5, keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0))
               .InSecondsF());
  keyframe_model->SetRunState(KeyframeModel::RUNNING,
                              TicksFromSecondsF(1024.0));
  EXPECT_EQ(
      0.5, keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0))
               .InSecondsF());
  EXPECT_EQ(
      1, keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.5))
             .InSecondsF());
  keyframe_model->Pause(base::TimeDelta::FromSecondsD(0.6));
  EXPECT_EQ(
      0.6, keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2000.0))
               .InSecondsF());
  keyframe_model->SetRunState(KeyframeModel::RUNNING,
                              TicksFromSecondsF(2000.0));
  EXPECT_EQ(
      0.7, keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2000.1))
               .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePauseResumeReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  keyframe_model->SetRunState(KeyframeModel::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  keyframe_model->Pause(base::TimeDelta::FromSecondsD(0.25));
  EXPECT_EQ(0.75, keyframe_model
                      ->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0))
                      .InSecondsF());
  keyframe_model->SetRunState(KeyframeModel::RUNNING,
                              TicksFromSecondsF(1024.0));
  EXPECT_EQ(0.75, keyframe_model
                      ->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0))
                      .InSecondsF());
  EXPECT_EQ(
      0, keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.75))
             .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimeZeroDuration) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(0, 0));
  keyframe_model->SetRunState(KeyframeModel::RUNNING, TicksFromSecondsF(0.0));
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

TEST(KeyframeModelTest, TrimTimeStarting) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1, 5.0));
  keyframe_model->SetRunState(KeyframeModel::STARTING, TicksFromSecondsF(0.0));
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  keyframe_model->set_time_offset(TimeDelta::FromMilliseconds(2000));
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  keyframe_model->set_start_time(TicksFromSecondsF(1.0));
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

TEST(KeyframeModelTest, TrimTimeNeedsSynchronizedStartTime) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1, 5.0));
  keyframe_model->SetRunState(KeyframeModel::RUNNING, TicksFromSecondsF(0.0));
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
  keyframe_model->set_time_offset(TimeDelta::FromMilliseconds(2000));
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  keyframe_model->set_start_time(TicksFromSecondsF(1.0));
  keyframe_model->set_needs_synchronized_start_time(false);
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
  keyframe_model->SetRunState(KeyframeModel::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(1.0)));
}

TEST(KeyframeModelTest, IsFinishedAtOneIteration) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->SetRunState(KeyframeModel::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(-1.0)));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(1.0)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(2.0)));
}

TEST(KeyframeModelTest, IsFinishedAtInfiniteIterations) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(-1));
  keyframe_model->SetRunState(KeyframeModel::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.5)));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(1.0)));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(1.5)));
}

TEST(KeyframeModelTest, IsFinishedNegativeTimeOffset) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_time_offset(TimeDelta::FromMilliseconds(-500));
  keyframe_model->SetRunState(KeyframeModel::RUNNING, TicksFromSecondsF(0.0));

  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(-1.0)));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.5)));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(1.0)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(1.5)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(2.0)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(2.5)));
}

TEST(KeyframeModelTest, IsFinishedPositiveTimeOffset) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_time_offset(TimeDelta::FromMilliseconds(500));
  keyframe_model->SetRunState(KeyframeModel::RUNNING, TicksFromSecondsF(0.0));

  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(-1.0)));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.5)));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(1.0)));
}

TEST(KeyframeModelTest, IsFinishedAtNotRunning) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(0));
  keyframe_model->SetRunState(KeyframeModel::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  keyframe_model->SetRunState(KeyframeModel::PAUSED, TicksFromSecondsF(0.0));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  keyframe_model->SetRunState(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
                              TicksFromSecondsF(0.0));
  EXPECT_FALSE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  keyframe_model->SetRunState(KeyframeModel::FINISHED, TicksFromSecondsF(0.0));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
  keyframe_model->SetRunState(KeyframeModel::ABORTED, TicksFromSecondsF(0.0));
  EXPECT_TRUE(keyframe_model->IsFinishedAt(TicksFromSecondsF(0.0)));
}

TEST(KeyframeModelTest, IsFinished) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->SetRunState(KeyframeModel::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_FALSE(keyframe_model->is_finished());
  keyframe_model->SetRunState(KeyframeModel::PAUSED, TicksFromSecondsF(0.0));
  EXPECT_FALSE(keyframe_model->is_finished());
  keyframe_model->SetRunState(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
                              TicksFromSecondsF(0.0));
  EXPECT_FALSE(keyframe_model->is_finished());
  keyframe_model->SetRunState(KeyframeModel::FINISHED, TicksFromSecondsF(0.0));
  EXPECT_TRUE(keyframe_model->is_finished());
  keyframe_model->SetRunState(KeyframeModel::ABORTED, TicksFromSecondsF(0.0));
  EXPECT_TRUE(keyframe_model->is_finished());
}

TEST(KeyframeModelTest, IsFinishedNeedsSynchronizedStartTime) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->SetRunState(KeyframeModel::RUNNING, TicksFromSecondsF(2.0));
  EXPECT_FALSE(keyframe_model->is_finished());
  keyframe_model->SetRunState(KeyframeModel::PAUSED, TicksFromSecondsF(2.0));
  EXPECT_FALSE(keyframe_model->is_finished());
  keyframe_model->SetRunState(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
                              TicksFromSecondsF(2.0));
  EXPECT_FALSE(keyframe_model->is_finished());
  keyframe_model->SetRunState(KeyframeModel::FINISHED, TicksFromSecondsF(0.0));
  EXPECT_TRUE(keyframe_model->is_finished());
  keyframe_model->SetRunState(KeyframeModel::ABORTED, TicksFromSecondsF(0.0));
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
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(1,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePlaybackSlowReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(
      CreateKeyframeModel(1, 2, -0.5));
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0))
                .InSecondsF());
  EXPECT_EQ(1.75,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(1.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                .InSecondsF());
  EXPECT_EQ(1,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
  EXPECT_EQ(0.75,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(3))
                .InSecondsF());
  EXPECT_EQ(0.25,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(3.5))
                .InSecondsF());
  EXPECT_EQ(0, keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(4))
                   .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(4.5))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePlaybackFastReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1, 2, -2));
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                .InSecondsF());
  EXPECT_EQ(1,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                .InSecondsF());
}

TEST(KeyframeModelTest, TrimTimePlaybackFastInfiniteIterations) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(-1, 4, 4));
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

TEST(KeyframeModelTest, TrimTimePlaybackFastDoubleReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1, 4, -2));
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
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
     TrimTimeAlternateReverseThreeIterationsPlaybackFastAlternateReverse) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(3, 2, -2));
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_REVERSE);
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
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.75))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(3.0))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(3.25))
                .InSecondsF());
}

TEST(KeyframeModelTest,
     TrimTimeAlternateReverseTwoIterationsPlaybackNormalAlternate) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(2, 2, -1));
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_NORMAL);
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                .InSecondsF());
  EXPECT_EQ(2.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
  EXPECT_EQ(1.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(3.0))
                .InSecondsF());
  EXPECT_EQ(0.5,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(3.5))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(4.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(4.5))
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
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                .InSecondsF());
  EXPECT_EQ(0.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(3.0))
                .InSecondsF());
  EXPECT_EQ(1.0,
            keyframe_model->TrimTimeToCurrentIteration(TicksFromSecondsF(3.5))
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
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(1.0)));

  keyframe_model->set_fill_mode(KeyframeModel::FillMode::FORWARDS);
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_FALSE(keyframe_model->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(keyframe_model->InEffect(TicksFromSecondsF(1.0)));

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
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(-1, 1));
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

// CalculatePhase uses -time_offset_ which may cause integer overflow when
// time_offset_ is set to min(). This test makes sure that the code handles it
// correctly. See https://crbug.com/921454.
TEST(KeyframeModelTest, CalculatePhaseWithMinTimeOffset) {
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(1));
  keyframe_model->set_time_offset(
      TimeDelta::FromMilliseconds(std::numeric_limits<int64_t>::min()));

  // Setting the time_offset_ to min implies that the effect has a max start
  // delay and any local time will fall into the BEFORE phase.
  EXPECT_EQ(
      keyframe_model->CalculatePhaseForTesting(TimeDelta::FromSecondsD(1.0)),
      KeyframeModel::Phase::BEFORE);
}

TEST(KeyframeModelTest, ToString) {
  std::unique_ptr<KeyframeModel> keyframe_model =
      KeyframeModel::Create(std::make_unique<FakeFloatAnimationCurve>(15), 42,
                            73, TargetProperty::OPACITY);
  EXPECT_EQ(
      base::StringPrintf("KeyframeModel{id=%d, group=73, target_property_id=1, "
                         "run_state=WAITING_FOR_TARGET_AVAILABILITY}",
                         keyframe_model->id()),
      keyframe_model->ToString());
}

TEST(KeyframeModelTest, CustomPropertyKeyframe) {
  std::unique_ptr<KeyframeModel> keyframe_model =
      KeyframeModel::Create(std::make_unique<FakeFloatAnimationCurve>(1), 1, 1,
                            TargetProperty::CSS_CUSTOM_PROPERTY, "foo");
  EXPECT_EQ(keyframe_model->custom_property_name(), "foo");
}

TEST(KeyframeModelTest, NonCustomPropertyKeyframe) {
  std::unique_ptr<KeyframeModel> keyframe_model =
      KeyframeModel::Create(std::make_unique<FakeFloatAnimationCurve>(1), 1, 1,
                            TargetProperty::TRANSFORM);
  EXPECT_EQ(keyframe_model->custom_property_name(), "");
}

}  // namespace
}  // namespace cc
