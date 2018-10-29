// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/oauth2_token_service_delegate.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/stl_util.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/account_manager/account_manager.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "google_apis/gaia/oauth2_token_service_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

using account_manager::AccountType::ACCOUNT_TYPE_GAIA;
using account_manager::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY;

class AccessTokenConsumer : public OAuth2AccessTokenConsumer {
 public:
  AccessTokenConsumer() = default;
  ~AccessTokenConsumer() override = default;

  void OnGetTokenSuccess(const TokenResponse& token_response) override {
    ++num_access_token_fetch_success_;
  }

  void OnGetTokenFailure(const GoogleServiceAuthError& error) override {
    ++num_access_token_fetch_failure_;
  }

  int num_access_token_fetch_success_ = 0;
  int num_access_token_fetch_failure_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessTokenConsumer);
};

class TokenServiceObserver : public OAuth2TokenService::Observer {
 public:
  void OnStartBatchChanges() override {
    EXPECT_FALSE(is_inside_batch_);
    is_inside_batch_ = true;

    // Start a new batch
    batch_change_records_.emplace_back(std::vector<std::string>());
  }

  void OnEndBatchChanges() override {
    EXPECT_TRUE(is_inside_batch_);
    is_inside_batch_ = false;
  }

  void OnRefreshTokenAvailable(const std::string& account_id) override {
    EXPECT_TRUE(is_inside_batch_);
    account_ids_.insert(account_id);

    // Record the |account_id| in the last batch.
    batch_change_records_.rbegin()->emplace_back(account_id);
  }

  void OnRefreshTokenRevoked(const std::string& account_id) override {
    EXPECT_TRUE(is_inside_batch_);
    account_ids_.erase(account_id);

    // Record the |account_id| in the last batch.
    batch_change_records_.rbegin()->emplace_back(account_id);
  }

  void OnAuthErrorChanged(const std::string& account_id,
                          const GoogleServiceAuthError& auth_error) override {
    last_err_account_id_ = account_id;
    last_err_ = auth_error;
  }

  std::string last_err_account_id_;
  GoogleServiceAuthError last_err_;
  std::set<std::string> account_ids_;
  bool is_inside_batch_ = false;

  // Records batch changes for later verification. Each index of this vector
  // represents a batch change. Each batch change is a vector of account ids for
  // which |OnRefreshTokenAvailable| is called.
  std::vector<std::vector<std::string>> batch_change_records_;
};

class SigninErrorObserver : public SigninErrorController::Observer {
 public:
  explicit SigninErrorObserver(SigninErrorController* signin_error_controller)
      : signin_error_controller_(signin_error_controller) {
    signin_error_controller_->AddObserver(this);
  }

  ~SigninErrorObserver() override {
    signin_error_controller_->RemoveObserver(this);
  }

  // |SigninErrorController::Observer| overrides.
  void OnErrorChanged() override {
    last_err_account_id_ = signin_error_controller_->error_account_id();
    last_err_ = signin_error_controller_->auth_error();
  }

  std::string last_err_account_id_;
  GoogleServiceAuthError last_err_ = GoogleServiceAuthError::AuthErrorNone();

 private:
  // Non-owning pointer to |SigninErrorController|.
  SigninErrorController* const signin_error_controller_;

  DISALLOW_COPY_AND_ASSIGN(SigninErrorObserver);
};

}  // namespace

