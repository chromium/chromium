// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limit_notifier.h"

#include "base/memory/ref_counted.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class TimeLimitNotifierTest : public testing::Test {
 public:
  TimeLimitNotifierTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>()),
        notification_tester_(&profile_),
        notifier_(&profile_, task_runner_) {}

  TimeLimitNotifierTest(const TimeLimitNotifierTest&) = delete;
  TimeLimitNotifierTest& operator=(const TimeLimitNotifierTest&) = delete;

  ~TimeLimitNotifierTest() override = default;

 protected:
  bool HasLockNotification() {
    return notification_tester_.GetNotification("time-limit-lock-notification")
        .has_value();
  }

  bool HasPolicyUpdateNotification(TimeLimitNotifier::LimitType limit_type) {
    std::string notification_id;
    switch (limit_type) {
      case TimeLimitNotifier::LimitType::kScreenTime:
        notification_id = "time-limit-screen-time-updated";
        break;
      case TimeLimitNotifier::LimitType::kBedTime:
        notification_id = "time-limit-bedtime-updated";
        break;
      case TimeLimitNotifier::LimitType::kOverride:
        notification_id = "time-limit-override-updated";
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    return notification_tester_.GetNotification(notification_id).has_value();
  }

  void RemoveNotification() {
    notification_tester_.RemoveAllNotifications(
        NotificationHandler::Type::TRANSIENT, true /* by_user */);
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  NotificationDisplayServiceTester notification_tester_;
  TimeLimitNotifier notifier_;
};

TEST_F(TimeLimitNotifierTest, ShowLockNotifications) {
  notifier_.MaybeScheduleLockNotifications(
      TimeLimitNotifier::LimitType::kScreenTime, base::Minutes(20));

  // Fast forward a bit, but not far enough to show a notification.
  task_runner_->FastForwardBy(base::Minutes(10));
  EXPECT_FALSE(HasLockNotification());

  // Fast forward to the 5-minute warning time.
  task_runner_->FastForwardBy(base::Minutes(5));
  EXPECT_TRUE(HasLockNotification());

  // Fast forward to the 1-minute warning time.
  task_runner_->FastForwardBy(base::Minutes(4));
  EXPECT_TRUE(HasLockNotification());
}

TEST_F(TimeLimitNotifierTest, DismisLocksNotification) {
  notifier_.MaybeScheduleLockNotifications(
      TimeLimitNotifier::LimitType::kBedTime, base::Minutes(10));

  // Fast forward to the 5-minute warning time.
  task_runner_->FastForwardBy(base::Minutes(5));
  EXPECT_TRUE(HasLockNotification());
  RemoveNotification();

  // Fast forward one minute; the same notification is not reshown.
  task_runner_->FastForwardBy(base::Minutes(1));
  EXPECT_FALSE(HasLockNotification());

  // Fast forward to the 1-minute warning time.
  task_runner_->FastForwardBy(base::Minutes(3));
  EXPECT_TRUE(HasLockNotification());
}

TEST_F(TimeLimitNotifierTest, OnlyExiLocktNotification) {
  notifier_.MaybeScheduleLockNotifications(
      TimeLimitNotifier::LimitType::kScreenTime, base::Minutes(3));

  // Fast forward a bit, but not far enough to show a notification.
  task_runner_->FastForwardBy(base::Minutes(1));
  EXPECT_FALSE(HasLockNotification());

  // Fast forward to the 1-minute warning time.
  task_runner_->FastForwardBy(base::Minutes(1));
  EXPECT_TRUE(HasLockNotification());
  RemoveNotification();

  // Only one notification was shown.
  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(HasLockNotification());
}

TEST_F(TimeLimitNotifierTest, NoLockNotifications) {
  notifier_.MaybeScheduleLockNotifications(
      TimeLimitNotifier::LimitType::kBedTime, base::Seconds(30));

  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(HasLockNotification());
}

TEST_F(TimeLimitNotifierTest, UnscheduleLockNotifications) {
  notifier_.MaybeScheduleLockNotifications(
      TimeLimitNotifier::LimitType::kScreenTime, base::Minutes(10));

  // Fast forward to the 5-minute warning time.
  task_runner_->FastForwardBy(base::Minutes(5));
  EXPECT_TRUE(HasLockNotification());
  RemoveNotification();

  // Stop the timers.
  notifier_.UnscheduleNotifications();
  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(HasLockNotification());
}

TEST_F(TimeLimitNotifierTest, RescheduleLockNotifications) {
  notifier_.MaybeScheduleLockNotifications(
      TimeLimitNotifier::LimitType::kScreenTime, base::Minutes(20));

  // Update the notifier with a new limit.
  notifier_.MaybeScheduleLockNotifications(
      TimeLimitNotifier::LimitType::kScreenTime, base::Minutes(30));

  // Fast forward a bit, but not far enough to show a notification.
  task_runner_->FastForwardBy(base::Minutes(20));
  EXPECT_FALSE(HasLockNotification());

  // Fast forward to the 5-minute warning time.
  task_runner_->FastForwardBy(base::Minutes(5));
  EXPECT_TRUE(HasLockNotification());
  RemoveNotification();

  // Fast forward to the 1-minute warning time.
  task_runner_->FastForwardBy(base::Minutes(4));
  EXPECT_TRUE(HasLockNotification());
}

TEST_F(TimeLimitNotifierTest, NoOverriePolicyUpdateNotification) {
  notifier_.ShowPolicyUpdateNotification(
      TimeLimitNotifier::LimitType::kOverride, std::nullopt);

  EXPECT_FALSE(
      HasPolicyUpdateNotification(TimeLimitNotifier::LimitType::kOverride));
}

TEST_F(TimeLimitNotifierTest, ShowPolicyUpdateNotifications) {
  notifier_.ShowPolicyUpdateNotification(
      TimeLimitNotifier::LimitType::kScreenTime, std::nullopt);
  notifier_.ShowPolicyUpdateNotification(TimeLimitNotifier::LimitType::kBedTime,
                                         std::nullopt);
  base::Time lock_time;
  ASSERT_TRUE(base::Time::FromUTCString("1 Jan 2019 22:00 PST", &lock_time));
  notifier_.ShowPolicyUpdateNotification(
      TimeLimitNotifier::LimitType::kOverride, std::make_optional(lock_time));

  EXPECT_TRUE(
      HasPolicyUpdateNotification(TimeLimitNotifier::LimitType::kScreenTime));
  EXPECT_TRUE(
      HasPolicyUpdateNotification(TimeLimitNotifier::LimitType::kBedTime));
}

}  // namespace ash
