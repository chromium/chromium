// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/session/user_session_manager.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/language/core/browser/pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::IsEmpty;
using testing::Not;

namespace ash {
namespace {

constexpr char kFakePassword[] = "p4zzw0r(|";

// Publicly exposes lifetime methods. Note that the singleton instance
// UserSessionManager::GetInstance() can't be used since it would be reused
// between tests.
class TestUserSessionManager : public UserSessionManager {
 public:
  TestUserSessionManager() = default;
  ~TestUserSessionManager() override = default;
};

class UserSessionManagerTest : public testing::Test {
 public:
  UserSessionManagerTest()
      : fake_user_manager_(std::make_unique<FakeChromeUserManager>()),
        profile_manager_(std::make_unique<TestingProfileManager>(
            TestingBrowserProcess::GetGlobal())) {
    static_assert(
        static_cast<int>(
            UserSessionManager::PasswordConsumingService::kCount) == 2,
        "Update PasswordConsumerService_* tests");

    SessionManagerClient::InitializeFake();
    user_session_manager_ = std::make_unique<TestUserSessionManager>();
  }

  void SetUp() override { ASSERT_TRUE(profile_manager_->SetUp()); }

  UserSessionManagerTest(const UserSessionManagerTest&) = delete;
  UserSessionManagerTest& operator=(const UserSessionManagerTest&) = delete;

  ~UserSessionManagerTest() override {
    profile_manager_->DeleteAllTestingProfiles();
    user_session_manager_.reset();
    SessionManagerClient::Shutdown();
  }

 protected:
  void InitLoginPassword() {
    user_session_manager_->mutable_user_context_for_testing()->SetPasswordKey(
        Key(kFakePassword));
    EXPECT_FALSE(user_session_manager_->user_context()
                     .GetPasswordKey()
                     ->GetSecret()
                     .empty());
    EXPECT_TRUE(FakeSessionManagerClient::Get()->login_password().empty());
  }

  // Convenience shortcut to the login password stored in
  // `user_session_manager_`'s user context.
  const std::string& GetUserSessionManagerLoginPassword() const {
    return user_session_manager_->user_context().GetPasswordKey()->GetSecret();
  }

  // Creates a dummy user with a testing profile and logs in.
  TestingProfile* LoginTestUser() {
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId("demo@test.com", "demo_user"));
    // TODO(http://b/310599489): We are logging a Gaia type user as a Public
    // Session user here. This is inconsistent.
    test_user_ = fake_user_manager_->AddPublicAccountUser(account_id);

    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    TestingProfile* profile = profile_manager_->CreateTestingProfile(
        account_id.GetUserEmail(), std::move(prefs), u"Test profile",
        /*avatar_id=*/1, TestingProfile::TestingFactories());

    fake_user_manager_->LoginUser(account_id);
    return profile;
  }

  std::unique_ptr<TestUserSessionManager> user_session_manager_;

  // Allows UserSessionManager to request the NetworkConnectionTracker in its
  // constructor.
  content::BrowserTaskEnvironment task_environment_;

