// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_response_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/scoped_observer.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/signin/mutable_profile_oauth2_token_service_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/fake_oauth2_token_service_delegate.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using signin::DiceAction;
using signin::DiceResponseParams;

namespace {

const char kAuthorizationCode[] = "authorization_code";
const char kEmail[] = "email";
const char kGaiaID[] = "gaia_id";
const int kSessionIndex = 42;

// TestSigninClient implementation that intercepts the GaiaAuthConsumer and
// replaces it by a dummy one.
class DiceTestSigninClient : public TestSigninClient, public GaiaAuthConsumer {
 public:
  explicit DiceTestSigninClient(PrefService* pref_service)
      : TestSigninClient(pref_service), consumer_(nullptr) {}

  ~DiceTestSigninClient() override {}

  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      const std::string& source,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override {
    DCHECK(!consumer_ || (consumer_ == consumer));
    consumer_ = consumer;

    // Pass |this| as a dummy consumer to CreateGaiaAuthFetcher().
    // Since DiceTestSigninClient does not overrides any consumer method,
    // everything will be dropped on the floor.
    return TestSigninClient::CreateGaiaAuthFetcher(this, source,
                                                   url_loader_factory);
  }

  GaiaAuthConsumer* consumer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DiceTestSigninClient);
};

// Checks if OnRefreshTokenAvailable() has been called for the specified
// account.
class DiceTestTokenServiceObserver : public OAuth2TokenService::Observer {
 public:
  explicit DiceTestTokenServiceObserver(const std::string& account_id)
      : account_id_(account_id) {}

  bool token_received() { return token_received_; }

 private:
  // OAuth2TokenServiceObserver:
  void OnRefreshTokenAvailable(const std::string& account_id) override {
    if (account_id == account_id_)
      token_received_ = true;
  }

  bool token_received_ = false;
  std::string account_id_;

  DISALLOW_COPY_AND_ASSIGN(DiceTestTokenServiceObserver);
};

