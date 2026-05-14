// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/password_sync_token_verifier.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_login_pref_names.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/saml/password_sync_token_fetcher.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr GaiaId::Literal kSAMLUserId1("12345");
constexpr char kSAMLUserEmail1[] = "alice@corp.example.com";

constexpr char kSyncToken[] = "sync-token-1";

constexpr base::TimeDelta kSyncTokenCheckInterval = base::Minutes(6);

constexpr base::TimeDelta kSyncTokenCheckBelowInterval = base::Minutes(4);

}  // namespace

class PasswordSyncTokenVerifierTest : public testing::TestWithParam<bool> {
 protected:
  PasswordSyncTokenVerifierTest();
  ~PasswordSyncTokenVerifierTest() override;

  // testing::Test:
  void SetUp() override;

  void CreatePasswordSyncTokenVerifier();
  void DestroyPasswordSyncTokenVerifier();
  void OnTokenVerified(bool is_verified);
  bool IsPasswordSyncTokenFetcherInitialized();
  void CheckForPasswordNotInSyncAndWait();
  void FetchSyncTokenOnReauthAndWait();

  const AccountId saml_login_account_id_ =
      AccountId::FromUserEmailGaiaId(kSAMLUserEmail1, kSAMLUserId1);

  content::BrowserTaskEnvironment test_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> primary_profile_ = nullptr;

  user_manager::TypedScopedUserManager<FakeChromeUserManager> user_manager_;
  std::unique_ptr<PasswordSyncTokenVerifier> verifier_;
  std::unique_ptr<user_manager::KnownUser> known_user_;
  base::test::ScopedFeatureList features_;
};

PasswordSyncTokenVerifierTest::PasswordSyncTokenVerifierTest()
    : user_manager_(std::make_unique<FakeChromeUserManager>()) {
  if (GetParam()) {
    features_.InitWithFeatures(
        /*enabled_features=*/{features::kManagedLocalPinAndPassword,
                              features::kRecoveryFlowReorder},
        /*disabled_features=*/{});
  } else {
    features_.InitWithFeatures(/*enabled_features=*/{}, /*disabled_features=*/{
                                   features::kManagedLocalPinAndPassword,
                                   features::kRecoveryFlowReorder});
  }

  known_user_ = std::make_unique<user_manager::KnownUser>(
      g_browser_process->local_state());
}

PasswordSyncTokenVerifierTest::~PasswordSyncTokenVerifierTest() {
  DestroyPasswordSyncTokenVerifier();
  FakeUserDataAuthClient::Shutdown();
}

void PasswordSyncTokenVerifierTest::SetUp() {
  FakeUserDataAuthClient::InitializeFake();
  FakeUserDataAuthClient::Get()->set_add_default_password_factor(true);
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
  // User needs to exist in FakeUserDataAuthClient to clear the auth factors.
  const auto account_id =
      cryptohome::CreateAccountIdentifierFromAccountId(saml_login_account_id_);
  FakeUserDataAuthClient::TestApi::Get()->AddExistingUser(account_id);
}

