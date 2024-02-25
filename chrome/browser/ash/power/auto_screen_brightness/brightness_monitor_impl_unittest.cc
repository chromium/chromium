// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/brightness_monitor_impl.h"

#include "ash/constants/ash_features.h"
#include "base/memory/ptr_util.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

namespace {

class TestObserver : public BrightnessMonitor::Observer {
 public:
  TestObserver() {}

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override = default;

  // BrightnessMonitor::Observer overrides:
  void OnBrightnessMonitorInitialized(bool success) override {
    status_ = std::optional<BrightnessMonitor::Status>(
        success ? BrightnessMonitor::Status::kSuccess
                : BrightnessMonitor::Status::kDisabled);
  }

  void OnUserBrightnessChanged(double old_brightness_percent,
                               double new_brightness_percent) override {
    old_brightness_percent_ = old_brightness_percent;
    new_brightness_percent_ = new_brightness_percent;
    ++num_brightness_changes_;
  }

  void OnUserBrightnessChangeRequested() override {
    ++num_user_brightness_change_requested_;
  }

  double old_brightness_percent() { return old_brightness_percent_; }
  double new_brightness_percent() { return new_brightness_percent_; }
  int num_brightness_changes() { return num_brightness_changes_; }
  int num_user_brightness_change_requested() {
    return num_user_brightness_change_requested_;
  }

  BrightnessMonitor::Status status() {
    CHECK(status_);
    return status_.value();
  }

 private:
  double old_brightness_percent_ = -1;
  double new_brightness_percent_ = -1;
  int num_brightness_changes_ = 0;
  int num_user_brightness_change_requested_ = 0;
  std::optional<BrightnessMonitor::Status> status_;
};
}  // namespace

class BrightnessMonitorImplTest : public testing::Test {
 public:
  BrightnessMonitorImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  BrightnessMonitorImplTest(const BrightnessMonitorImplTest&) = delete;
  BrightnessMonitorImplTest& operator=(const BrightnessMonitorImplTest&) =
      delete;

  ~BrightnessMonitorImplTest() override {}

  // testing::Test:
  void SetUp() override { chromeos::PowerManagerClient::InitializeFake(); }

  void TearDown() override {
    test_observer_.reset();
    monitor_.reset();
    chromeos::PowerManagerClient::Shutdown();
    base::ThreadPoolInstance::Get()->FlushForTesting();
  }

  // Creates and initializes |monitor_| and optionally sets initial brightness
  // on fake power manager client.
  void SetUpBrightnessMonitor(
      double init_brightness,
      const std::map<std::string, std::string>& params = {}) {
    if (init_brightness >= 0) {
      power_manager::SetBacklightBrightnessRequest request;
      request.set_percent(init_brightness);
      chromeos::PowerManagerClient::Get()->SetScreenBrightness(request);
    }

    if (!params.empty()) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kAutoScreenBrightness, params);
    }

    monitor_ = std::make_unique<BrightnessMonitorImpl>();
    monitor_->Init();
    test_observer_ = std::make_unique<TestObserver>();
    monitor_->AddObserver(test_observer_.get());
    task_environment_.RunUntilIdle();
  }

 protected:
  void ReportBrightnessChangeEvent(
      double level,
      power_manager::BacklightBrightnessChange_Cause cause) {
    power_manager::BacklightBrightnessChange change;
    change.set_percent(level);
    change.set_cause(cause);
    static_cast<chromeos::FakePowerManagerClient*>(
        chromeos::PowerManagerClient::Get())
        ->SendScreenBrightnessChanged(change);
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<BrightnessMonitorImpl> monitor_;
  std::unique_ptr<TestObserver> test_observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BrightnessMonitorImplTest, ReportSuccess) {
  SetUpBrightnessMonitor(10);
  task_environment_.FastForwardUntilNoTasksRemain();

  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.BrightnessMonitorStatus",
      static_cast<int>(BrightnessMonitor::Status::kSuccess), 1);
}

