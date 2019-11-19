// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_response_handler.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using signin::DiceAction;
using signin::DiceResponseParams;

namespace {

const char kAuthorizationCode[] = "authorization_code";
const char kEmail[] = "test@email.com";
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
      gaia::GaiaSource source) override {
    DCHECK(!consumer_ || (consumer_ == consumer));
    consumer_ = consumer;

    // Pass |this| as a dummy consumer to CreateGaiaAuthFetcher().
    // Since DiceTestSigninClient does not overrides any consumer method,
    // everything will be dropped on the floor.
    return TestSigninClient::CreateGaiaAuthFetcher(this, source);
  }

  // We want to reset |consumer_| here before the test interacts with the last
  // consumer. Interacting with the last consumer (simulating success of the
  // fetcher) namely sometimes immediately triggers another fetch with another
  // consumer. If |consumer_| is non-null, we would hit the DCHECK.
  GaiaAuthConsumer* GetAndClearConsumer() {
    GaiaAuthConsumer* last_consumer = consumer_;
    consumer_ = nullptr;
    return last_consumer;
  }

 private:
  GaiaAuthConsumer* consumer_;

  DISALLOW_COPY_AND_ASSIGN(DiceTestSigninClient);
};

class DiceResponseHandlerTest : public testing::Test,
                                public AccountReconcilor::Observer {
 public:
  // Called after the refresh token was fetched and added in the token service.
  void EnableSync(const CoreAccountId& account_id) {
    enable_sync_account_id_ = account_id;
  }

  void HandleTokenExchangeFailure(const std::string& email,
                                  const GoogleServiceAuthError& error) {
    auth_error_email_ = email;
    auth_error_ = error;
  }

 protected:
  DiceResponseHandlerTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::IO,
            base::test::SingleThreadTaskEnvironment::TimeSource::
                MOCK_TIME),  // URLRequestContext requires IO.
        signin_client_(&pref_service_),
        identity_test_env_(/*test_url_loader_factory=*/nullptr,
                           &pref_service_,
                           signin::AccountConsistencyMethod::kDice,
                           &signin_client_),
        signin_error_controller_(
            SigninErrorController::AccountMode::PRIMARY_ACCOUNT,
            identity_test_env_.identity_manager()),
        about_signin_internals_(identity_test_env_.identity_manager(),
                                &signin_error_controller_,
                                signin::AccountConsistencyMethod::kDice),
        reconcilor_blocked_count_(0),
        reconcilor_unblocked_count_(0) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    AboutSigninInternals::RegisterPrefs(pref_service_.registry());
    auto account_reconcilor_delegate =
        std::make_unique<signin::DiceAccountReconcilorDelegate>(
            &signin_client_, signin::AccountConsistencyMethod::kDiceMigration,
            /*migration_completed=*/false);
    account_reconcilor_ = std::make_unique<AccountReconcilor>(
        identity_test_env_.identity_manager(), &signin_client_,
        std::move(account_reconcilor_delegate));
    about_signin_internals_.Initialize(&signin_client_);
    account_reconcilor_->AddObserver(this);
  }

  ~DiceResponseHandlerTest() override {
    account_reconcilor_->RemoveObserver(this);
    account_reconcilor_->Shutdown();
    about_signin_internals_.Shutdown();
    signin_error_controller_.Shutdown();
  }

  void InitializeDiceResponseHandler(
      signin::AccountConsistencyMethod account_consistency) {
    DCHECK(!dice_response_handler_);
    dice_response_handler_ = std::make_unique<DiceResponseHandler>(
        &signin_client_, identity_test_env_.identity_manager(),
        account_reconcilor_.get(), &about_signin_internals_,
        account_consistency, temp_dir_.GetPath());
  }

  DiceResponseParams MakeDiceParams(DiceAction action) {
    DiceResponseParams dice_params;
    dice_params.user_intention = action;
    DiceResponseParams::AccountInfo account_info;
    account_info.gaia_id = signin::GetTestGaiaIdForEmail(kEmail);
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

  // AccountReconcilor::Observer:
  void OnBlockReconcile() override { ++reconcilor_blocked_count_; }
  void OnUnblockReconcile() override { ++reconcilor_unblocked_count_; }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  DiceTestSigninClient signin_client_;
  signin::IdentityTestEnvironment identity_test_env_;
  SigninErrorController signin_error_controller_;
  AboutSigninInternals about_signin_internals_;
  std::unique_ptr<AccountReconcilor> account_reconcilor_;
  std::unique_ptr<DiceResponseHandler> dice_response_handler_;
  int reconcilor_blocked_count_;
  int reconcilor_unblocked_count_;
  CoreAccountId enable_sync_account_id_;
  GoogleServiceAuthError auth_error_;
  std::string auth_error_email_;
};

