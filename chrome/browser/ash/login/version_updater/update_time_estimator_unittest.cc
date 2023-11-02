// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/version_updater/update_time_estimator.h"

#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr int kFinalizingTimeInSeconds = 5 * 60;

constexpr base::TimeDelta kTimeAdvanceSeconds10 = base::Seconds(10);
constexpr base::TimeDelta kTimeAdvanceSeconds60 = base::Seconds(60);
constexpr base::TimeDelta kZeroTime = base::TimeDelta();

}  // anonymous namespace

namespace ash {

class UpdateTimeEstimatorUnitTest : public testing::Test {
 public:
  UpdateTimeEstimatorUnitTest() = default;

  UpdateTimeEstimatorUnitTest(const UpdateTimeEstimatorUnitTest&) = delete;
  UpdateTimeEstimatorUnitTest& operator=(const UpdateTimeEstimatorUnitTest&) =
      delete;

  void SetUp() override {
    time_estimator_.set_tick_clock_for_testing(&tick_clock_);
  }

  update_engine::StatusResult CreateStatusResult(update_engine::Operation stage,
                                                 double image_size,
                                                 double progress) {
    update_engine::StatusResult status;
    status.set_current_operation(stage);
    status.set_new_size(image_size);
    status.set_progress(progress);
    return status;
  }

  UpdateTimeEstimator time_estimator_;

  base::SimpleTestTickClock tick_clock_;
};

TEST_F(UpdateTimeEstimatorUnitTest, DownloadingTimeLeft) {
  update_engine::StatusResult status =
      CreateStatusResult(update_engine::Operation::DOWNLOADING, 1.0, 0.0);
  time_estimator_.Update(status);

  tick_clock_.Advance(kTimeAdvanceSeconds10);
  status.set_progress(0.01);
  time_estimator_.Update(status);
  EXPECT_EQ(time_estimator_.GetDownloadTimeLeft().InSeconds(), 990);

  tick_clock_.Advance(kTimeAdvanceSeconds10);
  status.set_progress(0.10);
  time_estimator_.Update(status);
  EXPECT_EQ(time_estimator_.GetDownloadTimeLeft().InSeconds(), 500);
}

TEST_F(UpdateTimeEstimatorUnitTest, TotalTimeLeft) {
  update_engine::StatusResult status =
      CreateStatusResult(update_engine::Operation::FINALIZING, 0.0, 0.0);
  time_estimator_.Update(status);

  tick_clock_.Advance(kTimeAdvanceSeconds10);
  EXPECT_EQ(time_estimator_.GetUpdateStatus().time_left,
            base::Seconds(kFinalizingTimeInSeconds) - kTimeAdvanceSeconds10);

  tick_clock_.Advance(base::Seconds(kFinalizingTimeInSeconds));
  EXPECT_EQ(time_estimator_.GetUpdateStatus().time_left, kZeroTime);
}

TEST_F(UpdateTimeEstimatorUnitTest, DownloadingProgress) {
  update_engine::StatusResult status =
      CreateStatusResult(update_engine::Operation::DOWNLOADING, 1.0, 0.0);
  time_estimator_.Update(status);
  EXPECT_EQ(time_estimator_.GetUpdateStatus().progress, 0);

  tick_clock_.Advance(kTimeAdvanceSeconds10);
  status.set_progress(0.01);
  time_estimator_.Update(status);
  EXPECT_EQ(time_estimator_.GetUpdateStatus().progress, 1);

  tick_clock_.Advance(kTimeAdvanceSeconds10);
  status.set_progress(0.10);
  time_estimator_.Update(status);
  EXPECT_EQ(time_estimator_.GetUpdateStatus().progress, 9);

  status = CreateStatusResult(update_engine::Operation::VERIFYING, 0.0, 0.0);
  time_estimator_.Update(status);
  EXPECT_EQ(time_estimator_.GetUpdateStatus().progress, 90);
  tick_clock_.Advance(kTimeAdvanceSeconds60);
  EXPECT_EQ(time_estimator_.GetUpdateStatus().progress, 91);

  status = CreateStatusResult(update_engine::Operation::FINALIZING, 0.0, 0.0);
  time_estimator_.Update(status);
  EXPECT_EQ(time_estimator_.GetUpdateStatus().progress, 95);
  tick_clock_.Advance(kTimeAdvanceSeconds60);
  EXPECT_EQ(time_estimator_.GetUpdateStatus().progress, 96);
}

}  // namespace ash
