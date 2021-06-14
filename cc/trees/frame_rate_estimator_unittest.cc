// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/frame_rate_estimator.h"

#include "base/cxx17_backports.h"
#include "base/test/test_simple_task_runner.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class FrameRateEstimatorTest : public testing::Test {
 public:
  FrameRateEstimatorTest() = default;
  ~FrameRateEstimatorTest() override = default;

  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    estimator_ = std::make_unique<FrameRateEstimator>(task_runner_.get());
  }

  void TearDown() override {
    estimator_.reset();
    task_runner_.reset();
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<FrameRateEstimator> estimator_;
};

TEST_F(FrameRateEstimatorTest, ToggleEstimationEnabled) {
  EXPECT_EQ(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());
  estimator_->SetFrameEstimationEnabled(true);
  EXPECT_NE(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());
  estimator_->SetFrameEstimationEnabled(false);
  EXPECT_EQ(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());
}

TEST_F(FrameRateEstimatorTest, FrameHistoryUsed) {
  estimator_->SetFrameEstimationEnabled(true);
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
  estimator_->SetFrameEstimationEnabled(true);
  estimator_->NotifyInputEvent();
  EXPECT_EQ(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());

  task_runner_->RunUntilIdle();
  EXPECT_NE(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());
}

TEST_F(FrameRateEstimatorTest, RafAtHalfFps) {
  estimator_->SetFrameEstimationEnabled(true);
  // Recorded rAF intervals at 30 fps.
  const base::TimeDelta kIntervals[] = {
      base::TimeDelta::FromMicroseconds(33425),
      base::TimeDelta::FromMicroseconds(33298),
      base::TimeDelta::FromMicroseconds(33396),
      base::TimeDelta::FromMicroseconds(33339),
      base::TimeDelta::FromMicroseconds(33431),
      base::TimeDelta::FromMicroseconds(33320),
      base::TimeDelta::FromMicroseconds(33364),
      base::TimeDelta::FromMicroseconds(33360)};
  const base::TimeDelta kIntervalForHalfFps =
      viz::BeginFrameArgs::DefaultInterval() * 2;
  base::TimeTicks time;
  for (size_t i = 0; i <= base::size(kIntervals); ++i) {
    estimator_->WillDraw(time);
    EXPECT_EQ(kIntervalForHalfFps, estimator_->GetPreferredInterval());
    if (i < base::size(kIntervals))
      time += kIntervals[i];
  }
}
}  // namespace
}  // namespace cc
