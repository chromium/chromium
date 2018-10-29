// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/mutable_profile_oauth2_token_service_delegate.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/browser/device_id_helper.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/browser/webdata/token_web_data.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_token_service_test_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Defining constant here to handle backward compatiblity tests, but this
// constant is no longer used in current versions of chrome.
static const char kLSOService[] = "lso";
static const char kEmail[] = "user@gmail.com";

namespace {

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Create test account info.
AccountInfo CreateTestAccountInfo(const std::string& name,
                                  bool is_hosted_domain,
                                  bool is_valid) {
  AccountInfo account_info;
  account_info.account_id = name;
  account_info.gaia = name;
  account_info.email = name + "@email.com";
  account_info.full_name = "name";
  account_info.given_name = "name";
  if (is_valid) {
    account_info.hosted_domain =
        is_hosted_domain ? "example.com"
                         : AccountTrackerService::kNoHostedDomainFound;
  }
  account_info.locale = "en";
  account_info.picture_url = "https://example.com";
  account_info.is_child_account = false;
  EXPECT_EQ(is_valid, account_info.IsValid());
  return account_info;
}
#endif

}  // namespace

class MutableProfileOAuth2TokenServiceDelegateTest
    : public testing::Test,
      public OAuth2AccessTokenConsumer,
      public OAuth2TokenService::Observer,
      public WebDataServiceConsumer {
 public:
  MutableProfileOAuth2TokenServiceDelegateTest()
      : signin_error_controller_(
            SigninErrorController::AccountMode::ANY_ACCOUNT),
        access_token_success_count_(0),
        access_token_failure_count_(0),
        access_token_failure_(GoogleServiceAuthError::NONE),
        token_available_count_(0),
        token_revoked_count_(0),
        tokens_loaded_count_(0),
        start_batch_changes_(0),
        end_batch_changes_(0),
        auth_error_changed_count_(0),
        revoke_all_tokens_on_load_(false) {}

  void SetUp() override {
    OSCryptMocker::SetUp();

    MutableProfileOAuth2TokenServiceDelegate::RegisterProfilePrefs(
        pref_service_.registry());
    pref_service_.registry()->RegisterListPref(
        AccountTrackerService::kAccountInfoPref);
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kAccountIdMigrationState,
        AccountTrackerService::MIGRATION_NOT_STARTED);
    SigninManagerBase::RegisterProfilePrefs(pref_service_.registry());
    client_.reset(new TestSigninClient(&pref_service_));
    client_->test_url_loader_factory()->AddResponse(
        GaiaUrls::GetInstance()->oauth2_revoke_url().spec(), "");
    LoadTokenDatabase();
    account_tracker_service_.Initialize(&pref_service_, base::FilePath());
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    oauth2_service_delegate_->RemoveObserver(this);
    oauth2_service_delegate_->Shutdown();
    OSCryptMocker::TearDown();
  }

  void LoadTokenDatabase() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath path = temp_dir_.GetPath().AppendASCII("TestWebDB");
    scoped_refptr<WebDatabaseService> web_database =
        new WebDatabaseService(path, base::ThreadTaskRunnerHandle::Get(),
                               base::ThreadTaskRunnerHandle::Get());
    web_database->AddTable(std::make_unique<TokenServiceTable>());
    web_database->LoadDatabase();
    token_web_data_ =
        new TokenWebData(web_database, base::ThreadTaskRunnerHandle::Get(),
                         base::ThreadTaskRunnerHandle::Get(),
                         WebDataServiceBase::ProfileErrorCallback());
    token_web_data_->Init();
  }

  void AddSuccessfulOAuhTokenResponse() {
    client_->test_url_loader_factory()->AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_url().spec(),
        GetValidTokenResponse("token", 3600));
  }

  void CreateOAuth2ServiceDelegate(
      signin::AccountConsistencyMethod account_consistency) {
    oauth2_service_delegate_.reset(new MutableProfileOAuth2TokenServiceDelegate(
        client_.get(), &signin_error_controller_, &account_tracker_service_,
        token_web_data_, account_consistency, revoke_all_tokens_on_load_,
        true /* can_revoke_credantials */));
    // Make sure PO2TS has a chance to load itself before continuing.
    base::RunLoop().RunUntilIdle();
    oauth2_service_delegate_->AddObserver(this);
  }

  void AddAuthTokenManually(const std::string& service,
                            const std::string& value) {
    if (token_web_data_)
      token_web_data_->SetTokenForService(service, value);
  }

  // WebDataServiceConsumer implementation
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override {
    DCHECK(!token_web_data_result_);
    DCHECK_EQ(TOKEN_RESULT, result->GetType());
    token_web_data_result_.reset(
        static_cast<WDResult<TokenResult>*>(result.release()));
  }

  // OAuth2AccessTokenConusmer implementation
  void OnGetTokenSuccess(
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override {
    ++access_token_success_count_;
  }

  void OnGetTokenFailure(const GoogleServiceAuthError& error) override {
    ++access_token_failure_count_;
    access_token_failure_ = error;
  }

  // OAuth2TokenService::Observer implementation.
  void OnRefreshTokenAvailable(const std::string& account_id) override {
    ++token_available_count_;
  }
  void OnRefreshTokenRevoked(const std::string& account_id) override {
    ++token_revoked_count_;
  }
  void OnRefreshTokensLoaded() override { ++tokens_loaded_count_; }

  void OnStartBatchChanges() override { ++start_batch_changes_; }

  void OnEndBatchChanges() override { ++end_batch_changes_; }

  void OnAuthErrorChanged(const std::string& account_id,
                          const GoogleServiceAuthError& auth_error) override {
    ++auth_error_changed_count_;
  }

  void ResetObserverCounts() {
    token_available_count_ = 0;
    token_revoked_count_ = 0;
    tokens_loaded_count_ = 0;
    start_batch_changes_ = 0;
    end_batch_changes_ = 0;
    auth_error_changed_count_ = 0;
  }

  void ExpectNoNotifications() {
    EXPECT_EQ(0, token_available_count_);
    EXPECT_EQ(0, token_revoked_count_);
    EXPECT_EQ(0, tokens_loaded_count_);
    ResetObserverCounts();
  }

  void ExpectOneTokenAvailableNotification() {
    EXPECT_EQ(1, token_available_count_);
    EXPECT_EQ(0, token_revoked_count_);
    EXPECT_EQ(0, tokens_loaded_count_);
    ResetObserverCounts();
  }

  void ExpectOneTokenRevokedNotification() {
    EXPECT_EQ(0, token_available_count_);
    EXPECT_EQ(1, token_revoked_count_);
    EXPECT_EQ(0, tokens_loaded_count_);
    ResetObserverCounts();
  }

  void ExpectOneTokensLoadedNotification() {
    EXPECT_EQ(0, token_available_count_);
    EXPECT_EQ(0, token_revoked_count_);
    EXPECT_EQ(1, tokens_loaded_count_);
    ResetObserverCounts();
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestSigninClient> client_;
  std::unique_ptr<MutableProfileOAuth2TokenServiceDelegate>
      oauth2_service_delegate_;
  TestingOAuth2TokenServiceConsumer consumer_;
  SigninErrorController signin_error_controller_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  AccountTrackerService account_tracker_service_;
  scoped_refptr<TokenWebData> token_web_data_;
  std::unique_ptr<WDResult<TokenResult>> token_web_data_result_;
  int access_token_success_count_;
  int access_token_failure_count_;
  GoogleServiceAuthError access_token_failure_;
  int token_available_count_;
  int token_revoked_count_;
  int tokens_loaded_count_;
  int start_batch_changes_;
  int end_batch_changes_;
  int auth_error_changed_count_;
  bool revoke_all_tokens_on_load_;
};

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, PersistenceDBUpgrade) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kMirror);
  std::string main_account_id("account_id");
  std::string main_refresh_token("old_refresh_token");

  // Populate DB with legacy tokens.
  AddAuthTokenManually(GaiaConstants::kSyncService, "syncServiceToken");
  AddAuthTokenManually(kLSOService, "lsoToken");
  AddAuthTokenManually(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                       main_refresh_token);

  // Force LoadCredentials.
  oauth2_service_delegate_->LoadCredentials(main_account_id);
  base::RunLoop().RunUntilIdle();

  // Legacy tokens get discarded, but the old refresh token is kept.
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(main_account_id));
  EXPECT_EQ(1U, oauth2_service_delegate_->refresh_tokens_.size());
  EXPECT_EQ(main_refresh_token,
            oauth2_service_delegate_->refresh_tokens_[main_account_id]
                ->refresh_token());

  // Add an old legacy token to the DB, to ensure it will not overwrite existing
  // credentials for main account.
  AddAuthTokenManually(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                       "secondOldRefreshToken");
  // Add some other legacy token. (Expected to get discarded).
  AddAuthTokenManually(kLSOService, "lsoToken");
  // Also add a token using PO2TS.UpdateCredentials and make sure upgrade does
  // not wipe it.
  std::string other_account_id("other_account_id");
  std::string other_refresh_token("other_refresh_token");
  oauth2_service_delegate_->UpdateCredentials(other_account_id,
                                              other_refresh_token);
  ResetObserverCounts();

  // Force LoadCredentials.
  oauth2_service_delegate_->LoadCredentials(main_account_id);
  base::RunLoop().RunUntilIdle();

  // Again legacy tokens get discarded, but since the main porfile account
  // token is present it is not overwritten.
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(main_refresh_token,
            oauth2_service_delegate_->GetRefreshToken(main_account_id));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(main_account_id));
  // TODO(fgorski): cover both using RefreshTokenIsAvailable() and then get the
  // tokens using GetRefreshToken()
  EXPECT_EQ(2U, oauth2_service_delegate_->refresh_tokens_.size());
  EXPECT_EQ(main_refresh_token,
            oauth2_service_delegate_->refresh_tokens_[main_account_id]
                ->refresh_token());
  EXPECT_EQ(other_refresh_token,
            oauth2_service_delegate_->refresh_tokens_[other_account_id]
                ->refresh_token());

  oauth2_service_delegate_->RevokeAllCredentials();
  EXPECT_EQ(2, start_batch_changes_);
  EXPECT_EQ(2, end_batch_changes_);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       PersistenceRevokeCredentials) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  std::string account_id_1 = "account_id_1";
  std::string refresh_token_1 = "refresh_token_1";
  std::string account_id_2 = "account_id_2";
  std::string refresh_token_2 = "refresh_token_2";

  // TODO(fgorski): Enable below when implemented:
  // EXPECT_FALSE(oauth2_servive_->RefreshTokenIsAvailable(account_id_1));
  // EXPECT_FALSE(oauth2_servive_->RefreshTokenIsAvailable(account_id_2));
  oauth2_service_delegate_->UpdateCredentials(account_id_1, refresh_token_1);
  oauth2_service_delegate_->UpdateCredentials(account_id_2, refresh_token_2);
  EXPECT_EQ(2, start_batch_changes_);
  EXPECT_EQ(2, end_batch_changes_);

  // TODO(fgorski): Enable below when implemented:
  // EXPECT_TRUE(oauth2_servive_->RefreshTokenIsAvailable(account_id_1));
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(account_id_2));

  ResetObserverCounts();
  oauth2_service_delegate_->RevokeCredentials(account_id_1);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  ExpectOneTokenRevokedNotification();

  // TODO(fgorski): Enable below when implemented:
  // EXPECT_FALSE(oauth2_servive_->RefreshTokenIsAvailable(account_id_1));
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(account_id_2));

  oauth2_service_delegate_->RevokeAllCredentials();
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  ResetObserverCounts();
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       LoadCredentialsStateEmptyPrimaryAccountId) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  // Ensure DB is clean.
  oauth2_service_delegate_->RevokeAllCredentials();

  EXPECT_EQ(OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_NOT_STARTED,
            oauth2_service_delegate_->load_credentials_state());
  oauth2_service_delegate_->LoadCredentials("");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
            oauth2_service_delegate_->load_credentials_state());
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       PersistenceLoadCredentials) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kMirror);

  // Ensure DB is clean.
  oauth2_service_delegate_->RevokeAllCredentials();
  ResetObserverCounts();

  // Perform a load from an empty DB.
  EXPECT_EQ(OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_NOT_STARTED,
            oauth2_service_delegate_->load_credentials_state());
  oauth2_service_delegate_->LoadCredentials("account_id");
  EXPECT_EQ(OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_IN_PROGRESS,
            oauth2_service_delegate_->load_credentials_state());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(OAuth2TokenServiceDelegate::
                LOAD_CREDENTIALS_FINISHED_WITH_NO_TOKEN_FOR_PRIMARY_ACCOUNT,
            oauth2_service_delegate_->load_credentials_state());
  EXPECT_EQ(GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                    CREDENTIALS_MISSING),
            signin_error_controller_.auth_error());
  EXPECT_EQ(GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                    CREDENTIALS_MISSING),
            oauth2_service_delegate_->GetAuthError("account_id"));
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(1, auth_error_changed_count_);

  // A"tokens loaded" notification should have been fired.
  EXPECT_EQ(1, tokens_loaded_count_);

  // As the delegate puts the primary account into the token map with an invalid
  // token in the case of loading from an empty TB, a "token available"
  // notification should have been fired as well.
  EXPECT_EQ(1, token_available_count_);

  ResetObserverCounts();

  // LoadCredentials() guarantees that the account given to it as argument
  // is in the refresh_token map.
  EXPECT_EQ(1U, oauth2_service_delegate_->refresh_tokens_.size());
  EXPECT_EQ(
      MutableProfileOAuth2TokenServiceDelegate::kInvalidRefreshToken,
      oauth2_service_delegate_->refresh_tokens_["account_id"]->refresh_token());
  // Setup a DB with tokens that don't require upgrade and clear memory.
  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token");
  oauth2_service_delegate_->UpdateCredentials("account_id2", "refresh_token2");
  oauth2_service_delegate_->refresh_tokens_.clear();
  EXPECT_EQ(2, start_batch_changes_);
  EXPECT_EQ(2, end_batch_changes_);
  EXPECT_EQ(2, auth_error_changed_count_);
  ResetObserverCounts();

  oauth2_service_delegate_->LoadCredentials("account_id");
  EXPECT_EQ(OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_IN_PROGRESS,
            oauth2_service_delegate_->load_credentials_state());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
            oauth2_service_delegate_->load_credentials_state());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            signin_error_controller_.auth_error());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError("account_id"));
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(2, auth_error_changed_count_);
  ResetObserverCounts();

  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable("account_id"));
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable("account_id2"));

  oauth2_service_delegate_->RevokeAllCredentials();
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(2, token_revoked_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(0, auth_error_changed_count_);
  ResetObserverCounts();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       PersistenceLoadCredentialsEmptyPrimaryAccountId_DiceEnabled) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);

  // Ensure DB is clean.
  oauth2_service_delegate_->RevokeAllCredentials();
  ResetObserverCounts();
  // Perform a load from an empty DB.
  EXPECT_EQ(OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_NOT_STARTED,
            oauth2_service_delegate_->load_credentials_state());
  oauth2_service_delegate_->LoadCredentials("");
  EXPECT_EQ(OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_IN_PROGRESS,
            oauth2_service_delegate_->load_credentials_state());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
            oauth2_service_delegate_->load_credentials_state());
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(0, auth_error_changed_count_);
  ExpectOneTokensLoadedNotification();

  // No account should be present in the refresh token as no primary account
  // was passed to the token service.
  EXPECT_TRUE(oauth2_service_delegate_->refresh_tokens_.empty());

  // Setup a DB with tokens that don't require upgrade and clear memory.
  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token");
  oauth2_service_delegate_->UpdateCredentials("account_id2", "refresh_token2");
  oauth2_service_delegate_->refresh_tokens_.clear();
  EXPECT_EQ(2, start_batch_changes_);
  EXPECT_EQ(2, end_batch_changes_);
  EXPECT_EQ(2, auth_error_changed_count_);
  ResetObserverCounts();

  oauth2_service_delegate_->LoadCredentials("");
  EXPECT_EQ(OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_IN_PROGRESS,
            oauth2_service_delegate_->load_credentials_state());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
            oauth2_service_delegate_->load_credentials_state());
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(2, auth_error_changed_count_);
  ResetObserverCounts();

  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable("account_id"));
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable("account_id2"));

  oauth2_service_delegate_->RevokeAllCredentials();
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(2, token_revoked_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(0, auth_error_changed_count_);
  ResetObserverCounts();
}