class TestProcessDiceHeaderDelegate : public ProcessDiceHeaderDelegate {
 public:
  explicit TestProcessDiceHeaderDelegate(DiceResponseHandlerTest* owner)
      : owner_(owner) {}

  ~TestProcessDiceHeaderDelegate() override = default;

  // Called after the refresh token was fetched and added in the token service.
  void EnableSync(const CoreAccountId& account_id) override {
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
  CoreAccountId account_id = identity_manager()->PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created.
  GaiaAuthConsumer* consumer = signin_client_.GetAndClearConsumer();
  ASSERT_THAT(consumer, testing::NotNull());
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
  // Simulate GaiaAuthFetcher success.
  consumer->OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
      "refresh_token", "access_token", 10, false /* is_child_account */,
      true /* is_advanced_protection*/));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id));
  EXPECT_TRUE(auth_error_email_.empty());
  EXPECT_EQ(GoogleServiceAuthError::NONE, auth_error_.state());
  // Check that the reconcilor was blocked and unblocked exactly once.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(1, reconcilor_unblocked_count_);
  // Check that the AccountInfo::is_under_advanced_protection is set.
  EXPECT_TRUE(
      identity_manager()
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              account_id)
          .value()
          .is_under_advanced_protection);
}

// Checks that a GaiaAuthFetcher failure is handled correctly.
TEST_F(DiceResponseHandlerTest, SigninFailure) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  const auto& account_info = dice_params.signin_info->account_info;
  CoreAccountId account_id = identity_manager()->PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created.
  GaiaAuthConsumer* consumer = signin_client_.GetAndClearConsumer();
  ASSERT_THAT(consumer, testing::NotNull());
  EXPECT_EQ(
      1u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Simulate GaiaAuthFetcher failure.
  GoogleServiceAuthError::State error_state =
      GoogleServiceAuthError::SERVICE_UNAVAILABLE;
  consumer->OnClientOAuthFailure(GoogleServiceAuthError(error_state));
  EXPECT_EQ(
      0u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Check that the token has not been inserted in the token service.
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id));
  EXPECT_EQ(account_info.email, auth_error_email_);
  EXPECT_EQ(error_state, auth_error_.state());
}

// Checks that a second token for the same account is not requested when a
// request is already in flight.
TEST_F(DiceResponseHandlerTest, SigninRepeatedWithSameAccount) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  const auto& account_info = dice_params.signin_info->account_info;
  CoreAccountId account_id = identity_manager()->PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  ASSERT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created.
  GaiaAuthConsumer* consumer_1 = signin_client_.GetAndClearConsumer();
  ASSERT_THAT(consumer_1, testing::NotNull());
  // Start a second request for the same account.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that there is no new request.
  GaiaAuthConsumer* consumer_2 = signin_client_.GetAndClearConsumer();
  ASSERT_THAT(consumer_2, testing::IsNull());
  // Simulate GaiaAuthFetcher success for the first request.
  consumer_1->OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
      "refresh_token", "access_token", 10, false /* is_child_account */,
      false /* is_advanced_protection*/));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id));
  EXPECT_FALSE(
      identity_manager()
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              account_id)
          .value()
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
  CoreAccountId account_id_1 = identity_manager()->PickAccountIdForAccount(
      account_info_1.gaia_id, account_info_1.email);
  CoreAccountId account_id_2 = identity_manager()->PickAccountIdForAccount(
      account_info_2.gaia_id, account_info_2.email);
  ASSERT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id_1));
  ASSERT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id_2));
  // Start first request.
  dice_response_handler_->ProcessDiceHeader(
      dice_params_1, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created.
  GaiaAuthConsumer* consumer_1 = signin_client_.GetAndClearConsumer();
  ASSERT_THAT(consumer_1, testing::NotNull());
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
  // Start second request.
  dice_response_handler_->ProcessDiceHeader(
      dice_params_2, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  GaiaAuthConsumer* consumer_2 = signin_client_.GetAndClearConsumer();
  ASSERT_THAT(consumer_2, testing::NotNull());
  // Simulate GaiaAuthFetcher success for the first request.
  consumer_1->OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
      "refresh_token", "access_token", 10, false /* is_child_account */,
      true /* is_advanced_protection*/));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id_1));
  EXPECT_TRUE(
      identity_manager()
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              account_id_1)
          .value()
          .is_under_advanced_protection);
  // Simulate GaiaAuthFetcher success for the second request.
  consumer_2->OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
      "refresh_token", "access_token", 10, false /* is_child_account */,
      false /* is_advanced_protection*/));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id_2));
  EXPECT_FALSE(
      identity_manager()
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              account_id_2)
          .value()
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
  CoreAccountId account_id = identity_manager()->PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  ASSERT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created.
  GaiaAuthConsumer* consumer = signin_client_.GetAndClearConsumer();
  ASSERT_THAT(consumer, testing::NotNull());
  // Simulate GaiaAuthFetcher success.
  consumer->OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
      "refresh_token", "access_token", 10, false /* is_child_account */,
      false /* is_advanced_protection*/));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id));
  // Check that delegate was not called to enable sync.
  EXPECT_TRUE(enable_sync_account_id_.empty());

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
  CoreAccountId account_id = identity_manager()->PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  ASSERT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created.
  GaiaAuthConsumer* consumer = signin_client_.GetAndClearConsumer();
  ASSERT_THAT(consumer, testing::NotNull());

  // Enable sync.
  dice_response_handler_->ProcessDiceHeader(
      MakeDiceParams(DiceAction::ENABLE_SYNC),
      std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that delegate was not called to enable sync.
  EXPECT_TRUE(enable_sync_account_id_.empty());

  // Simulate GaiaAuthFetcher success.
  consumer->OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
      "refresh_token", "access_token", 10, false /* is_child_account */,
      false /* is_advanced_protection*/));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id));
  // Check that delegate was called to enable sync.
  EXPECT_EQ(account_id, enable_sync_account_id_);
}

