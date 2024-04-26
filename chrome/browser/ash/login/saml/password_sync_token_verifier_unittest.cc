// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/password_sync_token_verifier.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/saml/password_sync_token_fetcher.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/prefs/pref_registry.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

const char kSAMLUserId1[] = "12345";
const char kSAMLUserEmail1[] = "alice@corp.example.com";

const char kSyncToken[] = "sync-token-1";

constexpr base::TimeDelta kSyncTokenCheckInterval = base::Minutes(6);

constexpr base::TimeDelta kSyncTokenCheckBelowInterval = base::Minutes(4);

}  // namespace

class PasswordSyncTokenVerifierTest : public testing::Test {
 protected:
  PasswordSyncTokenVerifierTest();
  ~PasswordSyncTokenVerifierTest() override;

  // testing::Test:
  void SetUp() override;

  void CreatePasswordSyncTokenVerifier();
  void DestroyPasswordSyncTokenVerifier();
  void OnTokenVerified(bool is_verified);
  bool PasswordSyncTokenFetcherIsAllocated();

  const AccountId saml_login_account_id_ =
      AccountId::FromUserEmailGaiaId(kSAMLUserEmail1, kSAMLUserId1);

  content::BrowserTaskEnvironment test_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> primary_profile_ = nullptr;

  raw_ptr<FakeChromeUserManager, DanglingUntriaged> user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<PasswordSyncTokenVerifier> verifier_;
  std::unique_ptr<user_manager::KnownUser> known_user_;
};

PasswordSyncTokenVerifierTest::PasswordSyncTokenVerifierTest() {
  std::unique_ptr<FakeChromeUserManager> fake_user_manager =
      std::make_unique<FakeChromeUserManager>();
  scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
      std::move(fake_user_manager));

  user_manager_ =
      static_cast<FakeChromeUserManager*>(user_manager::UserManager::Get());
  known_user_ = std::make_unique<user_manager::KnownUser>(
      g_browser_process->local_state());
}

PasswordSyncTokenVerifierTest::~PasswordSyncTokenVerifierTest() {
  DestroyPasswordSyncTokenVerifier();
}

void PasswordSyncTokenVerifierTest::SetUp() {
  ASSERT_TRUE(profile_manager_.SetUp());
  primary_profile_ = profile_manager_.CreateTestingProfile("test1");

  user_manager_->AddUserWithAffiliationAndTypeAndProfile(
      saml_login_account_id_, /* is_affiliated = */ false,
      user_manager::UserType::kRegular, primary_profile_);
  user_manager_->LoginUser(saml_login_account_id_);
  // ActiveUser in FakeChromeUserManager needs to be set explicitly.
  user_manager_->SwitchActiveUser(saml_login_account_id_);
  ASSERT_TRUE(user_manager_->GetActiveUser());
  primary_profile_->GetPrefs()->SetBoolean(
      prefs::kSamlInSessionPasswordChangeEnabled, true);
}

void PasswordSyncTokenVerifierTest::CreatePasswordSyncTokenVerifier() {
  DestroyPasswordSyncTokenVerifier();
  verifier_ = std::make_unique<PasswordSyncTokenVerifier>(primary_profile_);
}

void PasswordSyncTokenVerifierTest::DestroyPasswordSyncTokenVerifier() {
  if (verifier_) {
    verifier_->Shutdown();
    verifier_.reset();
  }
}

void PasswordSyncTokenVerifierTest::OnTokenVerified(bool is_verified) {
  verifier_->OnTokenVerified(is_verified);
}

TEST_F(PasswordSyncTokenVerifierTest, EmptySyncToken) {
  CreatePasswordSyncTokenVerifier();
  verifier_->CheckForPasswordNotInSync();
  OnTokenVerified(false);
  EXPECT_TRUE(user_manager_->GetActiveUser()->force_online_signin());
}

TEST_F(PasswordSyncTokenVerifierTest, SyncTokenValidationPassed) {
  known_user_->SetPasswordSyncToken(saml_login_account_id_, kSyncToken);
  CreatePasswordSyncTokenVerifier();
  verifier_->CheckForPasswordNotInSync();
  OnTokenVerified(true);
  EXPECT_FALSE(user_manager_->GetActiveUser()->force_online_signin());
}

TEST_F(PasswordSyncTokenVerifierTest, SyncTokenValidationFailed) {
  known_user_->SetPasswordSyncToken(saml_login_account_id_, kSyncToken);
  CreatePasswordSyncTokenVerifier();
  verifier_->CheckForPasswordNotInSync();
  OnTokenVerified(false);
  EXPECT_TRUE(user_manager_->GetActiveUser()->force_online_signin());
}

