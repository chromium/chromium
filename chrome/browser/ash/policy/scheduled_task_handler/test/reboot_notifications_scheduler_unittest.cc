// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/reboot_notifications_scheduler.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace policy {

namespace {
// Time zone that will be used in tests.
constexpr char kUTCTimezone[] = "UTC";
}  // namespace

class RebootNotificationsSchedulerForTest
    : public RebootNotificationsScheduler {
 public:
  RebootNotificationsSchedulerForTest(const base::Clock* clock,
                                      const base::TickClock* tick_clock)
      : RebootNotificationsScheduler(clock, tick_clock),
        clock_(clock),
        uptime_(base::Hours(10)) {
    time_zone_ = base::WrapUnique(icu::TimeZone::createTimeZone(
        icu::UnicodeString::fromUTF8(kUTCTimezone)));
  }

  RebootNotificationsSchedulerForTest(
      const RebootNotificationsSchedulerForTest&) = delete;
  RebootNotificationsSchedulerForTest& operator=(
      const RebootNotificationsSchedulerForTest&) = delete;

  ~RebootNotificationsSchedulerForTest() = default;

  int GetShowDialogCalls() const { return show_dialog_calls_; }
  int GetShowNotificationCalls() const { return show_notification_calls_; }
  void SetUptime(base::TimeDelta uptime) { uptime_ = uptime; }

 private:
  void MaybeShowNotification() override { ++show_notification_calls_; }

  void MaybeShowDialog() override { ++show_dialog_calls_; }

  const base::Time GetCurrentTime() const override { return clock_->Now(); }

  const icu::TimeZone& GetTimeZone() const override { return *time_zone_; }

  const base::TimeDelta GetSystemUptime() const override { return uptime_; }

  int show_dialog_calls_ = 0, show_notification_calls_ = 0;
  const base::Clock* clock_;
  std::unique_ptr<icu::TimeZone> time_zone_;
  // Default uptime for test is 10h.
  base::TimeDelta uptime_;
};

class RebootNotificationsSchedulerTest : public testing::Test {
 public:
  RebootNotificationsSchedulerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        notifications_scheduler_(task_environment_.GetMockClock(),
                                 task_environment_.GetMockTickClock()) {}

  ~RebootNotificationsSchedulerTest() override = default;

  ScheduledTaskExecutor::ScheduledTaskData GetTaskDataFromNow() const {
    base::Time reboot_time = task_environment_.GetMockClock()->Now();
    base::Time::Exploded exploded;
    reboot_time.UTCExplode(&exploded);

    ScheduledTaskExecutor::ScheduledTaskData data;
    data.frequency = ScheduledTaskExecutor::Frequency::kDaily;
    data.hour = exploded.hour;
    data.minute = exploded.minute;
    return data;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  RebootNotificationsSchedulerForTest notifications_scheduler_;
};

TEST_F(RebootNotificationsSchedulerTest, ApplyingGraceTime) {
  ScheduledTaskExecutor::ScheduledTaskData data = GetTaskDataFromNow();

  // Set uptime to 10 minutes and schedule reboot in 15 minutes. Apply grace
  // time.
  data.minute += 15;
  notifications_scheduler_.SetUptime(base::Minutes(10));
  EXPECT_EQ(notifications_scheduler_.ShouldApplyGraceTime(data), true);

  // Schedule reboot in 5 hours. Don't apply grace time.
  data.hour += 5;
  EXPECT_EQ(notifications_scheduler_.ShouldApplyGraceTime(data), false);

  // Set uptime to 2 hours and schedule reboot in 15 minutes. Don't apply grace
  // time.
  data.hour -= 5;
  notifications_scheduler_.SetUptime(base::Hours(2));
  EXPECT_EQ(notifications_scheduler_.ShouldApplyGraceTime(data), false);
}

TEST_F(RebootNotificationsSchedulerTest, ShowNotificationAndDialogOnSchedule) {
  ScheduledTaskExecutor::ScheduledTaskData data = GetTaskDataFromNow();

  // Schedule reboot in 3 minutes. Expect dialog and notification to be shown
  // immediately.
  data.minute += 3;
  notifications_scheduler_.ScheduleNotifications(base::NullCallback(), data);
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 1);
  EXPECT_EQ(notifications_scheduler_.GetShowNotificationCalls(), 1);
}