  user_manager::TypedScopedUserManager<FakeChromeUserManager>
      fake_user_manager_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<user_manager::User> test_user_;
};

// Calling VoteForSavingLoginPassword() with `save_password` set to false for
// all `PasswordConsumerService`s should not send the password to SessionManager
// and clear it from the user context.
TEST_F(UserSessionManagerTest, PasswordConsumerService_NoSave) {
  InitLoginPassword();
  user_session_manager_->set_start_session_type_for_testing(
      UserSessionManager::StartSessionType::kPrimary);

  // First service votes no: Should keep password in user context.
  user_session_manager_->VoteForSavingLoginPassword(
      UserSessionManager::PasswordConsumingService::kNetwork, false);
  EXPECT_TRUE(FakeSessionManagerClient::Get()->login_password().empty());
  EXPECT_EQ(kFakePassword, GetUserSessionManagerLoginPassword());

  // Second (last) service votes no: Should remove password from user context.
  user_session_manager_->VoteForSavingLoginPassword(
      UserSessionManager::PasswordConsumingService::kKerberos, false);
  EXPECT_TRUE(FakeSessionManagerClient::Get()->login_password().empty());
  EXPECT_TRUE(GetUserSessionManagerLoginPassword().empty());
}

// Calling VoteForSavingLoginPassword() with `save_password` set to true should
// send the password to SessionManager and clear it from the user context once
// all services have voted.
TEST_F(UserSessionManagerTest, PasswordConsumerService_Save) {
  InitLoginPassword();
  user_session_manager_->set_start_session_type_for_testing(
      UserSessionManager::StartSessionType::kPrimary);

  // First service votes yes: Should send password and remove from user context.
  user_session_manager_->VoteForSavingLoginPassword(
      UserSessionManager::PasswordConsumingService::kNetwork, true);
  EXPECT_EQ(kFakePassword, FakeSessionManagerClient::Get()->login_password());
  EXPECT_TRUE(GetUserSessionManagerLoginPassword().empty());

  // Second service votes yes: Shouldn't change anything.
  user_session_manager_->VoteForSavingLoginPassword(
      UserSessionManager::PasswordConsumingService::kKerberos, true);
  EXPECT_EQ(kFakePassword, FakeSessionManagerClient::Get()->login_password());
  EXPECT_TRUE(GetUserSessionManagerLoginPassword().empty());
}

// Calling OnPasswordConsumingServicePolicyParsed() with `save_password` set to
// false for one service, followed by true, should send the password to
// SessionManager on the second service and clear it from the user context.
TEST_F(UserSessionManagerTest, PasswordConsumerService_NoSave_Save) {
  InitLoginPassword();
  user_session_manager_->set_start_session_type_for_testing(
      UserSessionManager::StartSessionType::kPrimary);

  // First service votes no: Should keep password in user context.
  user_session_manager_->VoteForSavingLoginPassword(
      UserSessionManager::PasswordConsumingService::kNetwork, false);
  EXPECT_TRUE(FakeSessionManagerClient::Get()->login_password().empty());
  EXPECT_EQ(kFakePassword, GetUserSessionManagerLoginPassword());

  // Second service votes yes: Should save password and remove from user
  // context.
  user_session_manager_->VoteForSavingLoginPassword(
      UserSessionManager::PasswordConsumingService::kKerberos, true);
  EXPECT_EQ(kFakePassword, FakeSessionManagerClient::Get()->login_password());
  EXPECT_TRUE(GetUserSessionManagerLoginPassword().empty());
}

// Calling VoteForSavingLoginPassword() with `save_password` set to true should
// be ignored if a secondary user session is being started.
TEST_F(UserSessionManagerTest,
       PasswordConsumerService_NoSave_SecondarySession) {
  InitLoginPassword();
  user_session_manager_->set_start_session_type_for_testing(
      UserSessionManager::StartSessionType::kSecondary);

  // First service votes yes: Should send password and remove from user context.
  user_session_manager_->VoteForSavingLoginPassword(
      UserSessionManager::PasswordConsumingService::kNetwork, true);
  EXPECT_TRUE(FakeSessionManagerClient::Get()->login_password().empty());
}

TEST_F(UserSessionManagerTest, RespectLocale_WithProfileLocale) {
  TestingProfile* profile = LoginTestUser();

  profile->GetPrefs()->SetString(language::prefs::kApplicationLocale, "fr-CA");
  g_browser_process->SetApplicationLocale("fr");

  // Local state locale should be ignored.
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      language::prefs::kApplicationLocale, "es");

  user_session_manager_->RespectLocalePreference(profile, test_user_,
                                                 base::NullCallback());

  EXPECT_TRUE(profile->requested_locale().has_value());
  EXPECT_EQ("fr-CA", profile->requested_locale().value());
}