TEST_F(DiceResponseHandlerTest, Timeout) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  const auto& account_info = dice_params.signin_info->account_info;
  CoreAccountId account_id = identity_manager()->PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  ASSERT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created.
  GaiaAuthConsumer* consumer = signin_client_.GetAndClearConsumer();
  ASSERT_THAT(consumer, testing::NotNull());
  EXPECT_EQ(
      1u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Force a timeout.
  task_environment_.FastForwardBy(
      base::TimeDelta::FromSeconds(kDiceTokenFetchTimeoutSeconds + 1));
  EXPECT_EQ(
      0u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Check that the token has not been inserted in the token service.
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id));
  // Check that the reconcilor was blocked and unblocked exactly once.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(1, reconcilor_unblocked_count_);
}

TEST_F(DiceResponseHandlerTest, SignoutMainAccount) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  const char kSecondaryEmail[] = "other@gmail.com";
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  const auto& dice_account_info = dice_params.signout_info->account_infos[0];
  // User is signed in to Chrome, and has some refresh token for a secondary
  // account.
  AccountInfo account_info =
      identity_test_env_.MakePrimaryAccountAvailable(dice_account_info.email);
  AccountInfo secondary_account_info =
      identity_test_env_.MakeAccountAvailable(kSecondaryEmail);
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      secondary_account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());
  // Receive signout response for the main account.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));

  // User is not signed out, token for the main account is now invalid,
  // secondary account is untouched.
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.account_id));
  auto error = identity_manager()->GetErrorStateOfRefreshTokenForAccount(
      account_info.account_id);
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS, error.state());
  EXPECT_EQ(GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_CLIENT,
            error.GetInvalidGaiaCredentialsReason());

  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      secondary_account_info.account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          secondary_account_info.account_id));

  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());
  // Check that the reconcilor was not blocked.
  EXPECT_EQ(0, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
}

TEST_F(DiceResponseHandlerTest, MigrationSignout) {
  InitializeDiceResponseHandler(
      signin::AccountConsistencyMethod::kDiceMigration);
  const char kSecondaryEmail[] = "other@gmail.com";
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  dice_params.signout_info->account_infos.emplace_back(
      signin::GetTestGaiaIdForEmail(kSecondaryEmail), kSecondaryEmail, 1);
  const auto& main_account_info = dice_params.signout_info->account_infos[0];

  // User is signed in to Chrome, and has some refresh token for a secondary
  // account.
  AccountInfo account_info =
      identity_test_env_.MakePrimaryAccountAvailable(main_account_info.email);
  AccountInfo secondary_account_info =
      identity_test_env_.MakeAccountAvailable(kSecondaryEmail);
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      secondary_account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());

  // Receive signout response for all accounts.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));

  // User is not signed out from Chrome, only the secondary token is deleted.
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(
      secondary_account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());
  // Check that the reconcilor was not blocked.
  EXPECT_EQ(0, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
}