// Tests that Dice migration does not happen if an account is invalid. In
// particular, no hosted domain tokens are revoked.
TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       DiceNoMigrationOnInvalidAccount) {
  ASSERT_FALSE(pref_service_.GetBoolean(prefs::kTokenServiceDiceCompatible));
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDiceMigration);
  oauth2_service_delegate_->RevokeAllCredentials();

  // Add account info to the account tracker.
  AccountInfo primary_account = CreateTestAccountInfo(
      "primary_account", true /* is_hosted_domain*/, true /* is_valid*/);
  AccountInfo secondary_account = CreateTestAccountInfo(
      "secondary_account", false /* is_hosted_domain*/, false /* is_valid*/);
  account_tracker_service_.SeedAccountInfo(primary_account);
  account_tracker_service_.SeedAccountInfo(secondary_account);

  ResetObserverCounts();
  AddAuthTokenManually("AccountId-" + primary_account.account_id,
                       "refresh_token");
  AddAuthTokenManually("AccountId-" + secondary_account.account_id,
                       "refresh_token");
  oauth2_service_delegate_->LoadCredentials(primary_account.account_id);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(2, auth_error_changed_count_);
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(
      primary_account.account_id));
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(
      secondary_account.account_id));
  EXPECT_EQ(OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
            oauth2_service_delegate_->load_credentials_state());

  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kTokenServiceDiceCompatible));
}

