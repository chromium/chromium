// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/task_switch_time_tracker.h"

#include <memory>
#include <string>

#include "ash/metrics/task_switch_time_tracker_test_api.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// A dummy histogram name.
const std::string kHistogramName = "Dummy.Histogram";

}  // namespace

class TaskSwitchTimeTrackerTest : public testing::Test {
 public:
  TaskSwitchTimeTrackerTest();

  TaskSwitchTimeTrackerTest(const TaskSwitchTimeTrackerTest&) = delete;
  TaskSwitchTimeTrackerTest& operator=(const TaskSwitchTimeTrackerTest&) =
      delete;

  ~TaskSwitchTimeTrackerTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Wrapper to the test targets OnTaskSwitch() method.
  void OnTaskSwitch();

  TaskSwitchTimeTracker* time_tracker() {
    return time_tracker_test_api_->time_tracker();
  }

 protected:
  // Used to verify recorded histogram data.
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  // A Test API that wraps the test target.
  std::unique_ptr<TaskSwitchTimeTrackerTestAPI> time_tracker_test_api_;
};

TaskSwitchTimeTrackerTest::TaskSwitchTimeTrackerTest() = default;

TaskSwitchTimeTrackerTest::~TaskSwitchTimeTrackerTest() = default;

void TaskSwitchTimeTrackerTest::SetUp() {
  testing::Test::SetUp();

  histogram_tester_ = std::make_unique<base::HistogramTester>();
  time_tracker_test_api_ =
      std::make_unique<TaskSwitchTimeTrackerTestAPI>(kHistogramName);
  // The TaskSwitchTimeTracker interprets a value of base::TimeTicks() as if the
  // |last_action_time_| has not been set.
  time_tracker_test_api_->Advance(base::Milliseconds(1));
}

void TaskSwitchTimeTrackerTest::TearDown() {
  testing::Test::TearDown();
  time_tracker_test_api_.reset();
  histogram_tester_.reset();
}

void TaskSwitchTimeTrackerTest::OnTaskSwitch() {
  time_tracker()->OnTaskSwitch();
}

// Verifies TaskSwitchTimeTracker::HasLastActionTime() returns false after
// construction.
TEST_F(TaskSwitchTimeTrackerTest,
       HasLastActionTimeShouldBeFalseAfterConstruction) {
  EXPECT_FALSE(time_tracker_test_api_->HasLastActionTime());
}

// Verifies TaskSwitchTimeTracker::HasLastActionTime() returns true after the
// first call to TaskSwitchTimeTracker::OnTaskSwitch() and no histogram data was
// recorded.
TEST_F(TaskSwitchTimeTrackerTest,
       HasLastActionTimeShouldBeTrueAfterOnTaskSwitch) {
  OnTaskSwitch();
  histogram_tester_->ExpectTotalCount(kHistogramName, 0);
}

// Verfies that the histogram data is recorded in the correct buckets.
TEST_F(TaskSwitchTimeTrackerTest, RecordAfterTwoTaskSwitches) {
  OnTaskSwitch();

  time_tracker_test_api_->Advance(base::Milliseconds(2));
  OnTaskSwitch();
  histogram_tester_->ExpectTotalCount(kHistogramName, 1);
  histogram_tester_->ExpectBucketCount(kHistogramName, 0, 1);

  time_tracker_test_api_->Advance(base::Seconds(1));
  OnTaskSwitch();
  histogram_tester_->ExpectTotalCount(kHistogramName, 2);
  histogram_tester_->ExpectBucketCount(kHistogramName, 1, 1);
}

}  // namespace ash
