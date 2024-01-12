// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;

namespace policy {

class RebootNotificationsSchedulerTest : public testing::Test {
 public:
  using Requester = RebootNotificationsScheduler::Requester;

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
  raw_ptr<TestingProfile> profile_ = nullptr;

  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> fake_user_manager_ =
      nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
};

TEST_F(RebootNotificationsSchedulerTest, ShowNotificationAndDialogOnSchedule) {
  // Schedule reboot in 3 minutes. Expect dialog and notification to be shown
  // immediately.
  base::Time reboot_time =
      task_environment_.GetMockClock()->Now() + base::Minutes(3);
  notifications_scheduler_->SchedulePendingRebootNotifications(
      base::NullCallback(), reboot_time, Requester::kScheduledRebootPolicy);
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
      base::NullCallback(), reboot_time, Requester::kScheduledRebootPolicy);
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
      base::NullCallback(), reboot_time, Requester::kScheduledRebootPolicy);
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

TEST_F(RebootNotificationsSchedulerTest,
       RescheduleNotificationsForTheSameRequester) {
  // Schedule reboot in 30 minutes. Expect notification to be shown immediately.
  // Schedule timer for showing dialog.
  base::Time reboot_time =
      task_environment_.GetMockClock()->Now() + base::Minutes(30);
  notifications_scheduler_->SchedulePendingRebootNotifications(
      base::NullCallback(), reboot_time, Requester::kScheduledRebootPolicy);
  EXPECT_EQ(notifications_scheduler_->GetShowDialogCalls(), 0);
  EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 1);

  // Reschedule reboot to happen in 2 hours and 30 minutes. Don't expect any new
  // notification or dialog at this moment.
  reboot_time += base::Hours(2);
  notifications_scheduler_->SchedulePendingRebootNotifications(
      base::NullCallback(), reboot_time, Requester::kScheduledRebootPolicy);
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

TEST_F(RebootNotificationsSchedulerTest, ResetState) {
  // Check that fresh scheduler does not reset state.
  notifications_scheduler_->CancelRebootNotifications(
      Requester::kScheduledRebootPolicy);
  EXPECT_EQ(notifications_scheduler_->GetCloseNotificationCalls(), 0);

  const auto reboot_time =
      task_environment_.GetMockClock()->Now() + base::Minutes(10);
  notifications_scheduler_->SchedulePendingRebootNotifications(
      base::DoNothing(), reboot_time, Requester::kScheduledRebootPolicy);
  EXPECT_EQ(notifications_scheduler_->GetCloseNotificationCalls(), 1);

  // Check that requested scheduler does not reset state for another requester.
  notifications_scheduler_->CancelRebootNotifications(
      Requester::kRebootCommand);
  EXPECT_EQ(notifications_scheduler_->GetCloseNotificationCalls(), 1);

  // Check that requested scheduler resets state for the same requester.
  notifications_scheduler_->CancelRebootNotifications(
      Requester::kScheduledRebootPolicy);
  EXPECT_EQ(notifications_scheduler_->GetCloseNotificationCalls(), 2);
}

