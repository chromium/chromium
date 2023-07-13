// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
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

const char kFakeToken[] = "fake-token";

}  // namespace

class InSessionPasswordSyncManagerTest : public testing::Test {
 protected:
  InSessionPasswordSyncManagerTest();
  ~InSessionPasswordSyncManagerTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  void CreateInSessionSyncManager();
  void DestroyInSessionSyncManager();

  InSessionPasswordSyncManager::ReauthenticationReason InSessionReauthReason();
  bool IsTokenFetcherCreated();
  void LockScreen();
  void UnlockScreen();

  const AccountId saml_login_account_id1_ =
      AccountId::FromUserEmailGaiaId(kSAMLUserEmail1, kSAMLUserId1);

  const AccountId saml_login_account_id2_ =
      AccountId::FromUserEmailGaiaId(kSAMLUserEmail2, kSAMLUserId2);

  content::BrowserTaskEnvironment test_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile, ExperimentalAsh> primary_profile_ = nullptr;
  raw_ptr<TestingProfile, ExperimentalAsh> secondary_profile_ = nullptr;

  raw_ptr<FakeChromeUserManager, ExperimentalAsh> user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<MockLockHandler> lock_handler_;
  std::unique_ptr<InSessionPasswordSyncManager> manager_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<user_manager::KnownUser> known_user_;
};

InSessionPasswordSyncManagerTest::InSessionPasswordSyncManagerTest()
    : manager_(nullptr) {
  UserDataAuthClient::InitializeFake();

  std::unique_ptr<FakeChromeUserManager> fake_user_manager =
      std::make_unique<FakeChromeUserManager>();
  scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
      std::move(fake_user_manager));

  user_manager_ =
      static_cast<FakeChromeUserManager*>(user_manager::UserManager::Get());
  known_user_ = std::make_unique<user_manager::KnownUser>(
      g_browser_process->local_state());
}

InSessionPasswordSyncManagerTest::~InSessionPasswordSyncManagerTest() {
  DestroyInSessionSyncManager();
  UserDataAuthClient::Shutdown();
}

void InSessionPasswordSyncManagerTest::SetUp() {
  ASSERT_TRUE(profile_manager_.SetUp());
  primary_profile_ = profile_manager_.CreateTestingProfile("test1");
  secondary_profile_ = profile_manager_.CreateTestingProfile("test2");

  user_manager_->AddUserWithAffiliationAndTypeAndProfile(
      saml_login_account_id1_, /* is_affiliated = */ false,
      user_manager::UserType::USER_TYPE_REGULAR, primary_profile_);
  user_manager_->AddUserWithAffiliationAndTypeAndProfile(
      saml_login_account_id2_, /* is_affiliated = */ false,
      user_manager::UserType::USER_TYPE_REGULAR, secondary_profile_);
  user_manager_->AddUser(saml_login_account_id2_);
  user_manager_->LoginUser(saml_login_account_id1_);
  // ActiveUser in FakeChromeUserManager needs to be set explicitly.
  user_manager_->SwitchActiveUser(saml_login_account_id1_);
  ASSERT_TRUE(user_manager_->GetActiveUser());
}

void InSessionPasswordSyncManagerTest::TearDown() {
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(nullptr);
}

void InSessionPasswordSyncManagerTest::CreateInSessionSyncManager() {
  DestroyInSessionSyncManager();
  manager_ = std::make_unique<InSessionPasswordSyncManager>(primary_profile_);
  manager_->SetClockForTesting(test_environment_.GetMockClock());
}

void InSessionPasswordSyncManagerTest::DestroyInSessionSyncManager() {
  if (manager_) {
    manager_->Shutdown();
    manager_ = nullptr;
  }
}

void InSessionPasswordSyncManagerTest::LockScreen() {
  lock_handler_ = std::make_unique<MockLockHandler>();
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(lock_handler_.get());
}

void InSessionPasswordSyncManagerTest::UnlockScreen() {
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(nullptr);
}

InSessionPasswordSyncManager::ReauthenticationReason
InSessionPasswordSyncManagerTest::InSessionReauthReason() {
  return manager_->lock_screen_reauth_reason_;
}