class DiceResponseHandlerTest : public testing::Test,
                                public AccountReconcilor::Observer {
 public:
  // Called after the refresh token was fetched and added in the token service.
  void EnableSync(const std::string& account_id) {
    enable_sync_account_id_ = account_id;
  }

  void HandleTokenExchangeFailure(const std::string& email,
                                  const GoogleServiceAuthError& error) {
    auth_error_email_ = email;
    auth_error_ = error;
  }

 protected:
  DiceResponseHandlerTest()
      : loop_(base::MessageLoop::TYPE_IO),  // URLRequestContext requires IO.
        task_runner_(new base::TestMockTimeTaskRunner()),
        signin_client_(&pref_service_),
        token_service_(&pref_service_,
                       std::make_unique<FakeOAuth2TokenServiceDelegate>()),
        signin_error_controller_(
            SigninErrorController::AccountMode::PRIMARY_ACCOUNT),
        signin_manager_(&signin_client_,
                        &token_service_,
                        &account_tracker_service_,
                        nullptr,
                        &signin_error_controller_),
        cookie_service_(&token_service_,
                        GaiaConstants::kChromeSource,
                        &signin_client_,
                        /*use_fake_url_fetcher=*/false),
        about_signin_internals_(&token_service_,
                                &account_tracker_service_,
                                &signin_manager_,
                                &signin_error_controller_,
                                &cookie_service_,
                                signin::AccountConsistencyMethod::kDice),
        reconcilor_blocked_count_(0),
        reconcilor_unblocked_count_(0) {
    loop_.SetTaskRunner(task_runner_);
    DCHECK_EQ(task_runner_, base::ThreadTaskRunnerHandle::Get());
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    AboutSigninInternals::RegisterPrefs(pref_service_.registry());
    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    ProfileOAuth2TokenService::RegisterProfilePrefs(pref_service_.registry());
    SigninManager::RegisterProfilePrefs(pref_service_.registry());
    auto account_reconcilor_delegate =
        std::make_unique<signin::DiceAccountReconcilorDelegate>(
            &signin_client_, signin::AccountConsistencyMethod::kDisabled);
    account_reconcilor_ = std::make_unique<AccountReconcilor>(
        &token_service_, &signin_manager_, &signin_client_, nullptr,
        std::move(account_reconcilor_delegate));
    about_signin_internals_.Initialize(&signin_client_);
    account_tracker_service_.Initialize(&pref_service_, base::FilePath());
    account_reconcilor_->AddObserver(this);
  }

  ~DiceResponseHandlerTest() override {
    account_reconcilor_->RemoveObserver(this);
    account_reconcilor_->Shutdown();
    about_signin_internals_.Shutdown();
    cookie_service_.Shutdown();
    signin_error_controller_.Shutdown();
    signin_manager_.Shutdown();
    account_tracker_service_.Shutdown();
    token_service_.Shutdown();
    signin_client_.Shutdown();
    task_runner_->ClearPendingTasks();
  }

  void InitializeDiceResponseHandler(
      signin::AccountConsistencyMethod account_consistency) {
    DCHECK(!dice_response_handler_);
    dice_response_handler_ = std::make_unique<DiceResponseHandler>(
        &signin_client_, &signin_manager_, &token_service_,
        &account_tracker_service_, account_reconcilor_.get(),
        &about_signin_internals_, account_consistency, temp_dir_.GetPath());
  }

  DiceResponseParams MakeDiceParams(DiceAction action) {
    DiceResponseParams dice_params;
    dice_params.user_intention = action;
    DiceResponseParams::AccountInfo account_info;
    account_info.gaia_id = kGaiaID;
    account_info.email = kEmail;
    account_info.session_index = kSessionIndex;
    switch (action) {
      case DiceAction::SIGNIN:
        dice_params.signin_info =
            std::make_unique<DiceResponseParams::SigninInfo>();
        dice_params.signin_info->account_info = account_info;
        dice_params.signin_info->authorization_code = kAuthorizationCode;
        break;
      case DiceAction::ENABLE_SYNC:
        dice_params.enable_sync_info =
            std::make_unique<DiceResponseParams::EnableSyncInfo>();
        dice_params.enable_sync_info->account_info = account_info;
        break;
      case DiceAction::SIGNOUT:
        dice_params.signout_info =
            std::make_unique<DiceResponseParams::SignoutInfo>();
        dice_params.signout_info->account_infos.push_back(account_info);
        break;
      case DiceAction::NONE:
        NOTREACHED();
        break;
    }
    return dice_params;
  }

  std::string GetRefreshToken(const std::string& account_id) {
    return static_cast<FakeOAuth2TokenServiceDelegate*>(
               token_service_.GetDelegate())
        ->GetRefreshToken(account_id);
  }

  // AccountReconcilor::Observer:
  void OnBlockReconcile() override { ++reconcilor_blocked_count_; }
  void OnUnblockReconcile() override { ++reconcilor_unblocked_count_; }

  base::MessageLoop loop_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  DiceTestSigninClient signin_client_;
  ProfileOAuth2TokenService token_service_;
  AccountTrackerService account_tracker_service_;
  SigninErrorController signin_error_controller_;
  FakeSigninManager signin_manager_;
  FakeGaiaCookieManagerService cookie_service_;
  AboutSigninInternals about_signin_internals_;
  std::unique_ptr<AccountReconcilor> account_reconcilor_;
  std::unique_ptr<DiceResponseHandler> dice_response_handler_;
  int reconcilor_blocked_count_;
  int reconcilor_unblocked_count_;
  std::string enable_sync_account_id_;
  GoogleServiceAuthError auth_error_;
  std::string auth_error_email_;
};

class TestProcessDiceHeaderDelegate : public ProcessDiceHeaderDelegate {
 public:
  explicit TestProcessDiceHeaderDelegate(DiceResponseHandlerTest* owner)
      : owner_(owner) {}

  ~TestProcessDiceHeaderDelegate() override = default;

  // Called after the refresh token was fetched and added in the token service.
  void EnableSync(const std::string& account_id) override {
    owner_->EnableSync(account_id);
  }

  void HandleTokenExchangeFailure(
      const std::string& email,
      const GoogleServiceAuthError& error) override {
    owner_->HandleTokenExchangeFailure(email, error);
  }

 private:
  DiceResponseHandlerTest* owner_;
};

// Checks that a SIGNIN action triggers a token exchange request.
TEST_F(DiceResponseHandlerTest, Signin) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  const auto& account_info = dice_params.signin_info->account_info;
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created.
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
  // Simulate GaiaAuthFetcher success.
  signin_client_.consumer_->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("refresh_token", "access_token", 10,
                                          false /* is_child_account */,
                                          true /* is_advanced_protection*/));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(auth_error_email_.empty());
  EXPECT_EQ(GoogleServiceAuthError::NONE, auth_error_.state());
  // Check that the reconcilor was blocked and unblocked exactly once.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(1, reconcilor_unblocked_count_);
  // Check that the AccountInfo::is_under_advanced_protection is set.
  EXPECT_TRUE(account_tracker_service_.GetAccountInfo(account_id)
                  .is_under_advanced_protection);
}