TEST_F(RebootNotificationsSchedulerTest,
       ShowNotificationAndScheduleDialogTimer) {
  ScheduledTaskExecutor::ScheduledTaskData data = GetTaskDataFromNow();

  // Schedule reboot in 30 minutes. Expect notification to be shown immediately.
  // Schedule timer for showing dialog.
  data.minute += 30;
  notifications_scheduler_.ScheduleNotifications(base::NullCallback(), data);
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 0);
  EXPECT_EQ(notifications_scheduler_.GetShowNotificationCalls(), 1);

  // Fast forward time by 25 minutes. Expect dialog to be shown.
  task_environment_.FastForwardBy(base::Minutes(25));
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 1);
}

TEST_F(RebootNotificationsSchedulerTest, ScheduleNotificationAndDialogTimer) {
  ScheduledTaskExecutor::ScheduledTaskData data = GetTaskDataFromNow();

  // Schedule reboot in 2 hours. Schedule timers for showing dialog and
  // notification.
  data.hour += 2;
  notifications_scheduler_.ScheduleNotifications(base::NullCallback(), data);
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 0);
  EXPECT_EQ(notifications_scheduler_.GetShowNotificationCalls(), 0);

  // Fast forward time by 1 hour. Expect notification to be shown.
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(notifications_scheduler_.GetShowNotificationCalls(), 1);
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 0);

  // Fast forward time by 55 minutes. Expect dialog to be shown.
  task_environment_.FastForwardBy(base::Minutes(55));
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 1);
}

TEST_F(RebootNotificationsSchedulerTest, DoNotScheduleOrShowNotifications) {
  ScheduledTaskExecutor::ScheduledTaskData data = GetTaskDataFromNow();
  // Set uptime to 5 minutes and schedule reboot in 10 minutes. Apply grace time
  // and don't show notification or dialog.
  data.minute += 10;
  notifications_scheduler_.SetUptime(base::Minutes(5));
  notifications_scheduler_.ScheduleNotifications(base::NullCallback(), data);
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 0);
  EXPECT_EQ(notifications_scheduler_.GetShowNotificationCalls(), 0);
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(notifications_scheduler_.GetShowNotificationCalls(), 0);
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 0);
}

TEST_F(RebootNotificationsSchedulerTest, RescheduleNotifications) {
  ScheduledTaskExecutor::ScheduledTaskData data = GetTaskDataFromNow();

  // Schedule reboot in 30 minutes. Expect notification to be shown immediately.
  // Schedule timer for showing dialog.
  data.minute += 30;
  notifications_scheduler_.ScheduleNotifications(base::NullCallback(), data);
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 0);
  EXPECT_EQ(notifications_scheduler_.GetShowNotificationCalls(), 1);

  // Reschedule reboot to happen in 2 hours and 30 minutes. Don't expect any new
  // notification or dialog at this moment.
  data.hour += 2;
  notifications_scheduler_.ScheduleNotifications(base::NullCallback(), data);
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 0);
  EXPECT_EQ(notifications_scheduler_.GetShowNotificationCalls(), 1);

  // Fast forward time by 2 hours. Expect new notification to be shown.
  task_environment_.FastForwardBy(base::Hours(2));
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 0);
  EXPECT_EQ(notifications_scheduler_.GetShowNotificationCalls(), 2);

  // Fast forward time by 25 minutes. Expect dialog to be shown.
  task_environment_.FastForwardBy(base::Minutes(25));
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 1);
  EXPECT_EQ(notifications_scheduler_.GetShowNotificationCalls(), 2);
}

}  // namespace policy