TEST_F(PasswordSyncTokenVerifierTest, SyncTokenValidationAfterDelay) {
  known_user_->SetPasswordSyncToken(saml_login_account_id_, kSyncToken);
  CreatePasswordSyncTokenVerifier();
  verifier_->CheckForPasswordNotInSync();
  OnTokenVerified(true);
  EXPECT_FALSE(user_manager_->GetActiveUser()->force_online_signin());
  test_environment_.FastForwardBy(kSyncTokenCheckInterval);
  OnTokenVerified(false);
  EXPECT_TRUE(user_manager_->GetActiveUser()->force_online_signin());
}

TEST_F(PasswordSyncTokenVerifierTest, SyncTokenNoRecheckExecuted) {
  known_user_->SetPasswordSyncToken(saml_login_account_id_, kSyncToken);
  CreatePasswordSyncTokenVerifier();
  verifier_->CheckForPasswordNotInSync();
  OnTokenVerified(true);
  EXPECT_FALSE(user_manager_->GetActiveUser()->force_online_signin());
  known_user_->SetPasswordSyncToken(saml_login_account_id_, std::string());
  test_environment_.FastForwardBy(kSyncTokenCheckBelowInterval);
  EXPECT_FALSE(user_manager_->GetActiveUser()->force_online_signin());
}

TEST_F(PasswordSyncTokenVerifierTest, PasswordChangePolicyNotSet) {
  primary_profile_->GetPrefs()->SetBoolean(
      prefs::kSamlInSessionPasswordChangeEnabled, false);
  known_user_->SetPasswordSyncToken(saml_login_account_id_, kSyncToken);
  CreatePasswordSyncTokenVerifier();
  verifier_->CheckForPasswordNotInSync();
  OnTokenVerified(true);
  known_user_->SetPasswordSyncToken(saml_login_account_id_, std::string());
  test_environment_.FastForwardBy(kSyncTokenCheckInterval);
  EXPECT_FALSE(user_manager_->GetActiveUser()->force_online_signin());
}

TEST_F(PasswordSyncTokenVerifierTest, SyncTokenNotSet) {
  CreatePasswordSyncTokenVerifier();
  verifier_->FetchSyncTokenOnReauth();
  verifier_->OnTokenFetched(kSyncToken);
  EXPECT_EQ(*known_user_->GetPasswordSyncToken(saml_login_account_id_),
            kSyncToken);
}

TEST_F(PasswordSyncTokenVerifierTest, InitialSyncTokenListEmpty) {
  CreatePasswordSyncTokenVerifier();
  verifier_->FetchSyncTokenOnReauth();
  verifier_->OnApiCallFailed(PasswordSyncTokenFetcher::ErrorType::kGetNoList);
  verifier_->OnTokenCreated(kSyncToken);
  EXPECT_EQ(*known_user_->GetPasswordSyncToken(saml_login_account_id_),
            kSyncToken);
}

TEST_F(PasswordSyncTokenVerifierTest, SyncTokenInitForUser) {
  CreatePasswordSyncTokenVerifier();
  verifier_->FetchSyncTokenOnReauth();
  // Token API not initilized for the user - request token creation.
  verifier_->OnTokenFetched(std::string());
  verifier_->OnTokenCreated(kSyncToken);
  EXPECT_EQ(*known_user_->GetPasswordSyncToken(saml_login_account_id_),
            kSyncToken);
  // Start regular polling after session init.
  test_environment_.FastForwardBy(kSyncTokenCheckInterval);
  OnTokenVerified(true);
  EXPECT_FALSE(user_manager_->GetActiveUser()->force_online_signin());
}

TEST_F(PasswordSyncTokenVerifierTest, SyncTokenPrefsAreNotSyncable) {
  CreatePasswordSyncTokenVerifier();
  EXPECT_EQ(primary_profile_->GetPrefs()
                ->FindPreference(prefs::kSamlInSessionPasswordChangeEnabled)
                ->registration_flags(),
            PrefRegistry::NO_REGISTRATION_FLAGS);
}

TEST_F(PasswordSyncTokenVerifierTest, ValidateSyncTokenHistogram) {
  base::HistogramTester histogram_tester;
  CreatePasswordSyncTokenVerifier();
  verifier_->RecordTokenPollingStart();
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.SAML.InSessionPasswordSyncEvent", 0, 1);
}

}  // namespace ash
