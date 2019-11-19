// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/saml/in_session_password_change_manager.h"

#include "ash/public/cpp/session/session_activation_observer.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using message_center::Notification;

namespace chromeos {

namespace {

constexpr base::TimeDelta kOneHour = base::TimeDelta::FromHours(1);
constexpr base::TimeDelta kOneDay = base::TimeDelta::FromDays(1);
constexpr base::TimeDelta kAdvanceWarningTime = base::TimeDelta::FromDays(14);
constexpr base::TimeDelta kOneYear = base::TimeDelta::FromDays(365);
constexpr base::TimeDelta kTenYears = base::TimeDelta::FromDays(10 * 365);

inline base::string16 utf16(const char* ascii) {
  return base::ASCIIToUTF16(ascii);
}

}  // namespace

class InSessionPasswordChangeManagerTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test");
    profile_->GetPrefs()->SetBoolean(prefs::kSamlInSessionPasswordChangeEnabled,
                                     true);
    profile_->GetPrefs()->SetInteger(
        prefs::kSamlPasswordExpirationAdvanceWarningDays,
        kAdvanceWarningTime.InDays());

    std::unique_ptr<FakeChromeUserManager> fake_user_manager =
        std::make_unique<FakeChromeUserManager>();
    fake_user_manager->AddUser(user_manager::StubAccountId());
    fake_user_manager->LoginUser(user_manager::StubAccountId());
    ASSERT_TRUE(fake_user_manager->GetPrimaryUser());
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_);
    manager_ = std::make_unique<InSessionPasswordChangeManager>(profile_);

    // urgent_warning_days_ = -1: This means we only ever show a standard
    // notification, instead of an urgent one, because it is simpler to test.
    // TODO(https://crbug.com/930109): Test both types of notification.
    manager_->urgent_warning_days_ = -1;
    InSessionPasswordChangeManager::SetForTesting(manager_.get());
  }

  void TearDown() override {
    InSessionPasswordChangeManager::ResetForTesting();
  }

 protected:
  base::Optional<Notification> Notification() {
    return NotificationDisplayServiceTester::Get()->GetNotification(
        "saml.password-expiry-notification");
  }

  void SetExpirationTime(base::Time expiration_time) {
    SamlPasswordAttributes attrs(base::Time(), expiration_time, "");
    attrs.SaveToPrefs(profile_->GetPrefs());
  }

  void ExpectNotificationAndDismiss() {
    EXPECT_TRUE(Notification().has_value());
    manager_->DismissExpiryNotification();
    EXPECT_FALSE(Notification().has_value());
  }

  content::BrowserTaskEnvironment test_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  TestingProfile* profile_;

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
  std::unique_ptr<InSessionPasswordChangeManager> manager_;
};

TEST_F(InSessionPasswordChangeManagerTest, MaybeShow_PolicyDisabled) {
  SetExpirationTime(base::Time::Now());
  profile_->GetPrefs()->SetBoolean(prefs::kSamlInSessionPasswordChangeEnabled,
                                   false);
  manager_->MaybeShowExpiryNotification();

  EXPECT_FALSE(Notification().has_value());
}

