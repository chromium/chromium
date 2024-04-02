// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/lock/online_reauth/lock_screen_reauth_manager.h"
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/saml/mock_lock_handler.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"

namespace ash {
namespace {
const char kSAMLUserId1[] = "12345";
const char kSAMLUserEmail1[] = "alice@corp.example.com";

const char kSAMLUserId2[] = "67891";
const char kSAMLUserEmail2[] = "bob@corp.example.com";

constexpr base::TimeDelta kSamlOnlineShortDelay = base::Seconds(10);
}  // namespace

class LockScreenReauthManagerTest : public testing::Test {
 protected:
  LockScreenReauthManagerTest();
  ~LockScreenReauthManagerTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  void CreateLockScreenReauthManager();
  void DestroyLockScreenReauthManager();

  bool IsReauthRequiredBySamlTokenMismatch();
  bool IsReauthRequiredBySamlTimeLimitPolicy();

  void LockScreen();

  const AccountId saml_login_account_id1_ =
      AccountId::FromUserEmailGaiaId(kSAMLUserEmail1, kSAMLUserId1);

  const AccountId saml_login_account_id2_ =
      AccountId::FromUserEmailGaiaId(kSAMLUserEmail2, kSAMLUserId2);

  content::BrowserTaskEnvironment test_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> primary_profile_ = nullptr;
  raw_ptr<TestingProfile> secondary_profile_ = nullptr;

  std::unique_ptr<MockLockHandler> lock_handler_;
  std::unique_ptr<LockScreenReauthManager> manager_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<user_manager::KnownUser> known_user_;
};

LockScreenReauthManagerTest::LockScreenReauthManagerTest() : manager_(nullptr) {
  UserDataAuthClient::InitializeFake();

  known_user_ = std::make_unique<user_manager::KnownUser>(
      g_browser_process->local_state());
}

LockScreenReauthManagerTest::~LockScreenReauthManagerTest() {
  DestroyLockScreenReauthManager();
  UserDataAuthClient::Shutdown();
}

void LockScreenReauthManagerTest::SetUp() {
  ASSERT_TRUE(profile_manager_.SetUp());
  primary_profile_ = profile_manager_.CreateTestingProfile("test1");
  secondary_profile_ = profile_manager_.CreateTestingProfile("test2");

  fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
      saml_login_account_id1_, /* is_affiliated = */ false,
      user_manager::UserType::kRegular, primary_profile_);
  fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
      saml_login_account_id2_, /* is_affiliated = */ false,
      user_manager::UserType::kRegular, secondary_profile_);
  fake_user_manager_->AddUser(saml_login_account_id2_);
  fake_user_manager_->LoginUser(saml_login_account_id1_);
  // ActiveUser in FakeChromeUserManager needs to be set explicitly.
  fake_user_manager_->SwitchActiveUser(saml_login_account_id1_);
  ASSERT_TRUE(fake_user_manager_->GetActiveUser());
}

void LockScreenReauthManagerTest::TearDown() {
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(nullptr);
}

void LockScreenReauthManagerTest::CreateLockScreenReauthManager() {
  DestroyLockScreenReauthManager();
  manager_ = std::make_unique<LockScreenReauthManager>(primary_profile_);
  manager_->SetClockForTesting(test_environment_.GetMockClock());
}

void LockScreenReauthManagerTest::DestroyLockScreenReauthManager() {
  if (manager_) {
    manager_->Shutdown();
    manager_ = nullptr;
  }
}

void LockScreenReauthManagerTest::LockScreen() {
  lock_handler_ = std::make_unique<MockLockHandler>();
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(lock_handler_.get());
}

bool LockScreenReauthManagerTest::IsReauthRequiredBySamlTokenMismatch() {
  return manager_->is_reauth_required_by_saml_token_mismatch_;
}

bool LockScreenReauthManagerTest::IsReauthRequiredBySamlTimeLimitPolicy() {
  return manager_->is_reauth_required_by_saml_time_limit_policy_;
}

TEST_F(LockScreenReauthManagerTest, ReauthenticateRequiredByTimelimitPolicy) {
  CreateLockScreenReauthManager();
  fake_user_manager_->SaveForceOnlineSignin(saml_login_account_id1_, true);
  manager_->MaybeForceReauthOnLockScreen(
      ReauthReason::kSamlLockScreenReauthPolicy);
  EXPECT_TRUE(IsReauthRequiredBySamlTimeLimitPolicy());
}