// Tests that the migration happened after loading consummer accounts.
TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       DiceMigrationConsummerAccounts) {
  ASSERT_EQ(AccountTrackerService::MIGRATION_DONE,
            account_tracker_service_.GetMigrationState());
  ASSERT_FALSE(pref_service_.GetBoolean(prefs::kTokenServiceDiceCompatible));
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDiceMigration);
  oauth2_service_delegate_->RevokeAllCredentials();

  // Add account info to the account tracker.
  AccountInfo primary_account = CreateTestAccountInfo(
      "primary_account", false /* is_hosted_domain*/, true /* is_valid*/);
  AccountInfo secondary_account = CreateTestAccountInfo(
      "secondary_account", false /* is_hosted_domain*/, true /* is_valid*/);
  account_tracker_service_.SeedAccountInfo(primary_account);
  account_tracker_service_.SeedAccountInfo(secondary_account);

  ResetObserverCounts();
  AddAuthTokenManually("AccountId-" + primary_account.account_id,
                       "refresh_token");
  AddAuthTokenManually("AccountId-" + secondary_account.account_id,
                       "refresh_token");
  oauth2_service_delegate_->LoadCredentials(primary_account.account_id);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(2, auth_error_changed_count_);
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(
      primary_account.account_id));
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(
      secondary_account.account_id));
  EXPECT_EQ(OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
            oauth2_service_delegate_->load_credentials_state());

  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kTokenServiceDiceCompatible));
}

