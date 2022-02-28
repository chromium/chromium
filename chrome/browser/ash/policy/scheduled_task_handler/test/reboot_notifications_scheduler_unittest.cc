// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/test/fake_reboot_notifications_scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class RebootNotificationsSchedulerTest : public testing::Test {
 public:
  RebootNotificationsSchedulerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        notifications_scheduler_(task_environment_.GetMockClock(),
                                 task_environment_.GetMockTickClock()) {}

  ~RebootNotificationsSchedulerTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  FakeRebootNotificationsScheduler notifications_scheduler_;
};

TEST_F(RebootNotificationsSchedulerTest, ApplyingGraceTime) {
  base::Time now = task_environment_.GetMockClock()->Now();

  // Set uptime to 10 minutes and schedule reboot in 15 minutes. Apply grace
  // time.
  base::Time reboot_time = now + base::Minutes(15);
  notifications_scheduler_.SetUptime(base::Minutes(10));
  EXPECT_EQ(notifications_scheduler_.ShouldApplyGraceTime(reboot_time), true);

  // Schedule reboot in 5 hours. Don't apply grace time.
  reboot_time += base::Hours(5);
  EXPECT_EQ(notifications_scheduler_.ShouldApplyGraceTime(reboot_time), false);

  // Set uptime to 2 hours and schedule reboot in 15 minutes. Don't apply grace
  // time.
  reboot_time -= base::Hours(5);
  notifications_scheduler_.SetUptime(base::Hours(2));
  EXPECT_EQ(notifications_scheduler_.ShouldApplyGraceTime(reboot_time), false);
}

TEST_F(RebootNotificationsSchedulerTest, ShowNotificationAndDialogOnSchedule) {
  // Schedule reboot in 3 minutes. Expect dialog and notification to be shown
  // immediately.
  base::Time reboot_time =
      task_environment_.GetMockClock()->Now() + base::Minutes(3);
  notifications_scheduler_.ScheduleNotifications(base::NullCallback(),
                                                 reboot_time);
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 1);
  EXPECT_EQ(notifications_scheduler_.GetShowNotificationCalls(), 1);
}

TEST_F(RebootNotificationsSchedulerTest,
       ShowNotificationAndScheduleDialogTimer) {
  // Schedule reboot in 30 minutes. Expect notification to be shown immediately.
  // Schedule timer for showing dialog.
  base::Time reboot_time =
      task_environment_.GetMockClock()->Now() + base::Minutes(30);
  notifications_scheduler_.ScheduleNotifications(base::NullCallback(),
                                                 reboot_time);
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 0);
  EXPECT_EQ(notifications_scheduler_.GetShowNotificationCalls(), 1);

  // Fast forward time by 25 minutes. Expect dialog to be shown.
  task_environment_.FastForwardBy(base::Minutes(25));
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 1);
}

TEST_F(RebootNotificationsSchedulerTest, ScheduleNotificationAndDialogTimer) {
  // Schedule reboot in 2 hours. Schedule timers for showing dialog and
  // notification.
  base::Time reboot_time =
      task_environment_.GetMockClock()->Now() + base::Hours(2);
  notifications_scheduler_.ScheduleNotifications(base::NullCallback(),
                                                 reboot_time);
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
  // Set uptime to 5 minutes and schedule reboot in 10 minutes. Apply grace time
  // and don't show notification or dialog.
  base::Time reboot_time =
      task_environment_.GetMockClock()->Now() + base::Minutes(10);
  notifications_scheduler_.SetUptime(base::Minutes(5));
  notifications_scheduler_.ScheduleNotifications(base::NullCallback(),
                                                 reboot_time);
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 0);
  EXPECT_EQ(notifications_scheduler_.GetShowNotificationCalls(), 0);
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(notifications_scheduler_.GetShowNotificationCalls(), 0);
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 0);
}

TEST_F(RebootNotificationsSchedulerTest, RescheduleNotifications) {
  // Schedule reboot in 30 minutes. Expect notification to be shown immediately.
  // Schedule timer for showing dialog.
  base::Time reboot_time =
      task_environment_.GetMockClock()->Now() + base::Minutes(30);
  notifications_scheduler_.ScheduleNotifications(base::NullCallback(),
                                                 reboot_time);
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 0);
  EXPECT_EQ(notifications_scheduler_.GetShowNotificationCalls(), 1);

  // Reschedule reboot to happen in 2 hours and 30 minutes. Don't expect any new
  // notification or dialog at this moment.
  reboot_time += base::Hours(2);
  notifications_scheduler_.ScheduleNotifications(base::NullCallback(),
                                                 reboot_time);
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