TEST_F(DiceResponseHandlerTest, SignoutSecondaryAccount) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  const char kMainEmail[] = "main@gmail.com";
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  const auto& secondary_dice_account_info =
      dice_params.signout_info->account_infos[0];
  // User is signed in to Chrome, and has some refresh token for a secondary
  // account.
  AccountInfo main_account_info =
      identity_test_env_.MakePrimaryAccountAvailable(kMainEmail);
  AccountInfo secondary_account_info = identity_test_env_.MakeAccountAvailable(
      secondary_dice_account_info.email);
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      secondary_account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      main_account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());
  // Receive signout response for the secondary account.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));

  // Only the token corresponding the the Dice parameter has been removed, and
  // the user is still signed in.
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(
      secondary_account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      main_account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());
}

TEST_F(DiceResponseHandlerTest, SignoutWebOnly) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  const char kSecondaryEmail[] = "other@gmail.com";
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  const auto& dice_account_info = dice_params.signout_info->account_infos[0];
  // User is NOT signed in to Chrome, and has some refresh tokens for two
  // accounts.
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable(dice_account_info.email);
  AccountInfo secondary_account_info =
      identity_test_env_.MakeAccountAvailable(kSecondaryEmail);
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      secondary_account_info.account_id));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  // Receive signout response.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Only the token corresponding the the Dice parameter has been removed.
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      secondary_account_info.account_id));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
}

// Checks that signin in progress is canceled by a signout.
TEST_F(DiceResponseHandlerTest, SigninSignoutSameAccount) {
  InitializeDiceResponseHandler(signin::AccountConsistencyMethod::kDice);
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  const auto& dice_account_info = dice_params.signout_info->account_infos[0];

  // User is signed in to Chrome.
  AccountInfo account_info =
      identity_test_env_.MakePrimaryAccountAvailable(dice_account_info.email);
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.account_id));
  // Start Dice signin (reauth).
  DiceResponseParams dice_params_2 = MakeDiceParams(DiceAction::SIGNIN);
  dice_response_handler_->ProcessDiceHeader(
      dice_params_2, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created and is pending.
  ASSERT_THAT(signin_client_.GetAndClearConsumer(), testing::NotNull());
  EXPECT_EQ(
      1u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Signout while signin is in flight.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that the token fetcher has been canceled and the token is invalid.
  EXPECT_EQ(
      0u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.account_id));
  auto error = identity_manager()->GetErrorStateOfRefreshTokenForAccount(
      account_info.account_id);
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS, error.state());
  EXPECT_EQ(GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_CLIENT,
            error.GetInvalidGaiaCredentialsReason());
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
  CoreAccountId account_id_1 = identity_manager()->PickAccountIdForAccount(
      signin_account_info_1.gaia_id, signin_account_info_1.email);
  CoreAccountId account_id_2 = identity_manager()->PickAccountIdForAccount(
      signin_account_info_2.gaia_id, signin_account_info_2.email);
  dice_response_handler_->ProcessDiceHeader(
      signin_params_1, std::make_unique<TestProcessDiceHeaderDelegate>(this));

  GaiaAuthConsumer* consumer_1 = signin_client_.GetAndClearConsumer();
  ASSERT_THAT(consumer_1, testing::NotNull());
  dice_response_handler_->ProcessDiceHeader(
      signin_params_2, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  GaiaAuthConsumer* consumer_2 = signin_client_.GetAndClearConsumer();
  ASSERT_THAT(consumer_2, testing::NotNull());
  EXPECT_EQ(
      2u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  ASSERT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id_1));
  ASSERT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id_2));
  ASSERT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id_1));
  ASSERT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id_2));
  // Signout from one of the accounts while signin is in flight.
  dice_response_handler_->ProcessDiceHeader(
      signout_params_1, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that one of the fetchers is cancelled.
  EXPECT_EQ(
      1u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Allow the remaining fetcher to complete.
  consumer_2->OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
      "refresh_token", "access_token", 10, false /* is_child_account */,
      false /* is_advanced_protection*/));
  EXPECT_EQ(
      0u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Check that the right token is available.
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id_1));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id_2));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id_2));
}

// Tests that the DiceResponseHandler is created for a normal profile but not
// for an incognito profile.
TEST(DiceResponseHandlerFactoryTest, NotInIncognito) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  EXPECT_THAT(DiceResponseHandler::GetForProfile(&profile), testing::NotNull());
  EXPECT_THAT(
      DiceResponseHandler::GetForProfile(profile.GetOffTheRecordProfile()),
      testing::IsNull());
}

}  // namespace