class CrOSOAuthDelegateTest : public testing::Test {
 public:
  CrOSOAuthDelegateTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        signin_error_controller_(
            SigninErrorController::AccountMode::ANY_ACCOUNT),
        signin_error_observer_(&signin_error_controller_) {}
  ~CrOSOAuthDelegateTest() override = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());

    pref_service_.registry()->RegisterListPref(
        AccountTrackerService::kAccountInfoPref);
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kAccountIdMigrationState,
        AccountTrackerService::MIGRATION_NOT_STARTED);
    client_ = std::make_unique<TestSigninClient>(&pref_service_);
    account_manager_.Initialize(tmp_dir_.GetPath(),
                                client_->GetURLLoaderFactory(),
                                immediate_callback_runner_);
    scoped_task_environment_.RunUntilIdle();

    account_tracker_service_.Initialize(&pref_service_, base::FilePath());

    account_info_ = CreateAccountInfoTestFixture("111" /* gaia_id */,
                                                 "user@gmail.com" /* email */);
    account_tracker_service_.SeedAccountInfo(account_info_);

    delegate_ = std::make_unique<ChromeOSOAuth2TokenServiceDelegate>(
        &account_tracker_service_, &account_manager_,
        &signin_error_controller_);
    delegate_->LoadCredentials(
        account_info_.account_id /* primary_account_id */);
  }

  AccountInfo CreateAccountInfoTestFixture(const std::string& gaia_id,
                                           const std::string& email) {
    AccountInfo account_info;

    account_info.gaia = gaia_id;
    account_info.email = email;
    account_info.full_name = "name";
    account_info.given_name = "name";
    account_info.hosted_domain = "example.com";
    account_info.locale = "en";
    account_info.picture_url = "https://example.com";
    account_info.is_child_account = false;
    account_info.account_id = account_tracker_service_.PickAccountIdForAccount(
        account_info.gaia, account_info.email);

    // Cannot use |ASSERT_TRUE| due to a |void| return type in an |ASSERT_TRUE|
    // branch.
    EXPECT_TRUE(account_info.IsValid());

    return account_info;
  }

  void AddSuccessfulOAuthTokenResponse() {
    client_->test_url_loader_factory()->AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_url().spec(),
        GetValidTokenResponse("token", 3600));
  }

  // Check base/test/scoped_task_environment.h. This must be the first member /
  // declared before any member that cares about tasks.
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  // Needed because
  // |content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver| in
  // |ChromeOSOAuth2TokenServiceDelegate|'s constructor CHECKs that we are
  // running on the browser UI thread.
  content::TestBrowserThreadBundle thread_bundle_;

  base::ScopedTempDir tmp_dir_;
  AccountInfo account_info_;
  AccountTrackerService account_tracker_service_;
  AccountManager account_manager_;
  SigninErrorController signin_error_controller_;
  SigninErrorObserver signin_error_observer_;
  std::unique_ptr<ChromeOSOAuth2TokenServiceDelegate> delegate_;
  AccountManager::DelayNetworkCallRunner immediate_callback_runner_ =
      base::BindRepeating(
          [](const base::RepeatingClosure& closure) -> void { closure.Run(); });
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<TestSigninClient> client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrOSOAuthDelegateTest);
};

TEST_F(CrOSOAuthDelegateTest, RefreshTokenIsAvailableForGaiaAccounts) {
  EXPECT_EQ(OAuth2TokenServiceDelegate::LoadCredentialsState::
                LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
            delegate_->load_credentials_state());

  EXPECT_FALSE(delegate_->RefreshTokenIsAvailable(account_info_.account_id));

  const AccountManager::AccountKey account_key{account_info_.gaia,
                                               ACCOUNT_TYPE_GAIA};
  account_manager_.UpsertToken(account_key, "token");

  EXPECT_TRUE(delegate_->RefreshTokenIsAvailable(account_info_.account_id));
}

TEST_F(CrOSOAuthDelegateTest, ObserversAreNotifiedOnAuthErrorChange) {
  TokenServiceObserver observer;
  auto error =
      GoogleServiceAuthError(GoogleServiceAuthError::State::SERVICE_ERROR);
  delegate_->AddObserver(&observer);

  delegate_->UpdateAuthError(account_info_.account_id, error);
  EXPECT_EQ(error, delegate_->GetAuthError(account_info_.account_id));
  EXPECT_EQ(account_info_.account_id, observer.last_err_account_id_);
  EXPECT_EQ(error, observer.last_err_);

  delegate_->RemoveObserver(&observer);
}