// Checks that a GaiaAuthFetcher failure is handled correctly.
TEST_F(DiceResponseHandlerTest, SigninFailure) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  const auto& account_info = dice_params.signin_info->account_info;
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created.
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  EXPECT_EQ(
      1u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Simulate GaiaAuthFetcher failure.
  GoogleServiceAuthError::State error_state =
      GoogleServiceAuthError::SERVICE_UNAVAILABLE;
  signin_client_.consumer_->OnClientOAuthFailure(
      GoogleServiceAuthError(error_state));
  EXPECT_EQ(
      0u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Check that the token has not been inserted in the token service.
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_EQ(account_info.email, auth_error_email_);
  EXPECT_EQ(error_state, auth_error_.state());
}

// Checks that a second token for the same account is not requested when a
// request is already in flight.
TEST_F(DiceResponseHandlerTest, SigninRepeatedWithSameAccount) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  const auto& account_info = dice_params.signin_info->account_info;
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created.
  GaiaAuthConsumer* consumer = signin_client_.consumer_;
  ASSERT_THAT(consumer, testing::NotNull());
  // Start a second request for the same account.
  signin_client_.consumer_ = nullptr;
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that there is no new request.
  ASSERT_THAT(signin_client_.consumer_, testing::IsNull());
  // Simulate GaiaAuthFetcher success for the first request.
  consumer->OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
      "refresh_token", "access_token", 10, false /* is_child_account */,
      false /* is_advanced_protection*/));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_FALSE(account_tracker_service_.GetAccountInfo(account_id)
                   .is_under_advanced_protection);
}

// Checks that two SIGNIN requests can happen concurrently.
TEST_F(DiceResponseHandlerTest, SigninWithTwoAccounts) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  DiceResponseParams dice_params_1 = MakeDiceParams(DiceAction::SIGNIN);
  const auto& account_info_1 = dice_params_1.signin_info->account_info;
  DiceResponseParams dice_params_2 = MakeDiceParams(DiceAction::SIGNIN);
  dice_params_2.signin_info->account_info.email = "other_email";
  dice_params_2.signin_info->account_info.gaia_id = "other_gaia_id";
  const auto& account_info_2 = dice_params_2.signin_info->account_info;
  std::string account_id_1 = account_tracker_service_.PickAccountIdForAccount(
      account_info_1.gaia_id, account_info_1.email);
  std::string account_id_2 = account_tracker_service_.PickAccountIdForAccount(
      account_info_2.gaia_id, account_info_2.email);
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id_1));
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id_2));
  // Start first request.
  dice_response_handler_->ProcessDiceHeader(
      dice_params_1, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created.
  GaiaAuthConsumer* consumer_1 = signin_client_.consumer_;
  ASSERT_THAT(consumer_1, testing::NotNull());
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
  // Start second request.
  signin_client_.consumer_ = nullptr;
  dice_response_handler_->ProcessDiceHeader(
      dice_params_2, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  GaiaAuthConsumer* consumer_2 = signin_client_.consumer_;
  ASSERT_THAT(consumer_2, testing::NotNull());
  // Simulate GaiaAuthFetcher success for the first request.
  consumer_1->OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
      "refresh_token", "access_token", 10, false /* is_child_account */,
      true /* is_advanced_protection*/));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id_1));
  EXPECT_TRUE(account_tracker_service_.GetAccountInfo(account_id_1)
                  .is_under_advanced_protection);
  // Simulate GaiaAuthFetcher success for the second request.
  consumer_2->OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
      "refresh_token", "access_token", 10, false /* is_child_account */,
      false /* is_advanced_protection*/));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id_2));
  EXPECT_FALSE(account_tracker_service_.GetAccountInfo(account_id_2)
                   .is_under_advanced_protection);
  // Check that the reconcilor was blocked and unblocked exactly once.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(1, reconcilor_unblocked_count_);
}