// Tests that the migration revokes the hosted domain tokens.
TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       DiceMigrationHostedDomainAccounts) {
  ASSERT_EQ(AccountTrackerService::MIGRATION_DONE,
            account_tracker_service_.GetMigrationState());
  ASSERT_FALSE(pref_service_.GetBoolean(prefs::kTokenServiceDiceCompatible));
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDiceMigration);
  oauth2_service_delegate_->RevokeAllCredentials();

  // Add account info to the account tracker.
  AccountInfo primary_account = CreateTestAccountInfo(
      "primary_account", false /* is_hosted_domain*/, true /* is_valid*/);
  AccountInfo secondary_account = CreateTestAccountInfo(
      "secondary_account", true /* is_hosted_domain*/, true /* is_valid*/);
  account_tracker_service_.SeedAccountInfo(primary_account);
  account_tracker_service_.SeedAccountInfo(secondary_account);

  ResetObserverCounts();
  AddAuthTokenManually("AccountId-" + primary_account.account_id,
                       "refresh_token");
  AddAuthTokenManually("AccountId-" + secondary_account.account_id,
                       "refresh_token");
  oauth2_service_delegate_->LoadCredentials(primary_account.account_id);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(1, auth_error_changed_count_);
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(
      primary_account.account_id));
  EXPECT_EQ(OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
            oauth2_service_delegate_->load_credentials_state());

  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kTokenServiceDiceCompatible));
}

