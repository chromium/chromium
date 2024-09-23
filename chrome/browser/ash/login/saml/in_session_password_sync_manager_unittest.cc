// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/lock/online_reauth/lock_screen_reauth_manager.h"
#include "chrome/browser/ash/login/lock/online_reauth/lock_screen_reauth_manager_factory.h"
#include "chrome/browser/ash/login/saml/mock_lock_handler.h"
#include "chrome/browser/ash/login/saml/password_sync_token_fetcher.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

const char kSAMLUserId1[] = "12345";
const char kSAMLUserEmail1[] = "alice@corp.example.com";

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
  void SetReauthRequiredBySamlTokenMismatch();

  bool IsTokenFetcherCreated();
  bool IsReauthRequiredBySamlTokenMismatch();

  const AccountId saml_login_account_id1_ =
      AccountId::FromUserEmailGaiaId(kSAMLUserEmail1, kSAMLUserId1);

  content::BrowserTaskEnvironment test_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> primary_profile_ = nullptr;
  raw_ptr<LockScreenReauthManager> lock_screen_reauth_manager_ = nullptr;
  std::unique_ptr<MockLockHandler> lock_handler_;
  std::unique_ptr<InSessionPasswordSyncManager> manager_;
  std::unique_ptr<user_manager::KnownUser> known_user_;
};

InSessionPasswordSyncManagerTest::InSessionPasswordSyncManagerTest()
    : manager_(nullptr) {
  UserDataAuthClient::InitializeFake();

  known_user_ = std::make_unique<user_manager::KnownUser>(
      g_browser_process->local_state());
}

InSessionPasswordSyncManagerTest::~InSessionPasswordSyncManagerTest() {
  UserDataAuthClient::Shutdown();
}

void InSessionPasswordSyncManagerTest::SetUp() {
  ASSERT_TRUE(profile_manager_.SetUp());
  primary_profile_ = profile_manager_.CreateTestingProfile("test1");

  fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
      saml_login_account_id1_, /* is_affiliated = */ false,
      user_manager::UserType::kRegular, primary_profile_);
  fake_user_manager_->LoginUser(saml_login_account_id1_);
  // ActiveUser in FakeChromeUserManager needs to be set explicitly.
  fake_user_manager_->SwitchActiveUser(saml_login_account_id1_);
  ASSERT_TRUE(fake_user_manager_->GetActiveUser());
  lock_screen_reauth_manager_ =
      LockScreenReauthManagerFactory::GetForProfile(primary_profile_.get());
}

void InSessionPasswordSyncManagerTest::TearDown() {
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(nullptr);
}

void InSessionPasswordSyncManagerTest::CreateInSessionSyncManager() {
  manager_ = std::make_unique<InSessionPasswordSyncManager>(primary_profile_);
}

bool InSessionPasswordSyncManagerTest::IsTokenFetcherCreated() {
  return bool(manager_->password_sync_token_fetcher_);
}

void InSessionPasswordSyncManagerTest::SetReauthRequiredBySamlTokenMismatch() {
  lock_screen_reauth_manager_->is_reauth_required_by_saml_token_mismatch_ =
      true;
  EXPECT_TRUE(IsReauthRequiredBySamlTokenMismatch());
}

bool InSessionPasswordSyncManagerTest::IsReauthRequiredBySamlTokenMismatch() {
  return lock_screen_reauth_manager_
      ->is_reauth_required_by_saml_token_mismatch_;
}

TEST_F(InSessionPasswordSyncManagerTest, AuthenticateTokenNotInitialized) {
  CreateInSessionSyncManager();
  SetReauthRequiredBySamlTokenMismatch();
  // Simulate successful authentication.
  manager_->FetchTokenAsync();
  // Token wasn't initialized.
  manager_->OnApiCallFailed(PasswordSyncTokenFetcher::ErrorType::kGetNoList);
  EXPECT_TRUE(IsTokenFetcherCreated());
  manager_->OnTokenCreated(kFakeToken);
  EXPECT_FALSE(IsReauthRequiredBySamlTokenMismatch());
  EXPECT_FALSE(IsTokenFetcherCreated());
  const std::string* sync_token =
      known_user_->GetPasswordSyncToken(saml_login_account_id1_);
  ASSERT_TRUE(sync_token);
  EXPECT_EQ(kFakeToken, *sync_token);
}
}  // namespace ash