// Checks that a ENABLE_SYNC action received after the refresh token is added
// to the token service, triggers a call to enable sync on the delegate.
TEST_F(DiceResponseHandlerTest, SigninEnableSyncAfterRefreshTokenFetched) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  const auto& account_info = dice_params.signin_info->account_info;
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created.
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  // Simulate GaiaAuthFetcher success.
  signin_client_.consumer_->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("refresh_token", "access_token", 10,
                                          false /* is_child_account */,
                                          false /* is_advanced_protection*/));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  // Check that delegate was not called to enable sync.
  EXPECT_EQ("", enable_sync_account_id_);

  // Enable sync.
  dice_response_handler_->ProcessDiceHeader(
      MakeDiceParams(DiceAction::ENABLE_SYNC),
      std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that delegate was called to enable sync.
  EXPECT_EQ(account_id, enable_sync_account_id_);
}

// Checks that a ENABLE_SYNC action received before the refresh token is added
// to the token service, is schedules a call to enable sync on the delegate
// once the refresh token is received.
TEST_F(DiceResponseHandlerTest, SigninEnableSyncBeforeRefreshTokenFetched) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  const auto& account_info = dice_params.signin_info->account_info;
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created.
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());

  // Enable sync.
  dice_response_handler_->ProcessDiceHeader(
      MakeDiceParams(DiceAction::ENABLE_SYNC),
      std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that delegate was not called to enable sync.
  EXPECT_EQ("", enable_sync_account_id_);

  // Simulate GaiaAuthFetcher success.
  signin_client_.consumer_->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("refresh_token", "access_token", 10,
                                          false /* is_child_account */,
                                          false /* is_advanced_protection*/));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  // Check that delegate was called to enable sync.
  EXPECT_EQ(account_id, enable_sync_account_id_);
}

TEST_F(DiceResponseHandlerTest, Timeout) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  const auto& account_info = dice_params.signin_info->account_info;
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created.
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  EXPECT_EQ(
      1u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Force a timeout.
  task_runner_->FastForwardBy(
      base::TimeDelta::FromSeconds(kDiceTokenFetchTimeoutSeconds + 1));
  EXPECT_EQ(
      0u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Check that the token has not been inserted in the token service.
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  // Check that the reconcilor was blocked and unblocked exactly once.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(1, reconcilor_unblocked_count_);
}

TEST_F(DiceResponseHandlerTest, SignoutMainAccount) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  const char kSecondaryGaiaID[] = "secondary_account";
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  const auto& account_info = dice_params.signout_info->account_infos[0];
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  // User is signed in to Chrome, and has some refresh token for a secondary
  // account.
  signin_manager_.SignIn(account_info.gaia_id, account_info.email, "password");
  token_service_.UpdateCredentials(account_id, "token1");
  token_service_.UpdateCredentials(kSecondaryGaiaID, "token2");
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(kSecondaryGaiaID));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());
  // Receive signout response for the main account.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));

  // User is not signed out, token for the main account is now invalid,
  // secondary account is untouched.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_EQ(MutableProfileOAuth2TokenServiceDelegate::kInvalidRefreshToken,
            GetRefreshToken(account_id));
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(kSecondaryGaiaID));
  EXPECT_EQ("token2", GetRefreshToken(kSecondaryGaiaID));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());
  // Check that the reconcilor was not blocked.
  EXPECT_EQ(0, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
}

TEST_F(DiceResponseHandlerTest, MigrationSignout) {
  InitializeDiceResponseHandler(
      signin::AccountConsistencyMethod::kDiceMigration);
  const char kSecondaryGaiaID[] = "secondary_account";
  const char kSecondaryEmail[] = "other@gmail.com";
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  dice_params.signout_info->account_infos.emplace_back(kSecondaryGaiaID,
                                                       kSecondaryEmail, 1);
  const auto& main_account_info = dice_params.signout_info->account_infos[0];
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      main_account_info.gaia_id, main_account_info.email);
  std::string secondary_account_id =
      account_tracker_service_.PickAccountIdForAccount(kSecondaryGaiaID,
                                                       kSecondaryEmail);

  // User is signed in to Chrome, and has some refresh token for a secondary
  // account.
  signin_manager_.SignIn(main_account_info.gaia_id, main_account_info.email,
                         "password");
  token_service_.UpdateCredentials(account_id, "token1");
  token_service_.UpdateCredentials(secondary_account_id, "token2");
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(secondary_account_id));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());

  // Receive signout response for all accounts.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));

  // User is not signed out from Chrome, only the secondary token is deleted.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(secondary_account_id));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());
  // Check that the reconcilor was not blocked.
  EXPECT_EQ(0, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
}

