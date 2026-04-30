// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/in_session_password_change_manager.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/cryptohome/auth_factor.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/saml_password_attributes.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::message_center::Notification;

constexpr base::TimeDelta kOneHour = base::Hours(1);
constexpr base::TimeDelta kOneDay = base::Days(1);
constexpr base::TimeDelta kAdvanceWarningTime = base::Days(14);
constexpr base::TimeDelta kOneYear = base::Days(365);
constexpr base::TimeDelta kThreeYears = base::Days(3 * 365);

inline std::u16string utf16(const char* ascii) {
  return base::ASCIIToUTF16(ascii);
}

}  // namespace

class InSessionPasswordChangeManagerTestBase : public testing::Test {
 public:
  InSessionPasswordChangeManagerTestBase() {
    UserDataAuthClient::InitializeFake();
  }

  ~InSessionPasswordChangeManagerTestBase() override {
    UserDataAuthClient::Shutdown();
  }

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test");
    profile_->GetPrefs()->SetBoolean(prefs::kSamlInSessionPasswordChangeEnabled,
                                     true);
    profile_->GetPrefs()->SetInteger(
        prefs::kSamlPasswordExpirationAdvanceWarningDays,
        kAdvanceWarningTime.InDays());

    fake_user_manager_->AddUser(user_manager::StubAccountId());
    fake_user_manager_->LoginUser(user_manager::StubAccountId());
    ASSERT_TRUE(fake_user_manager_->GetPrimaryUser());

    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_);
    manager_ = std::make_unique<InSessionPasswordChangeManager>(
        TestingBrowserProcess::GetGlobal()->local_state(), profile_);

    // urgent_warning_days_ = -1: This means we only ever show a standard
    // notification, instead of an urgent one, because it is simpler to test.
    // TODO(crbug.com/40613129): Test both types of notification.
    manager_->urgent_warning_days_ = -1;
    InSessionPasswordChangeManager::SetForTesting(manager_.get());
  }

  void TearDown() override {
    InSessionPasswordChangeManager::ResetForTesting();
  }

 protected:
  void ConfigureOnlinePassword() {
    // Configure an online password for the user.
    const auto cryptohome_id = cryptohome::CreateAccountIdentifierFromAccountId(
        user_manager::StubAccountId());
    FakeUserDataAuthClient::TestApi::Get()->AddExistingUser(cryptohome_id);
    user_data_auth::AuthFactor factor;
    factor.set_type(user_data_auth::AUTH_FACTOR_TYPE_PASSWORD);
    factor.set_label(ash::kCryptohomeGaiaKeyLabel);
    user_data_auth::AuthInput input;
    input.mutable_password_input()->set_secret("secret");
    FakeUserDataAuthClient::TestApi::Get()->AddAuthFactor(cryptohome_id, factor,
                                                          input);
  }

  std::optional<Notification> Notification() {
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

  void MaybeShowExpiryNotificationAndWait() {
    manager_->MaybeShowExpiryNotification();
    // When kManagedLocalPinAndPassword is enabled, MaybeShowExpiryNotification
    // triggers an asynchronous check for auth factors. We need to run until
    // idle to ensure the callback is executed.
    test_environment_.RunUntilIdle();
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment test_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
  std::unique_ptr<InSessionPasswordChangeManager> manager_;
};

class InSessionPasswordChangeManagerTest
    : public InSessionPasswordChangeManagerTestBase,
      public testing::WithParamInterface<bool> {
 public:
  InSessionPasswordChangeManagerTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(features::kManagedLocalPinAndPassword);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kManagedLocalPinAndPassword);
    }
  }

  void SetUp() override {
    InSessionPasswordChangeManagerTestBase::SetUp();
    ConfigureOnlinePassword();
  }
};

TEST_P(InSessionPasswordChangeManagerTest, MaybeShow_PolicyDisabled) {
  SetExpirationTime(base::Time::Now());
  profile_->GetPrefs()->SetBoolean(prefs::kSamlInSessionPasswordChangeEnabled,
                                   false);
  MaybeShowExpiryNotificationAndWait();

  EXPECT_FALSE(Notification().has_value());
}