void PasswordSyncTokenVerifierTest::CreatePasswordSyncTokenVerifier() {
  DestroyPasswordSyncTokenVerifier();
  verifier_ = std::make_unique<PasswordSyncTokenVerifier>(
      TestingBrowserProcess::GetGlobal()->local_state(), primary_profile_);
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

bool PasswordSyncTokenVerifierTest::IsPasswordSyncTokenFetcherInitialized() {
  return !!verifier_->password_sync_token_fetcher_;
}

void PasswordSyncTokenVerifierTest::CheckForPasswordNotInSyncAndWait() {
  verifier_->CheckForPasswordNotInSync();
  // We use `RunUntilIdle` to wait for the async auth factor check that
  // happens before the token check when kManagedLocalPinAndPassword
  // feature is enabled.
  test_environment_.RunUntilIdle();
}

void PasswordSyncTokenVerifierTest::FetchSyncTokenOnReauthAndWait() {
  verifier_->FetchSyncTokenOnReauth();
  // We use `RunUntilIdle` to wait for the async auth factor check that
  // happens before the token fetch when kManagedLocalPinAndPassword
  // feature is enabled.
  test_environment_.RunUntilIdle();
}

TEST_P(PasswordSyncTokenVerifierTest, EmptySyncToken) {
  CreatePasswordSyncTokenVerifier();
  CheckForPasswordNotInSyncAndWait();
  OnTokenVerified(false);
  EXPECT_TRUE(user_manager_->GetActiveUser()->force_online_signin());
}

TEST_P(PasswordSyncTokenVerifierTest, SyncTokenValidationPassed) {
  known_user_->SetPasswordSyncToken(saml_login_account_id_, kSyncToken);
  CreatePasswordSyncTokenVerifier();
  CheckForPasswordNotInSyncAndWait();
  OnTokenVerified(true);
  EXPECT_FALSE(user_manager_->GetActiveUser()->force_online_signin());
}

TEST_P(PasswordSyncTokenVerifierTest, SyncTokenValidationFailed) {
  known_user_->SetPasswordSyncToken(saml_login_account_id_, kSyncToken);
  CreatePasswordSyncTokenVerifier();
  CheckForPasswordNotInSyncAndWait();
  OnTokenVerified(false);
  EXPECT_TRUE(user_manager_->GetActiveUser()->force_online_signin());
}

TEST_P(PasswordSyncTokenVerifierTest, SyncTokenValidationAfterDelay) {
  known_user_->SetPasswordSyncToken(saml_login_account_id_, kSyncToken);
  CreatePasswordSyncTokenVerifier();
  CheckForPasswordNotInSyncAndWait();
  OnTokenVerified(true);
  EXPECT_FALSE(user_manager_->GetActiveUser()->force_online_signin());
  test_environment_.FastForwardBy(kSyncTokenCheckInterval);
  OnTokenVerified(false);
  EXPECT_TRUE(user_manager_->GetActiveUser()->force_online_signin());
}

TEST_P(PasswordSyncTokenVerifierTest, SyncTokenNoRecheckExecuted) {
  known_user_->SetPasswordSyncToken(saml_login_account_id_, kSyncToken);
  CreatePasswordSyncTokenVerifier();
  CheckForPasswordNotInSyncAndWait();
  OnTokenVerified(true);
  EXPECT_FALSE(user_manager_->GetActiveUser()->force_online_signin());
  known_user_->SetPasswordSyncToken(saml_login_account_id_, std::string());
  test_environment_.FastForwardBy(kSyncTokenCheckBelowInterval);
  EXPECT_FALSE(user_manager_->GetActiveUser()->force_online_signin());
}

TEST_P(PasswordSyncTokenVerifierTest, PasswordChangePolicyNotSet) {
  primary_profile_->GetPrefs()->SetBoolean(
      prefs::kSamlInSessionPasswordChangeEnabled, false);
  known_user_->SetPasswordSyncToken(saml_login_account_id_, kSyncToken);
  CreatePasswordSyncTokenVerifier();
  CheckForPasswordNotInSyncAndWait();
  OnTokenVerified(true);
  known_user_->SetPasswordSyncToken(saml_login_account_id_, std::string());
  test_environment_.FastForwardBy(kSyncTokenCheckInterval);
  EXPECT_FALSE(user_manager_->GetActiveUser()->force_online_signin());
}

TEST_P(PasswordSyncTokenVerifierTest, SyncTokenNotSet) {
  CreatePasswordSyncTokenVerifier();
  FetchSyncTokenOnReauthAndWait();
  verifier_->OnTokenFetched(kSyncToken);
  EXPECT_EQ(*known_user_->GetPasswordSyncToken(saml_login_account_id_),
            kSyncToken);
}

TEST_P(PasswordSyncTokenVerifierTest, InitialSyncTokenListEmpty) {
  CreatePasswordSyncTokenVerifier();
  FetchSyncTokenOnReauthAndWait();
  verifier_->OnApiCallFailed(PasswordSyncTokenFetcher::ErrorType::kGetNoList);
  verifier_->OnTokenCreated(kSyncToken);
  EXPECT_EQ(*known_user_->GetPasswordSyncToken(saml_login_account_id_),
            kSyncToken);
}

TEST_P(PasswordSyncTokenVerifierTest, SyncTokenInitForUser) {
  CreatePasswordSyncTokenVerifier();
  FetchSyncTokenOnReauthAndWait();
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

TEST_P(PasswordSyncTokenVerifierTest, SyncTokenPrefsAreNotSyncable) {
  CreatePasswordSyncTokenVerifier();
  EXPECT_EQ(primary_profile_->GetPrefs()
                ->FindPreference(prefs::kSamlInSessionPasswordChangeEnabled)
                ->registration_flags(),
            PrefRegistry::NO_REGISTRATION_FLAGS);
}

TEST_P(PasswordSyncTokenVerifierTest, ValidateSyncTokenHistogram) {
  base::HistogramTester histogram_tester;
  CreatePasswordSyncTokenVerifier();
  verifier_->RecordTokenPollingStart();
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.SAML.InSessionPasswordSyncEvent", 0, 1);
}

TEST_P(PasswordSyncTokenVerifierTest, NoGaiaPasswordWithFlagEnabled) {
  if (!GetParam()) {
    return;
  }
  // Remove all auth factors including the default Gaia password.
  FakeUserDataAuthClient::TestApi::Get()->ClearAuthFactors(
      cryptohome::CreateAccountIdentifierFromAccountId(saml_login_account_id_));

  known_user_->SetPasswordSyncToken(saml_login_account_id_, kSyncToken);
  CreatePasswordSyncTokenVerifier();
  CheckForPasswordNotInSyncAndWait();

  // Fetcher should not be allocated because there is no Gaia password.
  EXPECT_FALSE(IsPasswordSyncTokenFetcherInitialized());
}

TEST_P(PasswordSyncTokenVerifierTest, NoGaiaPasswordOnReauthWithFlagEnabled) {
  if (!GetParam()) {
    return;
  }
  // Remove all auth factors including the default Gaia password.
  FakeUserDataAuthClient::TestApi::Get()->ClearAuthFactors(
      cryptohome::CreateAccountIdentifierFromAccountId(saml_login_account_id_));

  CreatePasswordSyncTokenVerifier();
  FetchSyncTokenOnReauthAndWait();

  // Fetcher should not be allocated because there is no Gaia password.
  EXPECT_FALSE(IsPasswordSyncTokenFetcherInitialized());
}

INSTANTIATE_TEST_SUITE_P(PasswordSyncTokenVerifierTestInstantiation,
                         PasswordSyncTokenVerifierTest,
                         ::testing::Bool());

}  // namespace ash