TEST_F(BrightnessMonitorImplTest, ReportDisabled) {
  SetUpBrightnessMonitor(-1);
  task_environment_.FastForwardUntilNoTasksRemain();

  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.BrightnessMonitorStatus",
      static_cast<int>(BrightnessMonitor::Status::kDisabled), 1);
}

// PowerManagerClient is not set up to return initial brightness, hence
// Status is kDisabled.
TEST_F(BrightnessMonitorImplTest, PowerManagerClientBrightnessUnset) {
  // Do not set initial brightess in FakePowerManagerClient.
  SetUpBrightnessMonitor(-1);
  EXPECT_EQ(BrightnessMonitor::Status::kDisabled, test_observer_->status());

  // User request will be ignored.
  ReportBrightnessChangeEvent(
      20, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  task_environment_.FastForwardBy(
      monitor_->GetBrightnessSampleDelayForTesting() * 2);
  EXPECT_EQ(0, test_observer_->num_brightness_changes());
  EXPECT_EQ(0, test_observer_->num_user_brightness_change_requested());
}

// Two user brightness adjustments are received with a gap shorter than
// brightness sample delay, hence final brightness is recorded.
TEST_F(BrightnessMonitorImplTest, TwoUserAdjustmentsShortGap) {
  SetUpBrightnessMonitor(10);
  EXPECT_EQ(BrightnessMonitor::Status::kSuccess, test_observer_->status());
  EXPECT_EQ(0, test_observer_->num_brightness_changes());
  EXPECT_EQ(0, test_observer_->num_user_brightness_change_requested());

  // First user-requested brightness adjustment.
  ReportBrightnessChangeEvent(
      20, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  EXPECT_EQ(1, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(0, test_observer_->num_brightness_changes());

  task_environment_.FastForwardBy(
      monitor_->GetBrightnessSampleDelayForTesting() / 2);
  // Second user-requested brightness adjustment.
  ReportBrightnessChangeEvent(
      30, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  EXPECT_EQ(2, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(0, test_observer_->num_brightness_changes());

  task_environment_.FastForwardBy(
      monitor_->GetBrightnessSampleDelayForTesting() * 2);

  EXPECT_EQ(2, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(1, test_observer_->num_brightness_changes());
  EXPECT_DOUBLE_EQ(10, test_observer_->old_brightness_percent());
  EXPECT_DOUBLE_EQ(30, test_observer_->new_brightness_percent());
}

// Two user brightness adjustments are received with a gap longer than
// brightness sample delay, hence two brightness changes are recorded.
TEST_F(BrightnessMonitorImplTest, TwoUserAdjustmentsLongGap) {
  SetUpBrightnessMonitor(10, {{"brightness_sample_delay_seconds", "2"}});
  EXPECT_EQ(BrightnessMonitor::Status::kSuccess, test_observer_->status());
  EXPECT_EQ(0, test_observer_->num_brightness_changes());
  EXPECT_EQ(0, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(2, monitor_->GetBrightnessSampleDelayForTesting().InSeconds());

  // First user-requested brightness adjustment.
  ReportBrightnessChangeEvent(
      20, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  EXPECT_EQ(1, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(0, test_observer_->num_brightness_changes());

  task_environment_.FastForwardBy(
      monitor_->GetBrightnessSampleDelayForTesting() * 2);
  EXPECT_EQ(1, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(1, test_observer_->num_brightness_changes());
  EXPECT_DOUBLE_EQ(10, test_observer_->old_brightness_percent());
  EXPECT_DOUBLE_EQ(20, test_observer_->new_brightness_percent());

  // Second user-requested brightness adjustment.
  ReportBrightnessChangeEvent(
      30, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  EXPECT_EQ(2, test_observer_->num_user_brightness_change_requested());
  task_environment_.FastForwardBy(
      monitor_->GetBrightnessSampleDelayForTesting() * 2);

  EXPECT_EQ(2, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(2, test_observer_->num_brightness_changes());
  EXPECT_DOUBLE_EQ(20, test_observer_->old_brightness_percent());
  EXPECT_DOUBLE_EQ(30, test_observer_->new_brightness_percent());
}

// A brightness change not triggered by user request, followed by user requested
// change. The gap between the two is shorter than brightness sample delay.
TEST_F(BrightnessMonitorImplTest, NonUserFollowedByUserShortGap) {
  SetUpBrightnessMonitor(10);

  // Non-user.
  ReportBrightnessChangeEvent(
      20, power_manager::BacklightBrightnessChange_Cause_USER_ACTIVITY);
  EXPECT_EQ(0, test_observer_->num_user_brightness_change_requested());
  task_environment_.FastForwardBy(
      monitor_->GetBrightnessSampleDelayForTesting() / 2);
  EXPECT_EQ(0, test_observer_->num_brightness_changes());

  // User.
  ReportBrightnessChangeEvent(
      30, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  EXPECT_EQ(1, test_observer_->num_user_brightness_change_requested());
  task_environment_.FastForwardBy(
      monitor_->GetBrightnessSampleDelayForTesting() * 2);

  EXPECT_EQ(1, test_observer_->num_brightness_changes());
  EXPECT_DOUBLE_EQ(20, test_observer_->old_brightness_percent());
  EXPECT_DOUBLE_EQ(30, test_observer_->new_brightness_percent());
}

// A brightness change not triggered by user request, followed by user requested
// change. The gap between the two is longer than brightness sample delay.
TEST_F(BrightnessMonitorImplTest, NonUserFollowedByUserLongGap) {
  SetUpBrightnessMonitor(10);

  ReportBrightnessChangeEvent(
      20, power_manager::BacklightBrightnessChange_Cause_USER_ACTIVITY);
  EXPECT_EQ(0, test_observer_->num_user_brightness_change_requested());
  task_environment_.FastForwardBy(
      monitor_->GetBrightnessSampleDelayForTesting() * 2);
  EXPECT_EQ(0, test_observer_->num_brightness_changes());

  ReportBrightnessChangeEvent(
      30, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  EXPECT_EQ(1, test_observer_->num_user_brightness_change_requested());
  task_environment_.FastForwardBy(
      monitor_->GetBrightnessSampleDelayForTesting() * 2);

  EXPECT_EQ(1, test_observer_->num_brightness_changes());
  EXPECT_DOUBLE_EQ(20, test_observer_->old_brightness_percent());
  EXPECT_DOUBLE_EQ(30, test_observer_->new_brightness_percent());
}

// A user requested brightness change is received, another non-user triggered
// change is received before timer times out. Followed by another user requested
// change.
TEST_F(BrightnessMonitorImplTest, UserAdjustmentsSeparatedByNonUser) {
  SetUpBrightnessMonitor(10);

  // User request.
  ReportBrightnessChangeEvent(
      20, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  EXPECT_EQ(1, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(0, test_observer_->num_brightness_changes());

  // Non-user.
  task_environment_.FastForwardBy(
      monitor_->GetBrightnessSampleDelayForTesting() / 2);
  ReportBrightnessChangeEvent(
      30, power_manager::BacklightBrightnessChange_Cause_USER_ACTIVITY);
  EXPECT_EQ(1, test_observer_->num_user_brightness_change_requested());
  // Timer times out immediately to send out brightness change notification.
  EXPECT_EQ(1, test_observer_->num_brightness_changes());
  EXPECT_DOUBLE_EQ(10, test_observer_->old_brightness_percent());
  EXPECT_DOUBLE_EQ(20, test_observer_->new_brightness_percent());

  // Another user request.
  ReportBrightnessChangeEvent(
      40, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  EXPECT_EQ(2, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(1, test_observer_->num_brightness_changes());

  task_environment_.FastForwardBy(
      monitor_->GetBrightnessSampleDelayForTesting() * 2);
  EXPECT_EQ(2, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(2, test_observer_->num_brightness_changes());
  EXPECT_DOUBLE_EQ(30, test_observer_->old_brightness_percent());
  EXPECT_DOUBLE_EQ(40, test_observer_->new_brightness_percent());
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