TEST_F(RebootNotificationsSchedulerTest, PendingNotificationRequests) {
  EXPECT_FALSE(notifications_scheduler_->GetCurrentRequesterForTesting());
  EXPECT_TRUE(notifications_scheduler_->GetRequestersForTesting().empty());

  // Schedule the first notification from the first requester.
  {
    const base::TimeDelta early_reboot_delay = base::Minutes(30);
    const base::Time early_reboot_time =
        task_environment_.GetMockClock()->Now() + early_reboot_delay;
    notifications_scheduler_->SchedulePendingRebootNotifications(
        base::DoNothing(), early_reboot_time,
        Requester::kScheduledRebootPolicy);
    EXPECT_EQ(notifications_scheduler_->GetCurrentRequesterForTesting(),
              Requester::kScheduledRebootPolicy);
    EXPECT_THAT(notifications_scheduler_->GetRequestersForTesting(),
                UnorderedElementsAre(Requester::kScheduledRebootPolicy));
    EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 1);
    EXPECT_EQ(notifications_scheduler_->GetCloseNotificationCalls(), 1);
  }

  // Schedule a notification from the second requester after the first one.
  // Check it goes to pending.
  {
    const base::TimeDelta later_reboot_delay = base::Minutes(40);
    const base::Time later_reboot_time =
        task_environment_.GetMockClock()->Now() + later_reboot_delay;
    notifications_scheduler_->SchedulePendingRebootNotifications(
        base::DoNothing(), later_reboot_time, Requester::kRebootCommand);
    EXPECT_EQ(notifications_scheduler_->GetCurrentRequesterForTesting(),
              Requester::kScheduledRebootPolicy);
    EXPECT_THAT(notifications_scheduler_->GetRequestersForTesting(),
                UnorderedElementsAre(Requester::kScheduledRebootPolicy,
                                     Requester::kRebootCommand));
    EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 1);
    EXPECT_EQ(notifications_scheduler_->GetCloseNotificationCalls(), 1);
  }

  // Schedule a notification from the the first requester before the initial
  // time. Check notification is rescheduled.
  {
    const base::TimeDelta earlier_reboot_delay = base::Minutes(20);
    const base::Time earlier_reboot_time =
        task_environment_.GetMockClock()->Now() + earlier_reboot_delay;
    notifications_scheduler_->SchedulePendingRebootNotifications(
        base::DoNothing(), earlier_reboot_time,
        Requester::kScheduledRebootPolicy);
    EXPECT_EQ(notifications_scheduler_->GetCurrentRequesterForTesting(),
              Requester::kScheduledRebootPolicy);
    EXPECT_THAT(notifications_scheduler_->GetRequestersForTesting(),
                UnorderedElementsAre(Requester::kScheduledRebootPolicy,
                                     Requester::kRebootCommand));
    EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 2);
    EXPECT_EQ(notifications_scheduler_->GetCloseNotificationCalls(), 2);
  }

  // Reset the first requester, check the pending is shown.
  {
    notifications_scheduler_->CancelRebootNotifications(
        Requester::kScheduledRebootPolicy);
    EXPECT_EQ(notifications_scheduler_->GetCurrentRequesterForTesting(),
              Requester::kRebootCommand);
    EXPECT_THAT(notifications_scheduler_->GetRequestersForTesting(),
                UnorderedElementsAre(Requester::kRebootCommand));
    EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 3);
    EXPECT_EQ(notifications_scheduler_->GetCloseNotificationCalls(), 3);
  }

  // Schedule a notification for the first requester before the second one.
  // Check the first become current and the second becomes pending.
  {
    const base::TimeDelta early_reboot_delay = base::Minutes(30);
    const base::Time early_reboot_time =
        task_environment_.GetMockClock()->Now() + early_reboot_delay;
    notifications_scheduler_->SchedulePendingRebootNotifications(
        base::DoNothing(), early_reboot_time,
        Requester::kScheduledRebootPolicy);
    EXPECT_EQ(notifications_scheduler_->GetCurrentRequesterForTesting(),
              Requester::kScheduledRebootPolicy);
    EXPECT_THAT(notifications_scheduler_->GetRequestersForTesting(),
                UnorderedElementsAre(Requester::kScheduledRebootPolicy,
                                     Requester::kRebootCommand));
    EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 4);
    EXPECT_EQ(notifications_scheduler_->GetCloseNotificationCalls(), 4);
  }

  // Reset the second requester. Check the pending is empty and notification is
  // not changed.
  {
    notifications_scheduler_->CancelRebootNotifications(
        Requester::kRebootCommand);
    EXPECT_EQ(notifications_scheduler_->GetCurrentRequesterForTesting(),
              Requester::kScheduledRebootPolicy);
    EXPECT_THAT(notifications_scheduler_->GetRequestersForTesting(),
                UnorderedElementsAre(Requester::kScheduledRebootPolicy));
    EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 4);
    EXPECT_EQ(notifications_scheduler_->GetCloseNotificationCalls(), 4);
  }

  // Schedule the second after the first, and then the first after second. Check
  // second becomes current and the first becomes pending.
  {
    const base::TimeDelta later_reboot_delay = base::Minutes(40);
    const base::Time later_reboot_time =
        task_environment_.GetMockClock()->Now() + later_reboot_delay;
    notifications_scheduler_->SchedulePendingRebootNotifications(
        base::DoNothing(), later_reboot_time, Requester::kRebootCommand);
    EXPECT_EQ(notifications_scheduler_->GetCurrentRequesterForTesting(),
              Requester::kScheduledRebootPolicy);
    EXPECT_THAT(notifications_scheduler_->GetRequestersForTesting(),
                UnorderedElementsAre(Requester::kScheduledRebootPolicy,
                                     Requester::kRebootCommand));
    EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 4);
    EXPECT_EQ(notifications_scheduler_->GetCloseNotificationCalls(), 4);

    const base::TimeDelta after_later_reboot_delay =
        later_reboot_delay + base::Minutes(1);
    const base::Time after_later_reboot_time =
        task_environment_.GetMockClock()->Now() + after_later_reboot_delay;
    notifications_scheduler_->SchedulePendingRebootNotifications(
        base::DoNothing(), after_later_reboot_time,
        Requester::kScheduledRebootPolicy);
    EXPECT_EQ(notifications_scheduler_->GetCurrentRequesterForTesting(),
              Requester::kRebootCommand);
    EXPECT_THAT(notifications_scheduler_->GetRequestersForTesting(),
                UnorderedElementsAre(Requester::kRebootCommand,
                                     Requester::kScheduledRebootPolicy));
    EXPECT_EQ(notifications_scheduler_->GetShowNotificationCalls(), 5);
    EXPECT_EQ(notifications_scheduler_->GetCloseNotificationCalls(), 5);
  }
}

}  // namespace policy