// Tests that the migration can revoke the primary token too.
TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       DiceMigrationHostedDomainPrimaryAccount) {
  ASSERT_EQ(AccountTrackerService::MIGRATION_DONE,
            account_tracker_service_.GetMigrationState());
  ASSERT_FALSE(pref_service_.GetBoolean(prefs::kTokenServiceDiceCompatible));
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDiceMigration);
  oauth2_service_delegate_->RevokeAllCredentials();

  // Add account info to the account tracker.
  AccountInfo primary_account = CreateTestAccountInfo(
      "primary_account", true /* is_hosted_domain*/, true /* is_valid*/);
  account_tracker_service_.SeedAccountInfo(primary_account);

  ResetObserverCounts();
  AddAuthTokenManually("AccountId-" + primary_account.account_id,
                       "refresh_token");
  oauth2_service_delegate_->LoadCredentials(primary_account.account_id);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(1, auth_error_changed_count_);

  // After having revoked the primary account's token during loading, the
  // delegate should have noticed that it had no token for the primary account
  // when the load was complete and inserted an invalid token for that account.
  EXPECT_EQ(1, token_available_count_);
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(
      primary_account.account_id));
  EXPECT_EQ(
      MutableProfileOAuth2TokenServiceDelegate::kInvalidRefreshToken,
      oauth2_service_delegate_->refresh_tokens_[primary_account.account_id]
          ->refresh_token());
  EXPECT_EQ(
      GoogleServiceAuthError::InvalidGaiaCredentialsReason::CREDENTIALS_MISSING,
      oauth2_service_delegate_->GetAuthError(primary_account.account_id)
          .GetInvalidGaiaCredentialsReason());
  EXPECT_EQ(OAuth2TokenServiceDelegate::
                LOAD_CREDENTIALS_FINISHED_WITH_NO_TOKEN_FOR_PRIMARY_ACCOUNT,
            oauth2_service_delegate_->load_credentials_state());

  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kTokenServiceDiceCompatible));
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if !defined(OS_CHROMEOS)
TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       LoadCredentialsClearsTokenDBWhenNoPrimaryAccount_DiceDisabled) {
  // Populate DB with 2 valid tokens.
  AddAuthTokenManually("AccountId-12345", "refresh_token");
  AddAuthTokenManually("AccountId-67890", "refresh_token");

  CreateOAuth2ServiceDelegate(
      signin::AccountConsistencyMethod::kDiceFixAuthErrors);
  oauth2_service_delegate_->LoadCredentials(/*primary_account_id=*/"");
  base::RunLoop().RunUntilIdle();

  // No tokens were loaded.
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(2, token_revoked_count_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(0U, oauth2_service_delegate_->refresh_tokens_.size());

  // Handle to the request reading tokens from database.
  token_web_data_->GetAllTokens(this);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(token_web_data_result_.get());
  ASSERT_EQ(0u, token_web_data_result_->GetValue().tokens.size());
}
#endif  // !defined(OS_CHROMEOS)

// Tests that calling UpdateCredentials revokes the old token, without sending
// the notification.
TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, RevokeOnUpdate) {
  // Add a token.
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  ASSERT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token");
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  ExpectOneTokenAvailableNotification();

  // Updating the token does not revoke the old one.
  // Regression test for http://crbug.com/865189
  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token2");
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  ExpectOneTokenAvailableNotification();

  // Flush the server revokes.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());

  // Set the same token again.
  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token2");
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  ExpectNoNotifications();

  // Clear the token.
  oauth2_service_delegate_->RevokeAllCredentials();
  EXPECT_EQ(1u, oauth2_service_delegate_->server_revokes_.size());
  ExpectOneTokenRevokedNotification();

  // Flush the server revokes.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, DelayedRevoke) {
  client_->SetNetworkCallsDelayed(true);
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token");
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  oauth2_service_delegate_->RevokeCredentials("account_id");

  // The revoke does not start until network calls are unblocked.
  EXPECT_EQ(1u, oauth2_service_delegate_->server_revokes_.size());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, oauth2_service_delegate_->server_revokes_.size());

  // Unblock network calls, and check that the revocation goes through.
  client_->SetNetworkCallsDelayed(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, ShutdownDuringRevoke) {
  // Shutdown cancels the revocation.
  client_->SetNetworkCallsDelayed(true);
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token");
  oauth2_service_delegate_->RevokeCredentials("account_id");
  EXPECT_EQ(1u, oauth2_service_delegate_->server_revokes_.size());

  // Shutdown.
  oauth2_service_delegate_->Shutdown();
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());

  // Unblocking network calls after shutdown does not crash.
  client_->SetNetworkCallsDelayed(false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, UpdateInvalidToken) {
  // Add the invalid token.
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  ASSERT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  oauth2_service_delegate_->UpdateCredentials(
      "account_id",
      MutableProfileOAuth2TokenServiceDelegate::kInvalidRefreshToken);
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  EXPECT_EQ(1, auth_error_changed_count_);
  ExpectOneTokenAvailableNotification();

  // The account is in authentication error.
  EXPECT_EQ(GoogleServiceAuthError(
                GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                    GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                        CREDENTIALS_REJECTED_BY_CLIENT)),
            oauth2_service_delegate_->GetAuthError("account_id"));
  EXPECT_EQ(GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                    CREDENTIALS_REJECTED_BY_CLIENT),
            signin_error_controller_.auth_error());

  // Update the token: authentication error is fixed, no actual server
  // revocation.
  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token");
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  EXPECT_EQ(1, auth_error_changed_count_);
  ExpectOneTokenAvailableNotification();
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            signin_error_controller_.auth_error());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError("account_id"));
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       InvalidateTokensForMultilogin) {
  class TokenServiceErrorObserver : public OAuth2TokenService::Observer {
   public:
    MOCK_METHOD2(OnAuthErrorChanged,
                 void(const std::string&, const GoogleServiceAuthError&));
  };

  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  TokenServiceErrorObserver observer;
  oauth2_service_delegate_->AddObserver(&observer);

  const std::string account_id1 = "account_id1";
  const std::string account_id2 = "account_id2";

  // This will be fired from UpdateCredentials.
  EXPECT_CALL(
      observer,
      OnAuthErrorChanged(::testing::_, GoogleServiceAuthError::AuthErrorNone()))
      .Times(2);
  oauth2_service_delegate_->UpdateCredentials(account_id1, "refresh_token1");
  oauth2_service_delegate_->UpdateCredentials(account_id2, "refresh_token2");

  testing::Mock::VerifyAndClearExpectations(&observer);

  // This should be fired after error is set.
  EXPECT_CALL(
      observer,
      OnAuthErrorChanged(account_id1,
                         GoogleServiceAuthError(
                             GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS)))
      .Times(1);

  oauth2_service_delegate_->InvalidateTokenForMultilogin(account_id1);
  EXPECT_EQ(oauth2_service_delegate_->GetAuthError(account_id1).state(),
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_EQ(oauth2_service_delegate_->GetAuthError(account_id2).state(),
            GoogleServiceAuthError::NONE);

  oauth2_service_delegate_->RemoveObserver(&observer);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, LoadInvalidToken) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  std::map<std::string, std::string> tokens;
  tokens["AccountId-account_id"] =
      MutableProfileOAuth2TokenServiceDelegate::kInvalidRefreshToken;

  oauth2_service_delegate_->LoadAllCredentialsIntoMemory(tokens);

  EXPECT_EQ(1u, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable("account_id"));
  EXPECT_STREQ(MutableProfileOAuth2TokenServiceDelegate::kInvalidRefreshToken,
               oauth2_service_delegate_->GetRefreshToken("account_id").c_str());

  // The account is in authentication error.
  EXPECT_EQ(GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                    CREDENTIALS_REJECTED_BY_CLIENT),
            signin_error_controller_.auth_error());
  EXPECT_EQ(GoogleServiceAuthError(
                GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                    GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                        CREDENTIALS_REJECTED_BY_CLIENT)),
            oauth2_service_delegate_->GetAuthError("account_id"));
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, GetTokenForMultilogin) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  const std::string account_id1 = "account_id1";
  const std::string account_id2 = "account_id2";

  oauth2_service_delegate_->UpdateCredentials(account_id1, "refresh_token1");
  oauth2_service_delegate_->UpdateCredentials(account_id2, "refresh_token2");
  oauth2_service_delegate_->UpdateAuthError(
      account_id2,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  EXPECT_EQ(oauth2_service_delegate_->GetTokenForMultilogin(account_id1),
            "refresh_token1");
  EXPECT_EQ(oauth2_service_delegate_->GetTokenForMultilogin(account_id2),
            std::string());
  EXPECT_EQ(oauth2_service_delegate_->GetTokenForMultilogin("unknown account"),
            std::string());
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, PersistenceNotifications) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token");
  ExpectOneTokenAvailableNotification();

  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token");
  ExpectNoNotifications();

  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token2");
  ExpectOneTokenAvailableNotification();

  oauth2_service_delegate_->RevokeCredentials("account_id");
  ExpectOneTokenRevokedNotification();

  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token2");
  ExpectOneTokenAvailableNotification();

  oauth2_service_delegate_->RevokeAllCredentials();
  ResetObserverCounts();
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, GetAccounts) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  EXPECT_TRUE(oauth2_service_delegate_->GetAccounts().empty());
  oauth2_service_delegate_->UpdateCredentials("account_id1", "refresh_token1");
  oauth2_service_delegate_->UpdateCredentials("account_id2", "refresh_token2");
  std::vector<std::string> accounts = oauth2_service_delegate_->GetAccounts();
  EXPECT_EQ(2u, accounts.size());
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), "account_id1"));
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), "account_id2"));
  oauth2_service_delegate_->RevokeCredentials("account_id2");
  accounts = oauth2_service_delegate_->GetAccounts();
  EXPECT_EQ(1u, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), "account_id1"));
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, FetchPersistentError) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  oauth2_service_delegate_->UpdateCredentials(kEmail, "refreshToken");
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            signin_error_controller_.auth_error());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError(kEmail));

  GoogleServiceAuthError authfail(GoogleServiceAuthError::ACCOUNT_DELETED);
  oauth2_service_delegate_->UpdateAuthError(kEmail, authfail);
  EXPECT_NE(GoogleServiceAuthError::AuthErrorNone(),
            signin_error_controller_.auth_error());
  EXPECT_NE(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError(kEmail));

  // Create a "success" fetch we don't expect to get called.
  AddSuccessfulOAuhTokenResponse();

  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(0, access_token_failure_count_);
  std::vector<std::string> scope_list;
  scope_list.push_back("scope");
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher(
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          kEmail, oauth2_service_delegate_->GetURLLoaderFactory(), this));
  fetcher->Start("foo", "bar", scope_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(1, access_token_failure_count_);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, RetryBackoff) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  oauth2_service_delegate_->UpdateCredentials(kEmail, "refreshToken");
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            signin_error_controller_.auth_error());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError(kEmail));

  GoogleServiceAuthError authfail(GoogleServiceAuthError::SERVICE_UNAVAILABLE);
  oauth2_service_delegate_->UpdateAuthError(kEmail, authfail);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            signin_error_controller_.auth_error());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError(kEmail));

  // Create a "success" fetch we don't expect to get called just yet.
  AddSuccessfulOAuhTokenResponse();

  // Transient error will repeat until backoff period expires.
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(0, access_token_failure_count_);
  std::vector<std::string> scope_list;
  scope_list.push_back("scope");
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher1(
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          kEmail, oauth2_service_delegate_->GetURLLoaderFactory(), this));
  fetcher1->Start("foo", "bar", scope_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(1, access_token_failure_count_);
  // Expect a positive backoff time.
  EXPECT_GT(oauth2_service_delegate_->backoff_entry_.GetTimeUntilRelease(),
            base::TimeDelta());

  // Pretend that backoff has expired and try again.
  oauth2_service_delegate_->backoff_entry_.SetCustomReleaseTime(
      base::TimeTicks());
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher2(
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          kEmail, oauth2_service_delegate_->GetURLLoaderFactory(), this));
  fetcher2->Start("foo", "bar", scope_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, access_token_success_count_);
  EXPECT_EQ(1, access_token_failure_count_);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, ResetBackoff) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  oauth2_service_delegate_->UpdateCredentials(kEmail, "refreshToken");
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            signin_error_controller_.auth_error());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError(kEmail));

  GoogleServiceAuthError authfail(GoogleServiceAuthError::SERVICE_UNAVAILABLE);
  oauth2_service_delegate_->UpdateAuthError(kEmail, authfail);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            signin_error_controller_.auth_error());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError(kEmail));

  // Create a "success" fetch we don't expect to get called just yet.
  AddSuccessfulOAuhTokenResponse();

  // Transient error will repeat until backoff period expires.
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(0, access_token_failure_count_);
  std::vector<std::string> scope_list;
  scope_list.push_back("scope");
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher1(
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          kEmail, oauth2_service_delegate_->GetURLLoaderFactory(), this));
  fetcher1->Start("foo", "bar", scope_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(1, access_token_failure_count_);

  // Notify of network change and ensure that request now runs.
  oauth2_service_delegate_->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher2(
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          kEmail, oauth2_service_delegate_->GetURLLoaderFactory(), this));
  fetcher2->Start("foo", "bar", scope_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, access_token_success_count_);
  EXPECT_EQ(1, access_token_failure_count_);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, CanonicalizeAccountId) {
  pref_service_.SetInteger(prefs::kAccountIdMigrationState,
                           AccountTrackerService::MIGRATION_NOT_STARTED);
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kMirror);
  std::map<std::string, std::string> tokens;
  tokens["AccountId-user@gmail.com"] = "refresh_token";
  tokens["AccountId-Foo.Bar@gmail.com"] = "refresh_token";
  tokens["AccountId-12345"] = "refresh_token";

  oauth2_service_delegate_->LoadAllCredentialsIntoMemory(tokens);

  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("user@gmail.com"));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("foobar@gmail.com"));
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable("12345"));
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       CanonAndNonCanonAccountId) {
  pref_service_.SetInteger(prefs::kAccountIdMigrationState,
                           AccountTrackerService::MIGRATION_NOT_STARTED);
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kMirror);
  std::map<std::string, std::string> tokens;
  tokens["AccountId-Foo.Bar@gmail.com"] = "bad_token";
  tokens["AccountId-foobar@gmail.com"] = "good_token";

  oauth2_service_delegate_->LoadAllCredentialsIntoMemory(tokens);

  EXPECT_EQ(1u, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("foobar@gmail.com"));
  EXPECT_STREQ(
      "good_token",
      oauth2_service_delegate_->GetRefreshToken("foobar@gmail.com").c_str());
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, ShutdownService) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kMirror);
  EXPECT_TRUE(oauth2_service_delegate_->GetAccounts().empty());
  oauth2_service_delegate_->UpdateCredentials("account_id1", "refresh_token1");
  oauth2_service_delegate_->UpdateCredentials("account_id2", "refresh_token2");
  std::vector<std::string> accounts = oauth2_service_delegate_->GetAccounts();
  EXPECT_EQ(2u, accounts.size());
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), "account_id1"));
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), "account_id2"));
  oauth2_service_delegate_->LoadCredentials("account_id1");
  oauth2_service_delegate_->UpdateCredentials("account_id1", "refresh_token3");
  oauth2_service_delegate_->Shutdown();
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  EXPECT_TRUE(oauth2_service_delegate_->refresh_tokens_.empty());
  EXPECT_EQ(0, oauth2_service_delegate_->web_data_service_request_);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, GaiaIdMigration) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kMirror);
  if (account_tracker_service_.GetMigrationState() !=
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    std::string email = "foo@gmail.com";
    std::string gaia_id = "foo's gaia id";

    pref_service_.SetInteger(prefs::kAccountIdMigrationState,
                             AccountTrackerService::MIGRATION_NOT_STARTED);

    ListPrefUpdate update(&pref_service_,
                          AccountTrackerService::kAccountInfoPref);
    update->Clear();
    auto dict = std::make_unique<base::DictionaryValue>();
    dict->SetString("account_id", email);
    dict->SetString("email", email);
    dict->SetString("gaia", gaia_id);
    update->Append(std::move(dict));
    account_tracker_service_.Shutdown();
    account_tracker_service_.Initialize(&pref_service_, base::FilePath());

    AddAuthTokenManually("AccountId-" + email, "refresh_token");
    oauth2_service_delegate_->LoadCredentials(gaia_id);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, tokens_loaded_count_);
    EXPECT_EQ(1, token_available_count_);
    EXPECT_EQ(1, start_batch_changes_);
    EXPECT_EQ(1, end_batch_changes_);

    std::vector<std::string> accounts = oauth2_service_delegate_->GetAccounts();
    EXPECT_EQ(1u, accounts.size());

    EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(email));
    EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(gaia_id));

    account_tracker_service_.SetMigrationDone();
    oauth2_service_delegate_->Shutdown();
    ResetObserverCounts();

    oauth2_service_delegate_->LoadCredentials(gaia_id);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, tokens_loaded_count_);
    EXPECT_EQ(1, token_available_count_);
    EXPECT_EQ(1, start_batch_changes_);
    EXPECT_EQ(1, end_batch_changes_);

    EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(email));
    EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(gaia_id));
    accounts = oauth2_service_delegate_->GetAccounts();
    EXPECT_EQ(1u, accounts.size());
  }
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       GaiaIdMigrationCrashInTheMiddle) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kMirror);
  if (account_tracker_service_.GetMigrationState() !=
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    std::string email1 = "foo@gmail.com";
    std::string gaia_id1 = "foo's gaia id";
    std::string email2 = "bar@gmail.com";
    std::string gaia_id2 = "bar's gaia id";

    pref_service_.SetInteger(prefs::kAccountIdMigrationState,
                             AccountTrackerService::MIGRATION_NOT_STARTED);

    ListPrefUpdate update(&pref_service_,
                          AccountTrackerService::kAccountInfoPref);
    update->Clear();
    auto dict = std::make_unique<base::DictionaryValue>();
    dict->SetString("account_id", email1);
    dict->SetString("email", email1);
    dict->SetString("gaia", gaia_id1);
    update->Append(std::move(dict));
    dict = std::make_unique<base::DictionaryValue>();
    dict->SetString("account_id", email2);
    dict->SetString("email", email2);
    dict->SetString("gaia", gaia_id2);
    update->Append(std::move(dict));
    account_tracker_service_.Shutdown();
    account_tracker_service_.Initialize(&pref_service_, base::FilePath());

    AddAuthTokenManually("AccountId-" + email1, "refresh_token");
    AddAuthTokenManually("AccountId-" + email2, "refresh_token");
    AddAuthTokenManually("AccountId-" + gaia_id1, "refresh_token");
    oauth2_service_delegate_->LoadCredentials(gaia_id1);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, tokens_loaded_count_);
    EXPECT_EQ(2, token_available_count_);
    EXPECT_EQ(1, start_batch_changes_);
    EXPECT_EQ(1, end_batch_changes_);

    std::vector<std::string> accounts = oauth2_service_delegate_->GetAccounts();
    EXPECT_EQ(2u, accounts.size());

    EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(email1));
    EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(gaia_id1));
    EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(email2));
    EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(gaia_id2));

    account_tracker_service_.SetMigrationDone();
    oauth2_service_delegate_->Shutdown();
    ResetObserverCounts();

    oauth2_service_delegate_->LoadCredentials(gaia_id1);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, tokens_loaded_count_);
    EXPECT_EQ(2, token_available_count_);
    EXPECT_EQ(1, start_batch_changes_);
    EXPECT_EQ(1, end_batch_changes_);

    EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(email1));
    EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(gaia_id1));
    EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(email2));
    EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(gaia_id2));
    accounts = oauth2_service_delegate_->GetAccounts();
    EXPECT_EQ(2u, accounts.size());
  }
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       LoadPrimaryAccountOnlyWhenAccountConsistencyDisabled) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  std::string primary_account = "primaryaccount";
  std::string secondary_account = "secondaryaccount";

  oauth2_service_delegate_->RevokeAllCredentials();
  ResetObserverCounts();
  AddAuthTokenManually("AccountId-" + primary_account, "refresh_token");
  AddAuthTokenManually("AccountId-" + secondary_account, "refresh_token");
  oauth2_service_delegate_->LoadCredentials(primary_account);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(primary_account));
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(secondary_account));
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       LoadSecondaryAccountsWhenMirrorEnabled) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kMirror);
  std::string primary_account = "primaryaccount";
  std::string secondary_account = "secondaryaccount";

  oauth2_service_delegate_->RevokeAllCredentials();
  ResetObserverCounts();
  AddAuthTokenManually("AccountId-" + primary_account, "refresh_token");
  AddAuthTokenManually("AccountId-" + secondary_account, "refresh_token");
  oauth2_service_delegate_->LoadCredentials(primary_account);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(primary_account));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(secondary_account));
}