TEST_F(DiceResponseHandlerTest, SignoutSecondaryAccount) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  const char kMainGaiaID[] = "main_account";
  const char kMainEmail[] = "main@gmail.com";
  std::string main_account_id =
      account_tracker_service_.PickAccountIdForAccount(kMainGaiaID, kMainEmail);
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  const auto& account_info = dice_params.signout_info->account_infos[0];
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  // User is signed in to Chrome, and has some refresh token for a secondary
  // account.
  signin_manager_.SignIn(kMainGaiaID, "user", "password");
  token_service_.UpdateCredentials(main_account_id, "token2");
  token_service_.UpdateCredentials(account_id, "token1");
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(main_account_id));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());
  // Receive signout response for the secondary account.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));

  // Only the token corresponding the the Dice parameter has been removed, and
  // the user is still signed in.
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(main_account_id));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());
}

TEST_F(DiceResponseHandlerTest, SignoutWebOnly) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  const char kSecondaryAccountID[] = "secondary_account";
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  const auto& account_info = dice_params.signout_info->account_infos[0];
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  // User is NOT signed in to Chrome, and has some refresh tokens for two
  // accounts.
  token_service_.UpdateCredentials(account_id, "refresh_token");
  token_service_.UpdateCredentials(kSecondaryAccountID, "refresh_token");
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(kSecondaryAccountID));
  EXPECT_FALSE(signin_manager_.IsAuthenticated());
  // Receive signout response.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Only the token corresponding the the Dice parameter has been removed.
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(kSecondaryAccountID));
  EXPECT_FALSE(signin_manager_.IsAuthenticated());
}

// Checks that signin in progress is canceled by a signout.
TEST_F(DiceResponseHandlerTest, SigninSignoutSameAccount) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  const auto& account_info = dice_params.signout_info->account_infos[0];
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  // User is signed in to Chrome.
  signin_manager_.SignIn(account_info.gaia_id, account_info.email, "password");
  token_service_.UpdateCredentials(account_id, "token");
  ASSERT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  ASSERT_FALSE(token_service_.RefreshTokenHasError(account_id));
  // Start Dice signin (reauth).
  DiceResponseParams dice_params_2 = MakeDiceParams(DiceAction::SIGNIN);
  dice_response_handler_->ProcessDiceHeader(
      dice_params_2, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created and is pending.
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  EXPECT_EQ(
      1u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Signout while signin is in flight.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that the token fetcher has been canceled and the token is invalid.
  EXPECT_EQ(
      0u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_EQ(MutableProfileOAuth2TokenServiceDelegate::kInvalidRefreshToken,
            GetRefreshToken(account_id));
}

// Checks that signin in progress is not canceled by a signout for a different
// account.
TEST_F(DiceResponseHandlerTest, SigninSignoutDifferentAccount) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  // User starts signin in the web with two accounts.
  DiceResponseParams signout_params_1 = MakeDiceParams(DiceAction::SIGNOUT);
  DiceResponseParams signin_params_1 = MakeDiceParams(DiceAction::SIGNIN);
  DiceResponseParams signin_params_2 = MakeDiceParams(DiceAction::SIGNIN);
  signin_params_2.signin_info->account_info.email = "other_email";
  signin_params_2.signin_info->account_info.gaia_id = "other_gaia_id";
  const auto& signin_account_info_1 = signin_params_1.signin_info->account_info;
  const auto& signin_account_info_2 = signin_params_2.signin_info->account_info;
  std::string account_id_1 = account_tracker_service_.PickAccountIdForAccount(
      signin_account_info_1.gaia_id, signin_account_info_1.email);
  std::string account_id_2 = account_tracker_service_.PickAccountIdForAccount(
      signin_account_info_2.gaia_id, signin_account_info_2.email);
  dice_response_handler_->ProcessDiceHeader(
      signin_params_1, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  signin_client_.consumer_ = nullptr;
  dice_response_handler_->ProcessDiceHeader(
      signin_params_2, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  EXPECT_EQ(
      2u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id_1));
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id_2));
  ASSERT_FALSE(token_service_.RefreshTokenHasError(account_id_1));
  ASSERT_FALSE(token_service_.RefreshTokenHasError(account_id_2));
  // Signout from one of the accounts while signin is in flight.
  dice_response_handler_->ProcessDiceHeader(
      signout_params_1, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that one of the fetchers is cancelled.
  EXPECT_EQ(
      1u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Allow the remaining fetcher to complete.
  signin_client_.consumer_->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("refresh_token", "access_token", 10,
                                          false /* is_child_account */,
                                          false /* is_advanced_protection*/));
  EXPECT_EQ(
      0u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Check that the right token is available.
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(account_id_1));
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id_2));
  EXPECT_EQ("refresh_token", GetRefreshToken(account_id_2));
}