TEST_F(LockScreenReauthManagerTest, ReauthenticateResetByToken) {
  primary_profile_->GetPrefs()->SetBoolean(
      prefs::kLockScreenReauthenticationEnabled, true);
  CreateLockScreenReauthManager();
  fake_user_manager_->SaveForceOnlineSignin(saml_login_account_id1_, true);
  manager_->MaybeForceReauthOnLockScreen(
      ReauthReason::kSamlPasswordSyncTokenValidationFailed);
  EXPECT_TRUE(IsReauthRequiredBySamlTokenMismatch());
}

TEST_F(LockScreenReauthManagerTest, ReauthenticateSetOnLock) {
  primary_profile_->GetPrefs()->SetBoolean(
      prefs::kLockScreenReauthenticationEnabled, true);
  CreateLockScreenReauthManager();
  LockScreen();
  EXPECT_CALL(*lock_handler_,
              SetAuthType(saml_login_account_id1_,
                          proximity_auth::mojom::AuthType::ONLINE_SIGN_IN,
                          std::u16string()))
      .Times(1);
  fake_user_manager_->SaveForceOnlineSignin(saml_login_account_id1_, true);
  manager_->MaybeForceReauthOnLockScreen(
      ReauthReason::kSamlLockScreenReauthPolicy);
  EXPECT_TRUE(IsReauthRequiredBySamlTimeLimitPolicy());
}

// User tries to unlock the screen using valid SAML credentials but not for the
// user who locked the screen. As a result screen remains locked.
TEST_F(LockScreenReauthManagerTest, AuthenticateWithIncorrectUser) {
  primary_profile_->GetPrefs()->SetBoolean(
      prefs::kLockScreenReauthenticationEnabled, true);
  CreateLockScreenReauthManager();
  LockScreen();
  EXPECT_CALL(*lock_handler_,
              SetAuthType(saml_login_account_id1_,
                          proximity_auth::mojom::AuthType::ONLINE_SIGN_IN,
                          std::u16string()))
      .Times(1);
  EXPECT_CALL(*lock_handler_, Unlock(saml_login_account_id1_)).Times(0);
  fake_user_manager_->SaveForceOnlineSignin(saml_login_account_id1_, true);
  manager_->MaybeForceReauthOnLockScreen(
      ReauthReason::kSamlLockScreenReauthPolicy);
  EXPECT_TRUE(IsReauthRequiredBySamlTimeLimitPolicy());
  UserContext user_context(user_manager::UserType::kRegular,
                           saml_login_account_id2_);
  EXPECT_DEATH(manager_->OnAuthSuccess(user_context), "");

  EXPECT_TRUE(IsReauthRequiredBySamlTimeLimitPolicy());
  EXPECT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
}

TEST_F(LockScreenReauthManagerTest, AuthenticateWithCorrectUser) {
  base::Time now = test_environment_.GetMockClock()->Now();
  known_user_->SetLastOnlineSignin(saml_login_account_id1_, now);
  known_user_->SetOfflineSigninLimit(saml_login_account_id1_,
                                     kSamlOnlineShortDelay);
  base::Time expected_signin_time = now + kSamlOnlineShortDelay;

  CreateLockScreenReauthManager();
  LockScreen();
  EXPECT_CALL(*lock_handler_,
              SetAuthType(saml_login_account_id1_,
                          proximity_auth::mojom::AuthType::ONLINE_SIGN_IN,
                          std::u16string()))
      .Times(1);
  EXPECT_CALL(*lock_handler_, Unlock(saml_login_account_id1_)).Times(1);
  fake_user_manager_->SaveForceOnlineSignin(saml_login_account_id1_, true);
  test_environment_.FastForwardBy(kSamlOnlineShortDelay);
  manager_->MaybeForceReauthOnLockScreen(
      ReauthReason::kSamlLockScreenReauthPolicy);
  EXPECT_TRUE(IsReauthRequiredBySamlTimeLimitPolicy());
  UserContext user_context(user_manager::UserType::kRegular,
                           saml_login_account_id1_);
  manager_->OnAuthSuccess(user_context);
  EXPECT_FALSE(IsReauthRequiredBySamlTimeLimitPolicy());
  now = known_user_->GetLastOnlineSignin(saml_login_account_id1_);
  EXPECT_EQ(now, expected_signin_time);
}

TEST_F(LockScreenReauthManagerTest, PolicySetToFalse) {
  primary_profile_->GetPrefs()->SetBoolean(
      prefs::kLockScreenReauthenticationEnabled, false);
  CreateLockScreenReauthManager();
  EXPECT_FALSE(manager_->ShouldPasswordSyncTriggerReauth());
}

TEST_F(LockScreenReauthManagerTest, PolicyNotSet) {
  CreateLockScreenReauthManager();
  EXPECT_FALSE(manager_->ShouldPasswordSyncTriggerReauth());
}

}  // namespace ash