// Regression test for https://crbug.com/823707
// Checks that OnAuthErrorChanged() is called during UpdateCredentials(), and
// that RefreshTokenIsAvailable() can be used at this time.
TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, OnAuthErrorChanged) {
  class ControllerErrorObserver : public SigninErrorController::Observer {
   public:
    explicit ControllerErrorObserver(
        MutableProfileOAuth2TokenServiceDelegate* delegate)
        : delegate_(delegate) {}

    void OnErrorChanged() override {
      error_changed_ = true;
      EXPECT_TRUE(delegate_->RefreshTokenIsAvailable("account_id"));
    }

    MutableProfileOAuth2TokenServiceDelegate* delegate_;
    bool error_changed_ = false;

    DISALLOW_COPY_AND_ASSIGN(ControllerErrorObserver);
  };

  class TokenServiceErrorObserver : public OAuth2TokenService::Observer {
   public:
    explicit TokenServiceErrorObserver(
        MutableProfileOAuth2TokenServiceDelegate* delegate)
        : delegate_(delegate) {}

    void OnAuthErrorChanged(const std::string& account_id,
                            const GoogleServiceAuthError& auth_error) override {
      error_changed_ = true;
      EXPECT_EQ("account_id", account_id);
      EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(), auth_error);
      EXPECT_TRUE(delegate_->RefreshTokenIsAvailable("account_id"));
      EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
                delegate_->GetAuthError("account_id"));
    }

    MutableProfileOAuth2TokenServiceDelegate* delegate_;
    bool error_changed_ = false;

    DISALLOW_COPY_AND_ASSIGN(TokenServiceErrorObserver);
  };

  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);

  // Start with the SigninErrorController in error state, so that it calls
  // OnErrorChanged() from AddProvider().
  oauth2_service_delegate_->UpdateCredentials(
      "error_account_id",
      MutableProfileOAuth2TokenServiceDelegate::kInvalidRefreshToken);

  TokenServiceErrorObserver token_service_observer(
      oauth2_service_delegate_.get());
  ControllerErrorObserver controller_observer(oauth2_service_delegate_.get());
  oauth2_service_delegate_->AddObserver(&token_service_observer);
  signin_error_controller_.AddObserver(&controller_observer);

  ASSERT_FALSE(token_service_observer.error_changed_);
  ASSERT_FALSE(controller_observer.error_changed_);
  oauth2_service_delegate_->UpdateCredentials("account_id", "token");
  EXPECT_TRUE(token_service_observer.error_changed_);
  EXPECT_TRUE(controller_observer.error_changed_);

  oauth2_service_delegate_->RemoveObserver(&token_service_observer);
  signin_error_controller_.RemoveObserver(&controller_observer);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, GetAuthError) {
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  // Accounts have no error by default.
  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token");
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError("account_id"));
  // Update the error.
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
  oauth2_service_delegate_->UpdateAuthError("account_id", error);
  EXPECT_EQ(error, oauth2_service_delegate_->GetAuthError("account_id"));
  // Unknown account has no error.
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError("foo"));
  // Add account with invalid token.
  oauth2_service_delegate_->UpdateCredentials(
      "account_id_2",
      MutableProfileOAuth2TokenServiceDelegate::kInvalidRefreshToken);
  EXPECT_EQ(GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                    CREDENTIALS_REJECTED_BY_CLIENT),
            oauth2_service_delegate_->GetAuthError("account_id_2"));
}

