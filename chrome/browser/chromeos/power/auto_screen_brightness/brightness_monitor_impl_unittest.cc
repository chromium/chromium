// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/brightness_monitor_impl.h"

#include "base/memory/ptr_util.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {

class TestObserver : public BrightnessMonitor::Observer {
 public:
  TestObserver() {}
  ~TestObserver() override = default;

  // BrightnessMonitor::Observer overrides:
  void OnBrightnessMonitorInitialized(bool success) override {
    status_ = base::Optional<BrightnessMonitor::Status>(
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
  base::Optional<BrightnessMonitor::Status> status_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};
}  // namespace

class BrightnessMonitorImplTest : public testing::Test {
 public:
  BrightnessMonitorImplTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME) {
    chromeos::DBusThreadManager::GetSetterForTesting()->SetPowerManagerClient(
        std::make_unique<chromeos::FakePowerManagerClient>());
  }

  ~BrightnessMonitorImplTest() override {
    base::TaskScheduler::GetInstance()->FlushForTesting();
  }

  // Creates and initializes |monitor_| and optionally sets initial brightness
  // on fake power manager client.
  void SetUpBrightnessMonitor(double init_brightness) {
    if (init_brightness >= 0) {
      power_manager::SetBacklightBrightnessRequest request;
      request.set_percent(init_brightness);
      chromeos::DBusThreadManager::Get()
          ->GetPowerManagerClient()
          ->SetScreenBrightness(request);
    }

    monitor_ = BrightnessMonitorImpl::CreateForTesting(
        chromeos::DBusThreadManager::Get()->GetPowerManagerClient(),
        base::SequencedTaskRunnerHandle::Get());
    test_observer_ = std::make_unique<TestObserver>();
    monitor_->AddObserver(test_observer_.get());
  }

 protected:
  void ReportBrightnessChangeEvent(
      double level,
      power_manager::BacklightBrightnessChange_Cause cause) {
    power_manager::BacklightBrightnessChange change;
    change.set_percent(level);
    change.set_cause(cause);
    static_cast<chromeos::FakePowerManagerClient*>(
        chromeos::DBusThreadManager::Get()->GetPowerManagerClient())
        ->SendScreenBrightnessChanged(change);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<BrightnessMonitorImpl> monitor_;
  std::unique_ptr<TestObserver> test_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrightnessMonitorImplTest);
};