TEST_F(CrOSOAuthDelegateTest, ObserversAreNotifiedOnCredentialsInsertion) {
  TokenServiceObserver observer;
  delegate_->AddObserver(&observer);
  delegate_->UpdateCredentials(account_info_.account_id, "123");

  EXPECT_EQ(1UL, observer.account_ids_.size());
  EXPECT_EQ(account_info_.account_id, *observer.account_ids_.begin());
  EXPECT_EQ(account_info_.account_id, observer.last_err_account_id_);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(), observer.last_err_);

  delegate_->RemoveObserver(&observer);
}

TEST_F(CrOSOAuthDelegateTest, ObserversAreNotifiedOnCredentialsUpdate) {
  TokenServiceObserver observer;
  delegate_->AddObserver(&observer);
  delegate_->UpdateCredentials(account_info_.account_id, "123");

  EXPECT_EQ(1UL, observer.account_ids_.size());
  EXPECT_EQ(account_info_.account_id, *observer.account_ids_.begin());
  EXPECT_EQ(account_info_.account_id, observer.last_err_account_id_);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(), observer.last_err_);

  delegate_->RemoveObserver(&observer);
}

TEST_F(CrOSOAuthDelegateTest,
       ObserversAreNotNotifiedIfCredentialsAreNotUpdated) {
  TokenServiceObserver observer;
  const std::string kToken = "123";
  delegate_->AddObserver(&observer);

  delegate_->UpdateCredentials(account_info_.account_id, kToken);
  observer.account_ids_.clear();
  observer.last_err_account_id_ = std::string();
  delegate_->UpdateCredentials(account_info_.account_id, kToken);

  EXPECT_TRUE(observer.account_ids_.empty());
  EXPECT_EQ(std::string(), observer.last_err_account_id_);

  delegate_->RemoveObserver(&observer);
}

TEST_F(CrOSOAuthDelegateTest,
       BatchChangeObserversAreNotifiedOnCredentialsUpdate) {
  TokenServiceObserver observer;
  delegate_->AddObserver(&observer);
  delegate_->UpdateCredentials(account_info_.account_id, "123");

  EXPECT_EQ(1UL, observer.batch_change_records_.size());
  EXPECT_EQ(1UL, observer.batch_change_records_[0].size());
  EXPECT_EQ(account_info_.account_id, observer.batch_change_records_[0][0]);

  delegate_->RemoveObserver(&observer);
}

// If observers register themselves with |OAuth2TokenServiceDelegate| before
// |AccountManager| has been initialized, they should receive all the accounts
// stored in |AccountManager| in a single batch.
TEST_F(CrOSOAuthDelegateTest, BatchChangeObserversAreNotifiedOncePerBatch) {
  // Setup
  AccountInfo account1 = CreateAccountInfoTestFixture(
      "1" /* gaia_id */, "test1@gmail.com" /* email */);
  AccountInfo account2 = CreateAccountInfoTestFixture(
      "2" /* gaia_id */, "test2@gmail.com" /* email */);

  account_tracker_service_.SeedAccountInfo(account1);
  account_tracker_service_.SeedAccountInfo(account2);
  account_manager_.UpsertToken(
      AccountManager::AccountKey{account1.gaia, ACCOUNT_TYPE_GAIA}, "token1");
  account_manager_.UpsertToken(
      AccountManager::AccountKey{account2.gaia, ACCOUNT_TYPE_GAIA}, "token2");
  scoped_task_environment_.RunUntilIdle();

  AccountManager account_manager;
  // AccountManager will not be fully initialized until
  // |scoped_task_environment_.RunUntilIdle()| is called.
  account_manager.Initialize(tmp_dir_.GetPath(), client_->GetURLLoaderFactory(),
                             immediate_callback_runner_);

  // Register callbacks before AccountManager has been fully initialized.
  auto delegate = std::make_unique<ChromeOSOAuth2TokenServiceDelegate>(
      &account_tracker_service_, &account_manager, &signin_error_controller_);
  delegate->LoadCredentials(account1.account_id /* primary_account_id */);
  TokenServiceObserver observer;
  delegate->AddObserver(&observer);
  // Wait until AccountManager is fully initialized.
  scoped_task_environment_.RunUntilIdle();

  // Tests

  // The observer should receive 3 batch change callbacks:
  // First - A batch of all accounts stored in AccountManager: because of the
  // delegate's invocation of |AccountManager::GetAccounts| in its constructor.
  // Followed by 2 updates for the individual accounts (|account1| and
  // |account2|): because of the delegate's registration as an
  // |AccountManager::Observer| before |AccountManager| has been fully
  // initialized.
  EXPECT_EQ(3UL, observer.batch_change_records_.size());

  const std::vector<std::string>& first_batch =
      observer.batch_change_records_[0];
  EXPECT_EQ(2UL, first_batch.size());
  EXPECT_TRUE(base::ContainsValue(first_batch, account1.account_id));
  EXPECT_TRUE(base::ContainsValue(first_batch, account2.account_id));

  delegate->RemoveObserver(&observer);
}