// Checks that OnAuthErrorChanged() is called before OnRefreshTokenAvailable,
// and that the error state is correctly available from within both calls.
// Regression test for https://crbug.com/824791.
TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       InvalidTokenObserverCallsOrdering) {
  class TokenServiceErrorObserver : public OAuth2TokenService::Observer {
   public:
    explicit TokenServiceErrorObserver(
        MutableProfileOAuth2TokenServiceDelegate* delegate)
        : delegate_(delegate) {}

    void OnAuthErrorChanged(const std::string& account_id,
                            const GoogleServiceAuthError& auth_error) override {
      error_changed_ = true;
      EXPECT_FALSE(token_available_)
          << "OnAuthErrorChanged() should be called first";
      EXPECT_EQ(auth_error, delegate_->GetAuthError(account_id));
      CheckTokenState(account_id);
    }

    void OnRefreshTokenAvailable(const std::string& account_id) override {
      token_available_ = true;
      EXPECT_TRUE(error_changed_)
          << "OnAuthErrorChanged() should be called first";
      CheckTokenState(account_id);
    }

    void CheckTokenState(const std::string& account_id) {
      EXPECT_EQ("account_id", account_id);
      EXPECT_TRUE(delegate_->RefreshTokenIsAvailable("account_id"));
      EXPECT_EQ(GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                    GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                        CREDENTIALS_REJECTED_BY_CLIENT),
                delegate_->GetAuthError("account_id"));
    }

    MutableProfileOAuth2TokenServiceDelegate* delegate_;
    bool error_changed_ = false;
    bool token_available_ = false;

    DISALLOW_COPY_AND_ASSIGN(TokenServiceErrorObserver);
  };

  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  TokenServiceErrorObserver token_service_observer(
      oauth2_service_delegate_.get());
  oauth2_service_delegate_->AddObserver(&token_service_observer);
  oauth2_service_delegate_->UpdateCredentials(
      "account_id",
      MutableProfileOAuth2TokenServiceDelegate::kInvalidRefreshToken);
  EXPECT_TRUE(token_service_observer.token_available_);
  EXPECT_TRUE(token_service_observer.error_changed_);
  oauth2_service_delegate_->RemoveObserver(&token_service_observer);
}