bool InSessionPasswordSyncManagerTest::IsTokenFetcherCreated() {
  return bool(manager_->password_sync_token_fetcher_);
}

TEST_F(InSessionPasswordSyncManagerTest, ReauthenticateSetInSession) {
  primary_profile_->GetPrefs()->SetBoolean(
      prefs::kLockScreenReauthenticationEnabled, true);
  CreateInSessionSyncManager();
  UnlockScreen();
  user_manager_->SaveForceOnlineSignin(saml_login_account_id1_, true);
  manager_->MaybeForceReauthOnLockScreen(
      InSessionPasswordSyncManager::ReauthenticationReason::kPolicy);
  EXPECT_EQ(InSessionReauthReason(),
            InSessionPasswordSyncManager::ReauthenticationReason::kPolicy);
}

TEST_F(InSessionPasswordSyncManagerTest, ReauthenticateResetByToken) {
  primary_profile_->GetPrefs()->SetBoolean(
      prefs::kLockScreenReauthenticationEnabled, true);
  CreateInSessionSyncManager();
  UnlockScreen();
  user_manager_->SaveForceOnlineSignin(saml_login_account_id1_, true);
  manager_->MaybeForceReauthOnLockScreen(
      InSessionPasswordSyncManager::ReauthenticationReason::kPolicy);
  manager_->MaybeForceReauthOnLockScreen(
      InSessionPasswordSyncManager::ReauthenticationReason::kInvalidToken);
  EXPECT_EQ(
      InSessionReauthReason(),
      InSessionPasswordSyncManager::ReauthenticationReason::kInvalidToken);
}

TEST_F(InSessionPasswordSyncManagerTest, ReauthenticateSetOnLock) {
  primary_profile_->GetPrefs()->SetBoolean(
      prefs::kLockScreenReauthenticationEnabled, true);
  CreateInSessionSyncManager();
  LockScreen();
  EXPECT_CALL(*lock_handler_,
              SetAuthType(saml_login_account_id1_,
                          proximity_auth::mojom::AuthType::ONLINE_SIGN_IN,
                          std::u16string()))
      .Times(1);
  user_manager_->SaveForceOnlineSignin(saml_login_account_id1_, true);
  manager_->MaybeForceReauthOnLockScreen(
      InSessionPasswordSyncManager::ReauthenticationReason::kPolicy);
  EXPECT_EQ(InSessionReauthReason(),
            InSessionPasswordSyncManager::ReauthenticationReason::kPolicy);
}

// User tries to unlock the screen using valid SAML credentials but not for the
// user who locked the screen. As a result screen remains locked and
// enforce_reauth_on_lock_ flag is still set.
TEST_F(InSessionPasswordSyncManagerTest, AuthenticateWithIncorrectUser) {
  primary_profile_->GetPrefs()->SetBoolean(
      prefs::kLockScreenReauthenticationEnabled, true);
  CreateInSessionSyncManager();
  LockScreen();
  EXPECT_CALL(*lock_handler_,
              SetAuthType(saml_login_account_id1_,
                          proximity_auth::mojom::AuthType::ONLINE_SIGN_IN,
                          std::u16string()))
      .Times(1);
  EXPECT_CALL(*lock_handler_, Unlock(saml_login_account_id1_)).Times(0);
  user_manager_->SaveForceOnlineSignin(saml_login_account_id1_, true);
  manager_->MaybeForceReauthOnLockScreen(
      InSessionPasswordSyncManager::ReauthenticationReason::kPolicy);
  EXPECT_EQ(InSessionReauthReason(),
            InSessionPasswordSyncManager::ReauthenticationReason::kPolicy);
  UserContext user_context(user_manager::USER_TYPE_REGULAR,
                           saml_login_account_id2_);
  manager_->OnAuthSuccess(user_context);
  EXPECT_EQ(InSessionReauthReason(),
            InSessionPasswordSyncManager::ReauthenticationReason::kPolicy);
  EXPECT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
}