TEST_F(UserSessionManagerTest, RespectLocale_WithoutProfileLocale) {
  TestingProfile* profile = LoginTestUser();

  g_browser_process->SetApplicationLocale("fr");

  // Local state locale should be ignored.
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      language::prefs::kApplicationLocale, "es");

  user_session_manager_->RespectLocalePreference(profile, test_user_,
                                                 base::NullCallback());

  EXPECT_TRUE(profile->requested_locale().has_value());
  EXPECT_EQ("fr", profile->requested_locale().value());
}

TEST_F(UserSessionManagerTest, RespectLocale_Demo_WithProfileLocale) {
  TestingProfile* profile = LoginTestUser();
  // Enable Demo Mode.
  profile->ScopedCrosSettingsTestHelper()->InstallAttributes()->SetDemoMode();
  DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kOnline);

  profile->GetPrefs()->SetString(language::prefs::kApplicationLocale, "fr-CA");
  g_browser_process->SetApplicationLocale("fr");

  // Local state locale should be ignored.
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      language::prefs::kApplicationLocale, "es");

  user_session_manager_->RespectLocalePreference(profile, test_user_,
                                                 base::NullCallback());

  EXPECT_TRUE(profile->requested_locale().has_value());
  EXPECT_EQ("fr-CA", profile->requested_locale().value());
}

TEST_F(UserSessionManagerTest, RespectLocale_Demo_WithoutProfileLocale) {
  TestingProfile* profile = LoginTestUser();
  // Enable Demo Mode.
  profile->ScopedCrosSettingsTestHelper()->InstallAttributes()->SetDemoMode();
  DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kOnline);

  g_browser_process->SetApplicationLocale("fr");

  // Because it's Demo Mode and the profile pref local is empty, local state
  // locale should not be ignored.
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      language::prefs::kApplicationLocale, "fr-CA");

  user_session_manager_->RespectLocalePreference(profile, test_user_,
                                                 base::NullCallback());

  EXPECT_TRUE(profile->requested_locale().has_value());
  EXPECT_EQ("fr-CA", profile->requested_locale().value());
}

TEST_F(UserSessionManagerTest, InitializeDeviceIdForNewUsers) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kStableDeviceId);
  LoginTestUser();
  test::UserSessionManagerTestApi test_api(user_session_manager_.get());
  user_manager::KnownUser known_user(g_browser_process->local_state());
  user_session_manager_->mutable_user_context_for_testing()->SetAccountId(
      test_user_->GetAccountId());
  ASSERT_THAT(known_user.GetDeviceId(test_user_->GetAccountId()), IsEmpty());
  ASSERT_THAT(user_session_manager_->user_context().GetDeviceId(), IsEmpty());

  test_api.InitializeDeviceId(/*is_ephemeral_user=*/false, known_user);
  const std::string stored_device_id =
      known_user.GetDeviceId(test_user_->GetAccountId());

  EXPECT_THAT(stored_device_id, Not(IsEmpty()));
  EXPECT_THAT(stored_device_id,
              Eq(user_session_manager_->user_context().GetDeviceId()));
}

TEST_F(UserSessionManagerTest, InitializeDeviceIdForExistingUsers) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kStableDeviceId);
  LoginTestUser();
  test::UserSessionManagerTestApi test_api(user_session_manager_.get());
  user_manager::KnownUser known_user(g_browser_process->local_state());
  user_session_manager_->mutable_user_context_for_testing()->SetAccountId(
      test_user_->GetAccountId());
  const std::string device_id =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/false);
  // For an existing user, we should already have a device id on disk.
  known_user.SetDeviceId(test_user_->GetAccountId(), device_id);
  ASSERT_THAT(user_session_manager_->user_context().GetDeviceId(), IsEmpty());

  test_api.InitializeDeviceId(/*is_ephemeral_user=*/false, known_user);
  const std::string stored_device_id =
      known_user.GetDeviceId(test_user_->GetAccountId());

  EXPECT_THAT(stored_device_id, Eq(device_id));
  EXPECT_THAT(stored_device_id,
              Eq(user_session_manager_->user_context().GetDeviceId()));
}

}  // namespace
}  // namespace ash