// Checks that set_revoke_all_tokens_on_first_load() revokes the tokens,
// updates the database, and is applied only once.
TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, ClearTokensOnStartup) {
  client_->SetNetworkCallsDelayed(true);
  revoke_all_tokens_on_load_ = true;
  CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  std::string primary_account = "primaryaccount";
  std::string secondary_account = "secondaryaccount";

  oauth2_service_delegate_->RevokeAllCredentials();
  ResetObserverCounts();
  AddAuthTokenManually("AccountId-" + primary_account, "refresh_token");
  AddAuthTokenManually("AccountId-" + secondary_account, "refresh_token");
  oauth2_service_delegate_->LoadCredentials(primary_account);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(primary_account));
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(secondary_account));
  EXPECT_STREQ(
      MutableProfileOAuth2TokenServiceDelegate::kInvalidRefreshToken,
      oauth2_service_delegate_->GetRefreshToken(primary_account).c_str());
  EXPECT_EQ(GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                    CREDENTIALS_REJECTED_BY_CLIENT),
            oauth2_service_delegate_->GetAuthError(primary_account));

  // Tokens are revoked on the server.
  EXPECT_EQ(2u, oauth2_service_delegate_->server_revokes_.size());
  client_->SetNetworkCallsDelayed(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());

  // Check that the changes have been persisted in the database: tokens are not
  // revoked again on the server.
  client_->SetNetworkCallsDelayed(true);
  oauth2_service_delegate_->LoadCredentials(primary_account);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(primary_account));
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(secondary_account));
  EXPECT_STREQ(
      MutableProfileOAuth2TokenServiceDelegate::kInvalidRefreshToken,
      oauth2_service_delegate_->GetRefreshToken(primary_account).c_str());
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
}