TEST_F(InSessionPasswordSyncManagerTest, AuthenticateWithCorrectUser) {
  base::Time now = test_environment_.GetMockClock()->Now();
  known_user_->SetLastOnlineSignin(saml_login_account_id1_, now);
  known_user_->SetOfflineSigninLimit(saml_login_account_id1_,
                                     kSamlOnlineShortDelay);
  base::Time expected_signin_time = now + kSamlOnlineShortDelay;

  primary_profile_->GetPrefs()->SetBoolean(
      prefs::kLockScreenReauthenticationEnabled, true);
  CreateInSessionSyncManager();
  LockScreen();
  EXPECT_CALL(*lock_handler_,
              SetAuthType(saml_login_account_id1_,
                          proximity_auth::mojom::AuthType::ONLINE_SIGN_IN,
                          std::u16string()))
      .Times(1);
  EXPECT_CALL(*lock_handler_, Unlock(saml_login_account_id1_)).Times(1);
  user_manager_->SaveForceOnlineSignin(saml_login_account_id1_, true);
  test_environment_.FastForwardBy(kSamlOnlineShortDelay);
  manager_->MaybeForceReauthOnLockScreen(
      InSessionPasswordSyncManager::ReauthenticationReason::kPolicy);
  EXPECT_EQ(InSessionReauthReason(),
            InSessionPasswordSyncManager::ReauthenticationReason::kPolicy);
  UserContext user_context(user_manager::USER_TYPE_REGULAR,
                           saml_login_account_id1_);
  manager_->OnAuthSuccess(user_context);
  EXPECT_EQ(InSessionReauthReason(),
            InSessionPasswordSyncManager::ReauthenticationReason::kNone);
  now = known_user_->GetLastOnlineSignin(saml_login_account_id1_);
  EXPECT_EQ(now, expected_signin_time);
}

TEST_F(InSessionPasswordSyncManagerTest, AuthenticateTokenNotInitialized) {
  primary_profile_->GetPrefs()->SetBoolean(
      prefs::kLockScreenReauthenticationEnabled, true);
  CreateInSessionSyncManager();
  LockScreen();
  EXPECT_CALL(*lock_handler_,
              SetAuthType(saml_login_account_id1_,
                          proximity_auth::mojom::AuthType::ONLINE_SIGN_IN,
                          std::u16string()))
      .Times(1);
  EXPECT_CALL(*lock_handler_, Unlock(saml_login_account_id1_)).Times(1);
  user_manager_->SaveForceOnlineSignin(saml_login_account_id1_, true);
  manager_->MaybeForceReauthOnLockScreen(
      InSessionPasswordSyncManager::ReauthenticationReason::kInvalidToken);
  EXPECT_EQ(
      InSessionReauthReason(),
      InSessionPasswordSyncManager::ReauthenticationReason::kInvalidToken);
  UserContext user_context(user_manager::USER_TYPE_REGULAR,
                           saml_login_account_id1_);
  manager_->OnAuthSuccess(user_context);
  manager_->OnApiCallFailed(PasswordSyncTokenFetcher::ErrorType::kGetNoList);
  EXPECT_TRUE(IsTokenFetcherCreated());
  manager_->OnTokenCreated(kFakeToken);
  EXPECT_EQ(InSessionReauthReason(),
            InSessionPasswordSyncManager::ReauthenticationReason::kNone);
  EXPECT_FALSE(IsTokenFetcherCreated());
  const std::string* sync_token =
      known_user_->GetPasswordSyncToken(saml_login_account_id1_);
  ASSERT_TRUE(sync_token);
  EXPECT_EQ(kFakeToken, *sync_token);
}

TEST_F(InSessionPasswordSyncManagerTest, PolicySetToFalse) {
  primary_profile_->GetPrefs()->SetBoolean(
      prefs::kLockScreenReauthenticationEnabled, false);
  CreateInSessionSyncManager();
  EXPECT_FALSE(manager_->IsLockReauthEnabled());
}

TEST_F(InSessionPasswordSyncManagerTest, PolicyNotSet) {
  CreateInSessionSyncManager();
  EXPECT_FALSE(manager_->IsLockReauthEnabled());
}

}  // namespace ash
