// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/test/fake_reboot_notifications_scheduler.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class RebootNotificationsSchedulerTest : public testing::Test {
 public:
  RebootNotificationsSchedulerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        prefs_(std::make_unique<TestingPrefServiceSimple>()),
        notifications_scheduler_(
            std::make_unique<FakeRebootNotificationsScheduler>(
                task_environment_.GetMockClock(),
                task_environment_.GetMockTickClock(),
                prefs_.get())) {
    RebootNotificationsScheduler::RegisterProfilePrefs(prefs_->registry());
  }

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    std::unique_ptr<ash::FakeChromeUserManager> fake_user_manager =
        std::make_unique<ash::FakeChromeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
    AccountId account_id =
        AccountId::FromUserEmailGaiaId("test@example.com", "12345");
    profile_ = profile_manager_.CreateTestingProfile(account_id.GetUserEmail());
    fake_user_manager_->AddUser(account_id);
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_);
    fake_user_manager_->LoginUser(account_id, true);
    EXPECT_EQ(ProfileManager::GetActiveUserProfile(), profile_);

    ASSERT_EQ(RebootNotificationsScheduler::Get(),
              notifications_scheduler_.get());
  }

  ~RebootNotificationsSchedulerTest() override = default;

  int GetDisplayedNotificationCount() const {
    return display_service_tester_
        ->GetDisplayedNotificationsForType(NotificationHandler::Type::TRANSIENT)
        .size();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  session_manager::SessionManager session_manager_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<FakeRebootNotificationsScheduler> notifications_scheduler_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  TestingProfile* profile_ = nullptr;

  ash::FakeChromeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
};

TEST_F(RebootNotificationsSchedulerTest, ApplyingGraceTime) {
  base::Time now = task_environment_.GetMockClock()->Now();

  // Set uptime to 10 minutes and schedule reboot in 15 minutes. Apply grace
  // time.
  base::Time reboot_time = now + base::Minutes(15);
  notifications_scheduler_->SetUptime(base::Minutes(10));
  EXPECT_EQ(notifications_scheduler_->ShouldApplyGraceTime(reboot_time), true);

  // Schedule reboot in 5 hours. Don't apply grace time.
  reboot_time += base::Hours(5);
  EXPECT_EQ(notifications_scheduler_->ShouldApplyGraceTime(reboot_time), false);

  // Set uptime to 2 hours and schedule reboot in 15 minutes. Don't apply grace
  // time.
  reboot_time -= base::Hours(5);
  notifications_scheduler_->SetUptime(base::Hours(2));
  EXPECT_EQ(notifications_scheduler_->ShouldApplyGraceTime(reboot_time), false);
}

TEST_F(RebootNotificationsSchedulerTest, ShowNotificationAndDialogOnSchedule) {
  // Schedule reboot in 3 minutes. Expect dialog and notification to be shown
  // immediately.
  base::Time reboot_time =
      task_environment_.GetMockClock()->Now() + base::Minutes(3);
  notifications_scheduler_->SchedulePendingRebootNotifications(
      base::NullCallback(), reboot_time);
  EXPECT_EQ(notifications_scheduler_->GetShowDialogCalls(), 1);
  EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 1);
}

TEST_F(RebootNotificationsSchedulerTest,
       ShowNotificationAndScheduleDialogTimer) {
  // Schedule reboot in 30 minutes. Expect notification to be shown immediately.
  // Schedule timer for showing dialog.
  base::Time reboot_time =
      task_environment_.GetMockClock()->Now() + base::Minutes(30);
  notifications_scheduler_->SchedulePendingRebootNotifications(
      base::NullCallback(), reboot_time);
  EXPECT_EQ(notifications_scheduler_->GetShowDialogCalls(), 0);
  EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 1);

  // Fast forward time by 25 minutes. Expect dialog to be shown.
  task_environment_.FastForwardBy(base::Minutes(25));
  EXPECT_EQ(notifications_scheduler_->GetShowDialogCalls(), 1);
}

TEST_F(RebootNotificationsSchedulerTest, ScheduleNotificationAndDialogTimer) {
  // Schedule reboot in 2 hours. Schedule timers for showing dialog and
  // notification.
  base::Time reboot_time =
      task_environment_.GetMockClock()->Now() + base::Hours(2);
  notifications_scheduler_->SchedulePendingRebootNotifications(
      base::NullCallback(), reboot_time);
  EXPECT_EQ(notifications_scheduler_->GetShowDialogCalls(), 0);
  EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 0);

  // Fast forward time by 1 hour. Expect notification to be shown.
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 1);
  EXPECT_EQ(notifications_scheduler_->GetShowDialogCalls(), 0);

  // Fast forward time by 55 minutes. Expect dialog to be shown.
  task_environment_.FastForwardBy(base::Minutes(55));
  EXPECT_EQ(notifications_scheduler_->GetShowDialogCalls(), 1);
}

