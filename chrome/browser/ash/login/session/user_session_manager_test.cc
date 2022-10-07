// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/session/user_session_manager.h"

#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/language/core/browser/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

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
      : profile_manager_(std::make_unique<TestingProfileManager>(
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
    FakeChromeUserManager* user_manager =
        static_cast<FakeChromeUserManager*>(user_manager::UserManager::Get());
    test_user_ = user_manager->AddPublicAccountUser(account_id);

    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    TestingProfile* profile = profile_manager_->CreateTestingProfile(
        account_id.GetUserEmail(), std::move(prefs), u"Test profile",
        /*avatar_id=*/1, TestingProfile::TestingFactories());

    user_manager->LoginUser(account_id);
    return profile;
  }

  std::unique_ptr<TestUserSessionManager> user_session_manager_;

  // Allows UserSessionManager to request the NetworkConnectionTracker in its
  // constructor.
  content::BrowserTaskEnvironment task_environment_;

  user_manager::ScopedUserManager scoped_user_manager_{
      std::make_unique<user_manager::FakeUserManager>()};

  std::unique_ptr<TestingProfileManager> profile_manager_;
  user_manager::User* test_user_;
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

}  // namespace
}  // namespace ash