TEST_P(InSessionPasswordChangeManagerTest, MaybeShow_WillNotExpire) {
  SamlPasswordAttributes::DeleteFromPrefs(profile_->GetPrefs());
  MaybeShowExpiryNotificationAndWait();

  EXPECT_FALSE(Notification().has_value());
  // No notification shown now and nothing shown in the next 3 years.
  test_environment_.FastForwardBy(kThreeYears);
  EXPECT_FALSE(Notification().has_value());
}

TEST_P(InSessionPasswordChangeManagerTest, MaybeShow_AlreadyExpired) {
  SetExpirationTime(base::Time::Now() - kOneYear);  // Expired last year.
  MaybeShowExpiryNotificationAndWait();

  // Notification is shown immediately since password has expired.
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password change overdue"), Notification()->title());
}

TEST_P(InSessionPasswordChangeManagerTest, MaybeShow_WillSoonExpire) {
  SetExpirationTime(base::Time::Now() + (kAdvanceWarningTime / 2) - kOneHour);
  MaybeShowExpiryNotificationAndWait();

  // Notification is shown immediately since password will soon expire.
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password expires in 7 days"), Notification()->title());
}

TEST_P(InSessionPasswordChangeManagerTest, MaybeShow_WillEventuallyExpire) {
  SetExpirationTime(base::Time::Now() + kOneYear + kAdvanceWarningTime);
  MaybeShowExpiryNotificationAndWait();

  // Notification is not shown when expiration is still over a year away.
  EXPECT_FALSE(Notification().has_value());

  // But, it will be shown once we are in the advance warning window:
  test_environment_.FastForwardBy(kOneYear + kOneHour);
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password expires in 14 days"), Notification()->title());
}

TEST_P(InSessionPasswordChangeManagerTest, MaybeShow_DeleteExpirationTime) {
  SetExpirationTime(base::Time::Now() + kOneYear);
  MaybeShowExpiryNotificationAndWait();

  // Notification is not shown immediately.
  EXPECT_FALSE(Notification().has_value());

  // Since expiration time is now removed, it is not shown later either.
  SamlPasswordAttributes::DeleteFromPrefs(profile_->GetPrefs());
  test_environment_.FastForwardBy(kThreeYears);
  EXPECT_FALSE(Notification().has_value());
}

TEST_P(InSessionPasswordChangeManagerTest, MaybeShow_PasswordChanged) {
  SetExpirationTime(base::Time::Now() + (kAdvanceWarningTime / 2) - kOneHour);
  MaybeShowExpiryNotificationAndWait();

  // Notification is shown immediately since password will soon expire.
  EXPECT_TRUE(Notification().has_value());
  EXPECT_EQ(utf16("Password expires in 7 days"), Notification()->title());

  // Password is changed and notification is dismissed.
  SamlPasswordAttributes::DeleteFromPrefs(profile_->GetPrefs());
  manager_->DismissExpiryNotification();

  // From now on, notification will not be reshown.
  test_environment_.FastForwardBy(kThreeYears);
  EXPECT_FALSE(Notification().has_value());
}

TEST_P(InSessionPasswordChangeManagerTest, MaybeShow_Idempotent) {
  SetExpirationTime(base::Time::Now() + kOneYear);
  // Ensure any initial asynchronous work (like auth factor check) is complete
  // before we start counting tasks.
  test_environment_.RunUntilIdle();

  // Calling MaybeShowSamlPasswordExpiryNotification should only add one task -
  // to maybe show the notification in about a year.
  int baseline_task_count = test_environment_.GetPendingMainThreadTaskCount();
  MaybeShowExpiryNotificationAndWait();
  int new_task_count = test_environment_.GetPendingMainThreadTaskCount();
  EXPECT_EQ(1, new_task_count - baseline_task_count);

  // Calling it many times shouldn't create more tasks - we only need one task
  // to show the notification in about a year.
  for (int i = 0; i < 10; i++) {
    MaybeShowExpiryNotificationAndWait();
  }
  new_task_count = test_environment_.GetPendingMainThreadTaskCount();
  EXPECT_EQ(1, new_task_count - baseline_task_count);
}