TEST_F(RebootNotificationsSchedulerTest, DoNotScheduleOrShowNotifications) {
  // Set uptime to 5 minutes and schedule reboot in 10 minutes. Apply grace time
  // and don't show notification or dialog.
  base::Time reboot_time =
      task_environment_.GetMockClock()->Now() + base::Minutes(10);
  notifications_scheduler_->SetUptime(base::Minutes(5));
  notifications_scheduler_->SchedulePendingRebootNotifications(
      base::NullCallback(), reboot_time);
  EXPECT_EQ(notifications_scheduler_->GetShowDialogCalls(), 0);
  EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 0);
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 0);
  EXPECT_EQ(notifications_scheduler_->GetShowDialogCalls(), 0);
}

TEST_F(RebootNotificationsSchedulerTest, RescheduleNotifications) {
  // Schedule reboot in 30 minutes. Expect notification to be shown immediately.
  // Schedule timer for showing dialog.
  base::Time reboot_time =
      task_environment_.GetMockClock()->Now() + base::Minutes(30);
  notifications_scheduler_->SchedulePendingRebootNotifications(
      base::NullCallback(), reboot_time);
  EXPECT_EQ(notifications_scheduler_->GetShowDialogCalls(), 0);
  EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 1);

  // Reschedule reboot to happen in 2 hours and 30 minutes. Don't expect any new
  // notification or dialog at this moment.
  reboot_time += base::Hours(2);
  notifications_scheduler_->SchedulePendingRebootNotifications(
      base::NullCallback(), reboot_time);
  EXPECT_EQ(notifications_scheduler_->GetShowDialogCalls(), 0);
  EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 1);

  // Fast forward time by 2 hours. Expect new notification to be shown.
  task_environment_.FastForwardBy(base::Hours(2));
  EXPECT_EQ(notifications_scheduler_->GetShowDialogCalls(), 0);
  EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 2);

  // Fast forward time by 25 minutes. Expect dialog to be shown.
  task_environment_.FastForwardBy(base::Minutes(25));
  EXPECT_EQ(notifications_scheduler_->GetShowDialogCalls(), 1);
  EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 2);
}

TEST_F(RebootNotificationsSchedulerTest,
       ScheduleAndShowPostRebootNotification) {
  // Verify initial state.
  EXPECT_FALSE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));
  EXPECT_EQ(GetDisplayedNotificationCount(), 0);

  // Schedule post reboot notification.
  notifications_scheduler_->SchedulePostRebootNotification();
  EXPECT_TRUE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));

  // Show post reboot notification.
  notifications_scheduler_->MaybeShowPostRebootNotification(true);
  EXPECT_EQ(GetDisplayedNotificationCount(), 1);

  // Verify pref is unset.
  EXPECT_FALSE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));
}

TEST_F(RebootNotificationsSchedulerTest,
       SchedulePostRebootNotificationFullRestoreDisabled) {
  // Verify initial state.
  EXPECT_FALSE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));
  EXPECT_EQ(GetDisplayedNotificationCount(), 0);

  // Schedule post reboot notification.
  notifications_scheduler_->SchedulePostRebootNotification();
  EXPECT_TRUE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));

  // Start the session and show post reboot notification.
  session_manager_.SessionStarted();
  EXPECT_EQ(GetDisplayedNotificationCount(), 1);

  // Verify pref is unset.
  EXPECT_FALSE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));
}

TEST_F(RebootNotificationsSchedulerTest,
       SchedulePostRebootNotificationFullRestoreEnabled) {
  // Verify initial state.
  EXPECT_FALSE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));
  EXPECT_EQ(GetDisplayedNotificationCount(), 0);

  // Schedule post reboot notification.
  notifications_scheduler_->SchedulePostRebootNotification();
  EXPECT_TRUE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));

  // Start the session and do not show post reboot notification.
  notifications_scheduler_->SetWaitFullRestoreInit(true);
  session_manager_.SessionStarted();
  EXPECT_EQ(GetDisplayedNotificationCount(), 0);
}

}  // namespace policy