// Checks that no auth error fix happens if the user is signed out.
TEST_F(DiceResponseHandlerTest, FixAuthErrorSignedOut) {
  InitializeDiceResponseHandler(
      signin::AccountConsistencyMethod::kDiceFixAuthErrors);
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  const auto& account_info = dice_params.signin_info->account_info;
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(
      account_tracker_service_.PickAccountIdForAccount(account_info.gaia_id,
                                                       account_info.email)));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has not been created.
  ASSERT_THAT(signin_client_.consumer_, testing::IsNull());
}

// Checks that the token is not stored if the user signs out during the token
// request.
TEST_F(DiceResponseHandlerTest, FixAuthErrorSignOutDuringRequest) {
  InitializeDiceResponseHandler(
      signin::AccountConsistencyMethod::kDiceFixAuthErrors);
  // User is signed in to Chrome.
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  const auto& account_info = dice_params.signin_info->account_info;
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  signin_manager_.SignIn(account_info.gaia_id, account_info.email, "password");
  token_service_.UpdateCredentials(account_id, "token1");
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());
  // Start re-authentication on the web.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created.
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  // Sign out.
  signin_manager_.ForceSignOut();
  // Simulate GaiaAuthFetcher success.
  signin_client_.consumer_->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("refresh_token", "access_token", 10,
                                          false /* is_child_account */,
                                          false /* is_advanced_protection*/));
  // Check that the token has not been inserted in the token service.
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
}

// Checks that the token is fixed if the Chrome account matches the web account.
TEST_F(DiceResponseHandlerTest, FixAuthError) {
  InitializeDiceResponseHandler(
      signin::AccountConsistencyMethod::kDiceFixAuthErrors);
  // User is signed in to Chrome.
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  const auto& account_info = dice_params.signin_info->account_info;
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  signin_manager_.SignIn(account_info.gaia_id, account_info.email, "password");
  token_service_.UpdateCredentials(account_id, "token1");
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());
  // Start re-authentication on the web.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created.
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  // We need to listen for new token notifications, since there is no way to
  // check the actual value of the token in the token service.
  std::unique_ptr<DiceTestTokenServiceObserver> token_service_observer =
      std::make_unique<DiceTestTokenServiceObserver>(account_info.gaia_id);
  ScopedObserver<ProfileOAuth2TokenService, DiceTestTokenServiceObserver>
      scoped_token_service_observer(token_service_observer.get());
  scoped_token_service_observer.Add(&token_service_);
  // Simulate GaiaAuthFetcher success.
  signin_client_.consumer_->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("refresh_token", "access_token", 10,
                                          false /* is_child_account */,
                                          false /* is_advanced_protection*/));
  // Check that the token has not been inserted in the token service.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(token_service_observer->token_received());
  // Check that the reconcilor was blocked and unblocked.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(1, reconcilor_unblocked_count_);
}

// Tests that the Dice Signout response is ignored when kDiceFixAuthErrors is
// used.
TEST_F(DiceResponseHandlerTest, FixAuthErroDoesNotSignout) {
  InitializeDiceResponseHandler(
      signin::AccountConsistencyMethod::kDiceFixAuthErrors);
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  const auto& account_info = dice_params.signout_info->account_infos[0];
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  // User is signed in to Chrome, and has some refresh token for a secondary
  // account.
  signin_manager_.SignIn(account_info.gaia_id, account_info.email, "password");
  token_service_.UpdateCredentials(account_id, "token1");
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());
  // Receive signout response for the main account.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // User is not signed out from Chrome.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());
}

// Tests that the DiceResponseHandler is created for a normal profile but not
// for an incognito profile.
TEST(DiceResponseHandlerFactoryTest, NotInIncognito) {
  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile;
  EXPECT_THAT(DiceResponseHandler::GetForProfile(&profile), testing::NotNull());
  EXPECT_THAT(
      DiceResponseHandler::GetForProfile(profile.GetOffTheRecordProfile()),
      testing::IsNull());
}

}  // namespace