TEST_P(InSessionPasswordChangeManagerTest, TimePasses_NoUserActionTaken) {
  SetExpirationTime(base::Time::Now() + kOneYear + kAdvanceWarningTime);
  MaybeShowExpiryNotificationAndWait();

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

TEST_P(InSessionPasswordChangeManagerTest, TimePasses_NotificationDismissed) {
  SetExpirationTime(base::Time::Now() + kOneYear + kAdvanceWarningTime / 2);
  MaybeShowExpiryNotificationAndWait();

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
  test_environment_.FastForwardBy(kThreeYears);
  ExpectNotificationAndDismiss();
  test_environment_.FastForwardBy(kOneDay);
  ExpectNotificationAndDismiss();
}

TEST_P(InSessionPasswordChangeManagerTest, ReshowOnUnlock) {
  SetExpirationTime(base::Time::Now() + kAdvanceWarningTime / 2);
  MaybeShowExpiryNotificationAndWait();

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
  // OnScreenUnlocked calls MaybeShowExpiryNotification, which performs an
  // asynchronous auth factor check when kManagedLocalPinAndPassword is enabled.
  // We need to run until idle to ensure this check completes and the
  // notification is reshown.
  test_environment_.RunUntilIdle();
  EXPECT_TRUE(Notification().has_value());
  EXPECT_NE(first_shown_at, Notification()->timestamp());
}

TEST_P(InSessionPasswordChangeManagerTest, DontReshowWhenDismissed) {
  SetExpirationTime(base::Time::Now() + kAdvanceWarningTime / 2);
  MaybeShowExpiryNotificationAndWait();

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

class InSessionPasswordChangeManagerLocalFactorCheckTest
    : public InSessionPasswordChangeManagerTestBase {
 public:
  InSessionPasswordChangeManagerLocalFactorCheckTest() {
    feature_list_.InitAndEnableFeature(features::kManagedLocalPinAndPassword);
  }
};

TEST_F(InSessionPasswordChangeManagerLocalFactorCheckTest,
       MaybeShow_LocalFactorIsPin) {
  // Add only a PIN.
  const auto cryptohome_id = cryptohome::CreateAccountIdentifierFromAccountId(
      user_manager::StubAccountId());
  FakeUserDataAuthClient::TestApi::Get()->AddExistingUser(cryptohome_id);

  user_data_auth::AuthFactor factor;
  factor.set_type(user_data_auth::AUTH_FACTOR_TYPE_PIN);
  factor.set_label("pin");
  user_data_auth::AuthInput input;
  input.mutable_pin_input()->set_secret("123456");
  FakeUserDataAuthClient::TestApi::Get()->AddAuthFactor(cryptohome_id, factor,
                                                        input);

  SetExpirationTime(base::Time::Now() - kOneYear);  // Expired last year.
  MaybeShowExpiryNotificationAndWait();

  // Notification is NOT shown because there is no online password.
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(InSessionPasswordChangeManagerLocalFactorCheckTest,
       MaybeShow_LocalPassword) {
  // Add only a local password.
  const auto cryptohome_id = cryptohome::CreateAccountIdentifierFromAccountId(
      user_manager::StubAccountId());
  FakeUserDataAuthClient::TestApi::Get()->AddExistingUser(cryptohome_id);

  user_data_auth::AuthFactor factor;
  factor.set_type(user_data_auth::AUTH_FACTOR_TYPE_PASSWORD);
  factor.set_label(ash::kCryptohomeLocalPasswordKeyLabel);
  user_data_auth::AuthInput input;
  input.mutable_password_input()->set_secret("local-secret");
  FakeUserDataAuthClient::TestApi::Get()->AddAuthFactor(cryptohome_id, factor,
                                                        input);

  SetExpirationTime(base::Time::Now() - kOneYear);  // Expired last year.
  MaybeShowExpiryNotificationAndWait();

  // Notification is NOT shown because it's a local password, not an online
  // password.
  EXPECT_FALSE(Notification().has_value());
}
INSTANTIATE_TEST_SUITE_P(All,
                         InSessionPasswordChangeManagerTest,
                         testing::Bool());

}  // namespace ash