TEST_F(InSessionPasswordChangeManagerTest, MaybeShow_WillNotExpire) {
  SamlPasswordAttributes::DeleteFromPrefs(profile_->GetPrefs());
  manager_->MaybeShowExpiryNotification();

  EXPECT_FALSE(Notification().has_value());
  // No notification shown now and nothing shown in the next 10 years.
  test_environment_.FastForwardBy(kTenYears);
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(InSessionPasswordChangeManagerTest, MaybeShow_AlreadyExpired) {
  SetExpirationTime(base::Time::Now() - kOneYear);  // Expired last year.
  manager_->MaybeShowExpiryNotification();

  // Notification is shown immediately since password has expired.
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password change overdue"), Notification()->title());
}

TEST_F(InSessionPasswordChangeManagerTest, MaybeShow_WillSoonExpire) {
  SetExpirationTime(base::Time::Now() + (kAdvanceWarningTime / 2) - kOneHour);
  manager_->MaybeShowExpiryNotification();

  // Notification is shown immediately since password will soon expire.
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password expires in 7 days"), Notification()->title());
}

TEST_F(InSessionPasswordChangeManagerTest, MaybeShow_WillEventuallyExpire) {
  SetExpirationTime(base::Time::Now() + kOneYear + kAdvanceWarningTime);
  manager_->MaybeShowExpiryNotification();

  // Notification is not shown when expiration is still over a year away.
  EXPECT_FALSE(Notification().has_value());

  // But, it will be shown once we are in the advance warning window:
  test_environment_.FastForwardBy(kOneYear + kOneHour);
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password expires in 14 days"), Notification()->title());
}

TEST_F(InSessionPasswordChangeManagerTest, MaybeShow_DeleteExpirationTime) {
  SetExpirationTime(base::Time::Now() + kOneYear);
  manager_->MaybeShowExpiryNotification();

  // Notification is not shown immediately.
  EXPECT_FALSE(Notification().has_value());

  // Since expiration time is now removed, it is not shown later either.
  SamlPasswordAttributes::DeleteFromPrefs(profile_->GetPrefs());
  test_environment_.FastForwardBy(kTenYears);
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(InSessionPasswordChangeManagerTest, MaybeShow_PasswordChanged) {
  SetExpirationTime(base::Time::Now() + (kAdvanceWarningTime / 2) - kOneHour);
  manager_->MaybeShowExpiryNotification();

  // Notification is shown immediately since password will soon expire.
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password expires in 7 days"), Notification()->title());

  // Password is changed and notification is dismissed.
  SamlPasswordAttributes::DeleteFromPrefs(profile_->GetPrefs());
  manager_->DismissExpiryNotification();

  // From now on, notification will not be reshown.
  test_environment_.FastForwardBy(kTenYears);
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(InSessionPasswordChangeManagerTest, MaybeShow_Idempotent) {
  SetExpirationTime(base::Time::Now() + kOneYear);

  // Calling MaybeShowSamlPasswordExpiryNotification should only add one task -
  // to maybe show the notification in about a year.
  int baseline_task_count = test_environment_.GetPendingMainThreadTaskCount();
  manager_->MaybeShowExpiryNotification();
  int new_task_count = test_environment_.GetPendingMainThreadTaskCount();
  EXPECT_EQ(1, new_task_count - baseline_task_count);

  // Calling it many times shouldn't create more tasks - we only need one task
  // to show the notification in about a year.
  for (int i = 0; i < 10; i++) {
    manager_->MaybeShowExpiryNotification();
  }
  new_task_count = test_environment_.GetPendingMainThreadTaskCount();
  EXPECT_EQ(1, new_task_count - baseline_task_count);
}

TEST_F(InSessionPasswordChangeManagerTest, TimePasses_NoUserActionTaken) {
  SetExpirationTime(base::Time::Now() + kOneYear + kAdvanceWarningTime);
  manager_->MaybeShowExpiryNotification();

  // Notification is not shown immediately.
  EXPECT_FALSE(Notification().has_value());

  // After one year, we are still not quite inside the advance warning window.
  test_environment_.FastForwardBy(kOneYear - (kOneDay / 2));
  EXPECT_FALSE(Notification().has_value());

  // But the next day, the notification is shown.
  test_environment_.FastForwardBy(kOneDay);
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password expires in 14 days"), Notification()->title());
  EXPECT_EQ(utf16("Choose a new one now"), Notification()->message());

  // As time passes, the notification updates each day.
  test_environment_.FastForwardBy(kAdvanceWarningTime / 2);
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password expires in 7 days"), Notification()->title());
  EXPECT_EQ(utf16("Choose a new one now"), Notification()->message());

  test_environment_.FastForwardBy(kAdvanceWarningTime / 2);
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password change overdue"), Notification()->title());
  EXPECT_EQ(utf16("Choose a new one now"), Notification()->message());

  test_environment_.FastForwardBy(kOneYear);
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password change overdue"), Notification()->title());
  EXPECT_EQ(utf16("Choose a new one now"), Notification()->message());
}

TEST_F(InSessionPasswordChangeManagerTest, TimePasses_NotificationDismissed) {
  SetExpirationTime(base::Time::Now() + kOneYear + kAdvanceWarningTime / 2);
  manager_->MaybeShowExpiryNotification();

  // Notification is not shown immediately.
  EXPECT_FALSE(Notification().has_value());

  // Notification appears once we are inside the advance warning window.
  test_environment_.FastForwardBy(kOneYear);
  ExpectNotificationAndDismiss();

  // If a day goes past and the password still hasn't been changed, then the
  // notification will be shown again.
  test_environment_.FastForwardBy(kOneDay);
  ExpectNotificationAndDismiss();

  // This continues each day even once the password has long expired.
  test_environment_.FastForwardBy(kTenYears);
  ExpectNotificationAndDismiss();
  test_environment_.FastForwardBy(kOneDay);
  ExpectNotificationAndDismiss();
}

TEST_F(InSessionPasswordChangeManagerTest, ReshowOnUnlock) {
  SetExpirationTime(base::Time::Now() + kAdvanceWarningTime / 2);
  manager_->MaybeShowExpiryNotification();

  // Notification is shown immediately.
  EXPECT_TRUE(Notification().has_value());
  base::Time first_shown_at = Notification()->timestamp();

  // This notification is still present an hour later - but it is the same
  // notification as before. So it is no longer shown prominently on screen.
  test_environment_.FastForwardBy(kOneHour);
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(first_shown_at, Notification()->timestamp());

  // But when the screen is unlocked, the old notification is replaced with a
  // newer one. The new one is prominently shown on screen for a few seconds.
  manager_->OnScreenUnlocked();
  EXPECT_TRUE(Notification().has_value());
  EXPECT_NE(first_shown_at, Notification()->timestamp());
}

TEST_F(InSessionPasswordChangeManagerTest, DontReshowWhenDismissed) {
  SetExpirationTime(base::Time::Now() + kAdvanceWarningTime / 2);
  manager_->MaybeShowExpiryNotification();

  // Notification is shown immediately.
  EXPECT_TRUE(Notification().has_value());

  // If dismissed, the notification won't reappear within the next hour, since
  // we don't want to nag the user continuously.
  manager_->DismissExpiryNotification();
  manager_->OnExpiryNotificationDismissedByUser();
  test_environment_.FastForwardBy(kOneHour);
  EXPECT_FALSE(Notification().has_value());

  // Nor will it reappear if the user unlocks the screen.
  manager_->OnScreenUnlocked();
  EXPECT_FALSE(Notification().has_value());

  // But it will eventually reappear the next day.
  test_environment_.FastForwardBy(kOneDay);
  EXPECT_TRUE(Notification().has_value());
}

}  // namespace chromeos