// PowerManagerClient is not set up to return initial brightness, hence
// Status is kDiabled.
TEST_F(BrightnessMonitorImplTest, PowerManagerClientBrightnessUnset) {
  // Do not set initial brightess in FakePowerManagerClient.
  SetUpBrightnessMonitor(-1);
  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(BrightnessMonitor::Status::kDisabled, test_observer_->status());

  // User request will be ignored.
  ReportBrightnessChangeEvent(
      20, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  scoped_task_environment_.FastForwardBy(
      BrightnessMonitorImpl::kBrightnessSampleDelay * 2);
  EXPECT_EQ(0, test_observer_->num_brightness_changes());
  EXPECT_EQ(0, test_observer_->num_user_brightness_change_requested());
}

// Two user brightness adjustments are received with a gap shorter than
// kBrightnessSampleDelay, hence final brightness is recorded.
TEST_F(BrightnessMonitorImplTest, TwoUserAdjustmentsShortGap) {
  SetUpBrightnessMonitor(10);
  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(BrightnessMonitor::Status::kSuccess, test_observer_->status());
  EXPECT_EQ(0, test_observer_->num_brightness_changes());
  EXPECT_EQ(0, test_observer_->num_user_brightness_change_requested());

  // First user-requested brightness adjustment.
  ReportBrightnessChangeEvent(
      20, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  EXPECT_EQ(1, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(0, test_observer_->num_brightness_changes());

  scoped_task_environment_.FastForwardBy(
      BrightnessMonitorImpl::kBrightnessSampleDelay / 2);
  // Second user-requested brightness adjustment.
  ReportBrightnessChangeEvent(
      30, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  EXPECT_EQ(2, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(0, test_observer_->num_brightness_changes());

  scoped_task_environment_.FastForwardBy(
      BrightnessMonitorImpl::kBrightnessSampleDelay * 2);

  EXPECT_EQ(2, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(1, test_observer_->num_brightness_changes());
  EXPECT_DOUBLE_EQ(10, test_observer_->old_brightness_percent());
  EXPECT_DOUBLE_EQ(30, test_observer_->new_brightness_percent());
}

// Two user brightness adjustments are received with a gap longer than
// kBrightnessSampleDelay, hence two brightness changes are recorded.
TEST_F(BrightnessMonitorImplTest, TwoUserAdjustmentsLongGap) {
  SetUpBrightnessMonitor(10);
  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(BrightnessMonitor::Status::kSuccess, test_observer_->status());
  EXPECT_EQ(0, test_observer_->num_brightness_changes());
  EXPECT_EQ(0, test_observer_->num_user_brightness_change_requested());

  // First user-requested brightness adjustment.
  ReportBrightnessChangeEvent(
      20, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  EXPECT_EQ(1, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(0, test_observer_->num_brightness_changes());

  scoped_task_environment_.FastForwardBy(
      BrightnessMonitorImpl::kBrightnessSampleDelay * 2);
  EXPECT_EQ(1, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(1, test_observer_->num_brightness_changes());
  EXPECT_DOUBLE_EQ(10, test_observer_->old_brightness_percent());
  EXPECT_DOUBLE_EQ(20, test_observer_->new_brightness_percent());

  // Second user-requested brightness adjustment.
  ReportBrightnessChangeEvent(
      30, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  EXPECT_EQ(2, test_observer_->num_user_brightness_change_requested());
  scoped_task_environment_.FastForwardBy(
      BrightnessMonitorImpl::kBrightnessSampleDelay * 2);

  EXPECT_EQ(2, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(2, test_observer_->num_brightness_changes());
  EXPECT_DOUBLE_EQ(20, test_observer_->old_brightness_percent());
  EXPECT_DOUBLE_EQ(30, test_observer_->new_brightness_percent());
}

// A brightness change not triggered by user request, followed by user requested
// change. The gap between the two is shorter than |kBrightnessSampleDelay|.
TEST_F(BrightnessMonitorImplTest, NonUserFollowedByUserShortGap) {
  SetUpBrightnessMonitor(10);
  scoped_task_environment_.FastForwardUntilNoTasksRemain();

  // Non-user.
  ReportBrightnessChangeEvent(
      20, power_manager::BacklightBrightnessChange_Cause_USER_ACTIVITY);
  EXPECT_EQ(0, test_observer_->num_user_brightness_change_requested());
  scoped_task_environment_.FastForwardBy(
      BrightnessMonitorImpl::kBrightnessSampleDelay / 2);
  EXPECT_EQ(0, test_observer_->num_brightness_changes());

  // User.
  ReportBrightnessChangeEvent(
      30, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  EXPECT_EQ(1, test_observer_->num_user_brightness_change_requested());
  scoped_task_environment_.FastForwardBy(
      BrightnessMonitorImpl::kBrightnessSampleDelay * 2);

  EXPECT_EQ(1, test_observer_->num_brightness_changes());
  EXPECT_DOUBLE_EQ(20, test_observer_->old_brightness_percent());
  EXPECT_DOUBLE_EQ(30, test_observer_->new_brightness_percent());
}

// A brightness change not triggered by user request, followed by user requested
// change. The gap between the two is longer than |kBrightnessSampleDelay|.
TEST_F(BrightnessMonitorImplTest, NonUserFollowedByUserLongGap) {
  SetUpBrightnessMonitor(10);
  scoped_task_environment_.FastForwardUntilNoTasksRemain();

  ReportBrightnessChangeEvent(
      20, power_manager::BacklightBrightnessChange_Cause_USER_ACTIVITY);
  EXPECT_EQ(0, test_observer_->num_user_brightness_change_requested());
  scoped_task_environment_.FastForwardBy(
      BrightnessMonitorImpl::kBrightnessSampleDelay * 2);
  EXPECT_EQ(0, test_observer_->num_brightness_changes());

  ReportBrightnessChangeEvent(
      30, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  EXPECT_EQ(1, test_observer_->num_user_brightness_change_requested());
  scoped_task_environment_.FastForwardBy(
      BrightnessMonitorImpl::kBrightnessSampleDelay * 2);

  EXPECT_EQ(1, test_observer_->num_brightness_changes());
  EXPECT_DOUBLE_EQ(20, test_observer_->old_brightness_percent());
  EXPECT_DOUBLE_EQ(30, test_observer_->new_brightness_percent());
}

// A user requested brightness change is received, another non-user triggered
// change is received before timer times out. Followed by another user requested
// change.
TEST_F(BrightnessMonitorImplTest, UserAdjustmentsSeparatedByNonUser) {
  SetUpBrightnessMonitor(10);
  scoped_task_environment_.FastForwardUntilNoTasksRemain();

  // User request.
  ReportBrightnessChangeEvent(
      20, power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  EXPECT_EQ(1, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(0, test_observer_->num_brightness_changes());

  // Non-user.
  scoped_task_environment_.FastForwardBy(
      BrightnessMonitorImpl::kBrightnessSampleDelay / 2);
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

  scoped_task_environment_.FastForwardBy(
      BrightnessMonitorImpl::kBrightnessSampleDelay * 2);
  EXPECT_EQ(2, test_observer_->num_user_brightness_change_requested());
  EXPECT_EQ(2, test_observer_->num_brightness_changes());
  EXPECT_DOUBLE_EQ(30, test_observer_->old_brightness_percent());
  EXPECT_DOUBLE_EQ(40, test_observer_->new_brightness_percent());
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
