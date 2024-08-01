// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/trees/frame_rate_estimator.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class FrameRateEstimatorTest : public testing::Test {
 public:
  FrameRateEstimatorTest() = default;
  ~FrameRateEstimatorTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /* enabled_features*/ {features::
                                   kThrottleFrameRateOnManyDidNotProduceFrame},
        /* disabled_features*/ {});
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    estimator_ = std::make_unique<FrameRateEstimator>(task_runner_.get());
  }

  void TearDown() override {
    estimator_.reset();
    task_runner_.reset();
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SimpleTestTickClock test_tick_clock_;
  std::unique_ptr<FrameRateEstimator> estimator_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FrameRateEstimatorTest, ToggleEstimationEnabled) {
  EXPECT_EQ(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());
  estimator_->SetVideoConferenceMode(true);
  EXPECT_NE(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());
  estimator_->SetVideoConferenceMode(false);
  EXPECT_EQ(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());
}

TEST_F(FrameRateEstimatorTest, FrameHistoryUsed) {
  estimator_->SetVideoConferenceMode(true);
  EXPECT_NE(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());
  base::TimeTicks time;
  for (int i = 0; i < 10; ++i) {
    estimator_->WillDraw(time);
    time += viz::BeginFrameArgs::DefaultInterval();
  }
  EXPECT_EQ(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());

  estimator_->WillDraw(time + (3 * viz::BeginFrameArgs::DefaultInterval()));
  EXPECT_NE(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());
}

TEST_F(FrameRateEstimatorTest, InputPriorityMode) {
  estimator_->SetVideoConferenceMode(true);
  estimator_->NotifyInputEvent();
  EXPECT_EQ(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());

  task_runner_->RunUntilIdle();
  EXPECT_NE(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());
}

TEST_F(FrameRateEstimatorTest, RafAtHalfFps) {
  estimator_->SetVideoConferenceMode(true);
  // Recorded rAF intervals at 30 fps.
  const base::TimeDelta kIntervals[] = {
      base::Microseconds(33425), base::Microseconds(33298),
      base::Microseconds(33396), base::Microseconds(33339),
      base::Microseconds(33431), base::Microseconds(33320),
      base::Microseconds(33364), base::Microseconds(33360)};
  const base::TimeDelta kIntervalForHalfFps =
      viz::BeginFrameArgs::DefaultInterval() * 2;
  base::TimeTicks time;
  for (size_t i = 0; i <= std::size(kIntervals); ++i) {
    estimator_->WillDraw(time);
    EXPECT_EQ(kIntervalForHalfFps, estimator_->GetPreferredInterval());
    if (i < std::size(kIntervals))
      time += kIntervals[i];
  }
}

TEST_F(FrameRateEstimatorTest, ThrottleWhenManyDidNotProduceFrame) {
  const base::TimeDelta kIntervalForHalfFps =
      viz::BeginFrameArgs::DefaultInterval() * 2;
  test_tick_clock_.Advance(viz::BeginFrameArgs::DefaultInterval());
  estimator_->WillDraw(test_tick_clock_.NowTicks());
  EXPECT_EQ(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());

  // Consecutive DidNotProduceFrame.
  size_t consecutive_num =
      static_cast<size_t>(
          features::kNumDidNotProduceFrameBeforeThrottle.Get()) +
      1;
  for (size_t i = 0; i < consecutive_num; ++i) {
    estimator_->DidNotProduceFrame();
  }
  EXPECT_EQ(estimator_->GetPreferredInterval(), kIntervalForHalfFps);
  // Stop throttling if draw.
  test_tick_clock_.Advance(viz::BeginFrameArgs::DefaultInterval());
  estimator_->WillDraw(test_tick_clock_.NowTicks());
  EXPECT_EQ(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());
}

TEST_F(FrameRateEstimatorTest, ManyDidNotProduceFrameWhenInput) {
  const base::TimeDelta kIntervalForHalfFps =
      viz::BeginFrameArgs::DefaultInterval() * 2;
  test_tick_clock_.Advance(viz::BeginFrameArgs::DefaultInterval());
  estimator_->WillDraw(test_tick_clock_.NowTicks());
  EXPECT_EQ(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());

  // Consecutive DidNotProduceFrame.
  size_t consecutive_num =
      static_cast<size_t>(
          features::kNumDidNotProduceFrameBeforeThrottle.Get()) +
      1;
  for (size_t i = 0; i < consecutive_num; ++i) {
    estimator_->DidNotProduceFrame();
  }
  EXPECT_EQ(estimator_->GetPreferredInterval(), kIntervalForHalfFps);
  // No throttlling if input event happens.
  estimator_->NotifyInputEvent();
  EXPECT_EQ(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());
}

}  // namespace
}  // namespace cc