TEST_F(CrOSOAuthDelegateTest, GetAccountsShouldNotReturnAdAccounts) {
  EXPECT_TRUE(delegate_->GetAccounts().empty());

  // Insert an Active Directory account into AccountManager.
  AccountManager::AccountKey ad_account_key{"111",
                                            ACCOUNT_TYPE_ACTIVE_DIRECTORY};
  account_manager_.UpsertToken(ad_account_key, "" /* token */);

  // OAuth delegate should not return Active Directory accounts.
  EXPECT_TRUE(delegate_->GetAccounts().empty());
}

TEST_F(CrOSOAuthDelegateTest, GetAccountsReturnsGaiaAccounts) {
  EXPECT_TRUE(delegate_->GetAccounts().empty());

  AccountManager::AccountKey gaia_account_key{"111", ACCOUNT_TYPE_GAIA};
  account_manager_.UpsertToken(gaia_account_key, "token");

  std::vector<std::string> accounts = delegate_->GetAccounts();
  EXPECT_EQ(1UL, accounts.size());
  EXPECT_EQ(account_info_.account_id, accounts[0]);
}

TEST_F(CrOSOAuthDelegateTest, UpdateCredentialsSucceeds) {
  EXPECT_TRUE(delegate_->GetAccounts().empty());

  delegate_->UpdateCredentials(account_info_.account_id, "token");

  std::vector<std::string> accounts = delegate_->GetAccounts();
  EXPECT_EQ(1UL, accounts.size());
  EXPECT_EQ(account_info_.account_id, accounts[0]);
}

TEST_F(CrOSOAuthDelegateTest, ObserversAreNotifiedOnAccountRemoval) {
  delegate_->UpdateCredentials(account_info_.account_id, "token");

  TokenServiceObserver observer;
  delegate_->AddObserver(&observer);
  const AccountManager::AccountKey account_key{account_info_.gaia,
                                               ACCOUNT_TYPE_GAIA};
  account_manager_.RemoveAccount(account_key);

  EXPECT_EQ(1UL, observer.batch_change_records_.size());
  EXPECT_EQ(1UL, observer.batch_change_records_[0].size());
  EXPECT_EQ(account_info_.account_id, observer.batch_change_records_[0][0]);
  EXPECT_TRUE(observer.account_ids_.empty());

  delegate_->RemoveObserver(&observer);
}

TEST_F(CrOSOAuthDelegateTest,
       SigninErrorObserversAreNotifiedOnAuthErrorChange) {
  auto error =
      GoogleServiceAuthError(GoogleServiceAuthError::State::SERVICE_ERROR);

  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            signin_error_observer_.last_err_);
  delegate_->UpdateAuthError(account_info_.account_id, error);

  EXPECT_EQ(error, delegate_->GetAuthError(account_info_.account_id));
  EXPECT_EQ(account_info_.account_id,
            signin_error_observer_.last_err_account_id_);
  EXPECT_EQ(error, signin_error_observer_.last_err_);
}

TEST_F(CrOSOAuthDelegateTest, TransientErrorsAreNotShown) {
  auto transient_error = GoogleServiceAuthError(
      GoogleServiceAuthError::State::SERVICE_UNAVAILABLE);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            signin_error_observer_.last_err_);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            delegate_->GetAuthError(account_info_.account_id));

  delegate_->UpdateAuthError(account_info_.account_id, transient_error);

  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            signin_error_observer_.last_err_);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            delegate_->GetAuthError(account_info_.account_id));
}

TEST_F(CrOSOAuthDelegateTest, BackOffIsTriggerredForTransientErrors) {
  delegate_->UpdateCredentials(account_info_.account_id, "123");
  auto transient_error = GoogleServiceAuthError(
      GoogleServiceAuthError::State::SERVICE_UNAVAILABLE);
  delegate_->UpdateAuthError(account_info_.account_id, transient_error);
  // Add a dummy success response. The actual network call has not been made
  // yet.
  AddSuccessfulOAuthTokenResponse();

  // Transient error should repeat until backoff period expires.
  AccessTokenConsumer access_token_consumer;
  EXPECT_EQ(0, access_token_consumer.num_access_token_fetch_success_);
  EXPECT_EQ(0, access_token_consumer.num_access_token_fetch_failure_);
  std::vector<std::string> scopes{"scope"};
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher(
      delegate_->CreateAccessTokenFetcher(account_info_.account_id,
                                          delegate_->GetURLLoaderFactory(),
                                          &access_token_consumer));
  scoped_task_environment_.RunUntilIdle();
  fetcher->Start("client_id", "client_secret", scopes);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(0, access_token_consumer.num_access_token_fetch_success_);
  EXPECT_EQ(1, access_token_consumer.num_access_token_fetch_failure_);
  // Expect a positive backoff time.
  EXPECT_GT(delegate_->backoff_entry_.GetTimeUntilRelease(), base::TimeDelta());

  // Pretend that backoff has expired and try again.
  delegate_->backoff_entry_.SetCustomReleaseTime(base::TimeTicks());
  fetcher.reset(delegate_->CreateAccessTokenFetcher(
      account_info_.account_id, delegate_->GetURLLoaderFactory(),
      &access_token_consumer));
  fetcher->Start("client_id", "client_secret", scopes);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1, access_token_consumer.num_access_token_fetch_success_);
  EXPECT_EQ(1, access_token_consumer.num_access_token_fetch_failure_);
}

TEST_F(CrOSOAuthDelegateTest, BackOffIsResetOnNetworkChange) {
  delegate_->UpdateCredentials(account_info_.account_id, "123");
  auto transient_error = GoogleServiceAuthError(
      GoogleServiceAuthError::State::SERVICE_UNAVAILABLE);
  delegate_->UpdateAuthError(account_info_.account_id, transient_error);
  // Add a dummy success response. The actual network call has not been made
  // yet.
  AddSuccessfulOAuthTokenResponse();

  // Transient error should repeat until backoff period expires.
  AccessTokenConsumer access_token_consumer;
  EXPECT_EQ(0, access_token_consumer.num_access_token_fetch_success_);
  EXPECT_EQ(0, access_token_consumer.num_access_token_fetch_failure_);
  std::vector<std::string> scopes{"scope"};
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher(
      delegate_->CreateAccessTokenFetcher(account_info_.account_id,
                                          delegate_->GetURLLoaderFactory(),
                                          &access_token_consumer));
  scoped_task_environment_.RunUntilIdle();
  fetcher->Start("client_id", "client_secret", scopes);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(0, access_token_consumer.num_access_token_fetch_success_);
  EXPECT_EQ(1, access_token_consumer.num_access_token_fetch_failure_);
  // Expect a positive backoff time.
  EXPECT_GT(delegate_->backoff_entry_.GetTimeUntilRelease(), base::TimeDelta());

  // Notify of network change and ensure that request now runs.
  delegate_->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  fetcher.reset(delegate_->CreateAccessTokenFetcher(
      account_info_.account_id, delegate_->GetURLLoaderFactory(),
      &access_token_consumer));
  fetcher->Start("client_id", "client_secret", scopes);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1, access_token_consumer.num_access_token_fetch_success_);
  EXPECT_EQ(1, access_token_consumer.num_access_token_fetch_failure_);
}

}  // namespace chromeos
