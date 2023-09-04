// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_response_handler.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "chrome/browser/signin/bound_session_credentials/registration_token_helper.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/unexportable_keys/fake_unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

using signin::DiceAction;
using signin::DiceResponseParams;
using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::Unused;

namespace {

const char kAuthorizationCode[] = "authorization_code";
const char kEmail[] = "test@email.com";
const int kSessionIndex = 42;

DiceResponseParams::AccountInfo GetDiceResponseParamsAccountInfo(
    const std::string& email) {
  DiceResponseParams::AccountInfo account_info;
  account_info.gaia_id = signin::GetTestGaiaIdForEmail(email);
  account_info.email = kEmail;
  account_info.session_index = kSessionIndex;
  return account_info;
}

// TestSigninClient implementation that intercepts the GaiaAuthConsumer and
// replaces it by a dummy one.
class DiceTestSigninClient : public TestSigninClient, public GaiaAuthConsumer {
 public:
  explicit DiceTestSigninClient(PrefService* pref_service)
      : TestSigninClient(pref_service), consumer_(nullptr) {}

  DiceTestSigninClient(const DiceTestSigninClient&) = delete;
  DiceTestSigninClient& operator=(const DiceTestSigninClient&) = delete;

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
  raw_ptr<GaiaAuthConsumer> consumer_;
};

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
class FakeRegistrationTokenHelper : public RegistrationTokenHelper {
 public:
  FakeRegistrationTokenHelper()
      : RegistrationTokenHelper(
            fake_unexportable_key_service_,
            base::BindRepeating(
                [](crypto::SignatureVerifier::SignatureAlgorithm,
                   base::span<const uint8_t>,
                   base::Time) -> absl::optional<std::string> {
                  return absl::nullopt;
                }),
            base::DoNothing()) {}

  ~FakeRegistrationTokenHelper() override = default;

  void Start() override {}

 private:
  unexportable_keys::FakeUnexportableKeyService fake_unexportable_key_service_;
};
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

class DiceResponseHandlerTest : public testing::Test,
                                public AccountReconcilor::Observer {
 public:
  // Called after the refresh token was fetched and added in the token service.
  void HandleTokenExchangeSuccess(CoreAccountId account_id,
                                  bool is_new_account) {
    token_exchange_account_id_ = account_id;
    token_exchange_is_new_account_ = is_new_account;
  }

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
                           &signin_client_),
        signin_error_controller_(
            SigninErrorController::AccountMode::PRIMARY_ACCOUNT,
            identity_test_env_.identity_manager()) {
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    feature_list_.InitWithFeatures(
        {switches::kEnableBoundSessionCredentials,
         kEnableBoundSessionCredentialsOnDiceProfiles},
        {});
#endif
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    AboutSigninInternals::RegisterPrefs(pref_service_.registry());
    auto account_reconcilor_delegate =
        std::make_unique<signin::DiceAccountReconcilorDelegate>(
            identity_manager(), &signin_client_);
    account_reconcilor_ = std::make_unique<AccountReconcilor>(
        identity_test_env_.identity_manager(), &signin_client_,
        std::move(account_reconcilor_delegate));
    account_reconcilor_->AddObserver(this);

    about_signin_internals_ = std::make_unique<AboutSigninInternals>(
        identity_test_env_.identity_manager(), &signin_error_controller_,
        signin::AccountConsistencyMethod::kDice, &signin_client_,
        account_reconcilor_.get());

    dice_response_handler_ = std::make_unique<DiceResponseHandler>(
        &signin_client_, identity_test_env_.identity_manager(),
        account_reconcilor_.get(), about_signin_internals_.get(),
        /*registration_token_helper_factory=*/
        DiceResponseHandler::RegistrationTokenHelperFactory());
  }

  ~DiceResponseHandlerTest() override {
    account_reconcilor_->RemoveObserver(this);
    account_reconcilor_->Shutdown();
    about_signin_internals_->Shutdown();
    signin_error_controller_.Shutdown();
  }

  DiceResponseParams MakeDiceParams(DiceAction action) {
    DiceResponseParams dice_params;
    dice_params.user_intention = action;
    DiceResponseParams::AccountInfo account_info =
        GetDiceResponseParamsAccountInfo(kEmail);
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

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  void EnableRegistrationTokenHelper(base::StringPiece authorization_code) {
    EXPECT_CALL(mock_registration_token_helper_factory_,
                Run(_, authorization_code, _, _))
        .WillOnce(Invoke([this](Unused, Unused, Unused, auto callback) {
          binding_registration_callback_ = std::move(callback);
          return std::make_unique<FakeRegistrationTokenHelper>();
        }));
    dice_response_handler_->SetRegistrationTokenHelperFactoryForTesting(
        mock_registration_token_helper_factory_.Get());
  }

  void SimulateRegistrationTokenHelperResult(
      absl::optional<RegistrationTokenHelper::Result> result) {
    ASSERT_FALSE(binding_registration_callback_.is_null());
    std::move(binding_registration_callback_).Run(std::move(result));
  }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

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
  std::unique_ptr<AboutSigninInternals> about_signin_internals_;
  std::unique_ptr<AccountReconcilor> account_reconcilor_;
  std::unique_ptr<DiceResponseHandler> dice_response_handler_;
  int reconcilor_blocked_count_ = 0;
  int reconcilor_unblocked_count_ = 0;
  CoreAccountId token_exchange_account_id_;
  bool token_exchange_is_new_account_ = false;
  CoreAccountId enable_sync_account_id_;
  GoogleServiceAuthError auth_error_;
  std::string auth_error_email_;
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  base::test::ScopedFeatureList feature_list_;
  StrictMock<
      base::MockCallback<DiceResponseHandler::RegistrationTokenHelperFactory>>
      mock_registration_token_helper_factory_;
  base::OnceCallback<void(absl::optional<RegistrationTokenHelper::Result>)>
      binding_registration_callback_;
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
};

class TestProcessDiceHeaderDelegate : public ProcessDiceHeaderDelegate {
 public:
  explicit TestProcessDiceHeaderDelegate(DiceResponseHandlerTest* owner)
      : owner_(owner) {}

  ~TestProcessDiceHeaderDelegate() override = default;

  // Called after the refresh token was fetched and added in the token service.
  void HandleTokenExchangeSuccess(CoreAccountId account_id,
                                  bool is_new_account) override {
    owner_->HandleTokenExchangeSuccess(account_id, is_new_account);
  }

  // Called after the refresh token was fetched and added in the token service.
  void EnableSync(const CoreAccountId& account_id) override {
    owner_->EnableSync(account_id);
  }

  void HandleTokenExchangeFailure(
      const std::string& email,
      const GoogleServiceAuthError& error) override {
    owner_->HandleTokenExchangeFailure(email, error);
  }

  signin_metrics::AccessPoint GetAccessPoint() override {
    return signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS;
  }

 private:
  raw_ptr<DiceResponseHandlerTest> owner_;
};

// Checks that a SIGNIN action triggers a token exchange request.
TEST_F(DiceResponseHandlerTest, Signin) {
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
      "refresh_token", "access_token", 10, /*is_child_account=*/false,
      /*is_under_advanced_protection=*/true, /*is_bound_to_key=*/false));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id));
  EXPECT_TRUE(auth_error_email_.empty());
  EXPECT_EQ(GoogleServiceAuthError::NONE, auth_error_.state());
  // Check HandleTokenExchangeSuccess parameters.
  EXPECT_EQ(token_exchange_account_id_, account_id);
  EXPECT_TRUE(token_exchange_is_new_account_);
  // Check that the reconcilor was blocked and unblocked exactly once.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(1, reconcilor_unblocked_count_);
  // Check that the AccountInfo::is_under_advanced_protection is set.
  AccountInfo extended_account_info =
      identity_manager()->FindExtendedAccountInfoByAccountId(account_id);
  EXPECT_TRUE(extended_account_info.is_under_advanced_protection);
  // Check that the AccessPoint was propagated from the delegate.
  EXPECT_EQ(extended_account_info.access_point,
            signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
}

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
// Checks that a SIGNIN action triggers a token exchange request.
TEST_F(DiceResponseHandlerTest, SigninWithBoundToken) {
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  const auto& account_info = dice_params.signin_info->account_info;
  CoreAccountId account_id = identity_manager()->PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id));
  EnableRegistrationTokenHelper(dice_params.signin_info->authorization_code);
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));

  // Token fetch should be blocked on the binding registration token generation.
  ASSERT_THAT(signin_client_.GetAndClearConsumer(), testing::IsNull());
  // Simulate successful token generation.
  SimulateRegistrationTokenHelperResult(RegistrationTokenHelper::Result(
      unexportable_keys::UnexportableKeyId(), std::vector<uint8_t>{1, 2, 3},
      "test_registration_token"));

  // Check that a GaiaAuthFetcher has been created.
  GaiaAuthConsumer* consumer = signin_client_.GetAndClearConsumer();
  ASSERT_THAT(consumer, testing::NotNull());
  // Simulate GaiaAuthFetcher success.
  consumer->OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
      "refresh_token", "access_token", 10, /*is_child_account=*/false,
      /*is_under_advanced_protection=*/false, /*is_bound_to_key=*/true));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id));
  EXPECT_TRUE(auth_error_email_.empty());
  EXPECT_EQ(GoogleServiceAuthError::NONE, auth_error_.state());
  // TODO(b/274463812): check that the inserted token is bound.
}

TEST_F(DiceResponseHandlerTest, SigninWithFailedBoundTokenAttempt) {
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  const auto& account_info = dice_params.signin_info->account_info;
  CoreAccountId account_id = identity_manager()->PickAccountIdForAccount(
      account_info.gaia_id, account_info.email);
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id));
  EnableRegistrationTokenHelper(dice_params.signin_info->authorization_code);
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));

  // Token fetch should be blocked on the binding registration token generation.
  ASSERT_THAT(signin_client_.GetAndClearConsumer(), testing::IsNull());
  // Simulate failed token generation.
  SimulateRegistrationTokenHelperResult(absl::nullopt);

  // Check that a GaiaAuthFetcher has been created.
  GaiaAuthConsumer* consumer = signin_client_.GetAndClearConsumer();
  ASSERT_THAT(consumer, testing::NotNull());
  // Simulate GaiaAuthFetcher success.
  consumer->OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
      "refresh_token", "access_token", 10, /*is_child_account=*/false,
      /*is_under_advanced_protection=*/false, /*is_bound_to_key=*/false));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id));
  EXPECT_TRUE(auth_error_email_.empty());
  EXPECT_EQ(GoogleServiceAuthError::NONE, auth_error_.state());
  // TODO(b/274463812): check that the inserted token is not bound.
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

// Checks that the account reconcilor is blocked when where was OAuth
// outage in Dice, and unblocked after the timeout.
TEST_F(DiceResponseHandlerTest, SupportOAuthOutageInDice) {
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  dice_params.signin_info->authorization_code.clear();
  dice_params.signin_info->no_authorization_code = true;
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that the reconcilor was blocked and not unblocked before timeout.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
  task_environment_.FastForwardBy(
      base::Hours(kLockAccountReconcilorTimeoutHours + 1));
  // Check that the reconcilor was unblocked.
  EXPECT_EQ(1, reconcilor_unblocked_count_);
  EXPECT_EQ(1, reconcilor_blocked_count_);
}

// Check that after receiving two headers with no authorization code,
// timeout still restarts.
TEST_F(DiceResponseHandlerTest, CheckTimersDuringOutageinDice) {
  ASSERT_GT(kLockAccountReconcilorTimeoutHours, 3);
  // Create params for the first header with no authorization code.
  DiceResponseParams dice_params_1 = MakeDiceParams(DiceAction::SIGNIN);
  dice_params_1.signin_info->authorization_code.clear();
  dice_params_1.signin_info->no_authorization_code = true;
  dice_response_handler_->ProcessDiceHeader(
      dice_params_1, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that the reconcilor was blocked and not unblocked before timeout.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
  // Wait half of the timeout.
  task_environment_.FastForwardBy(
      base::Hours(kLockAccountReconcilorTimeoutHours / 2));
  // Create params for the second header with no authorization code.
  DiceResponseParams dice_params_2 = MakeDiceParams(DiceAction::SIGNIN);
  dice_params_2.signin_info->authorization_code.clear();
  dice_params_2.signin_info->no_authorization_code = true;
  dice_response_handler_->ProcessDiceHeader(
      dice_params_2, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  task_environment_.FastForwardBy(
      base::Hours((kLockAccountReconcilorTimeoutHours + 1) / 2 + 1));
  // Check that the reconcilor was not unblocked after the first timeout
  // passed, timer should be restarted after getting the second header.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
  task_environment_.FastForwardBy(
      base::Hours((kLockAccountReconcilorTimeoutHours + 1) / 2));
  // Check that the reconcilor was unblocked.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(1, reconcilor_unblocked_count_);
}

// Check that signin works normally (the token is fetched and added to chrome)
// on valid headers after getting a no_authorization_code header.
TEST_F(DiceResponseHandlerTest, CheckSigninAfterOutageInDice) {
  // Create params for the header with no authorization code.
  DiceResponseParams dice_params_1 = MakeDiceParams(DiceAction::SIGNIN);
  dice_params_1.signin_info->authorization_code.clear();
  dice_params_1.signin_info->no_authorization_code = true;
  dice_response_handler_->ProcessDiceHeader(
      dice_params_1, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Create params for the valid header with an authorization code.
  DiceResponseParams dice_params_2 = MakeDiceParams(DiceAction::SIGNIN);
  const auto& account_info_2 = dice_params_2.signin_info->account_info;
  CoreAccountId account_id_2 = identity_manager()->PickAccountIdForAccount(
      account_info_2.gaia_id, account_info_2.email);
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id_2));
  dice_response_handler_->ProcessDiceHeader(
      dice_params_2, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that the reconcilor was blocked and not unblocked before timeout.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
  // Check that a GaiaAuthFetcher has been created.
  GaiaAuthConsumer* consumer = signin_client_.GetAndClearConsumer();
  ASSERT_THAT(consumer, testing::NotNull());
  // Simulate GaiaAuthFetcher success.
  consumer->OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
      "refresh_token", "access_token", 10, /*is_child_account=*/false,
      /*is_under_advanced_protection=*/true, /*is_bound_to_key=*/false));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id_2));
  EXPECT_TRUE(auth_error_email_.empty());
  EXPECT_EQ(GoogleServiceAuthError::NONE, auth_error_.state());
  // Check HandleTokenExchangeSuccess parameters.
  EXPECT_EQ(token_exchange_account_id_, account_id_2);
  EXPECT_TRUE(token_exchange_is_new_account_);
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
  // Check that the AccountInfo::is_under_advanced_protection is set.
  EXPECT_TRUE(identity_manager()
                  ->FindExtendedAccountInfoByAccountId(account_id_2)
                  .is_under_advanced_protection);
  task_environment_.FastForwardBy(
      base::Hours(kLockAccountReconcilorTimeoutHours + 1));
  // Check that the reconcilor was unblocked.
  EXPECT_EQ(1, reconcilor_unblocked_count_);
  EXPECT_EQ(1, reconcilor_blocked_count_);
}

// Checks that a SIGNIN action triggers a token exchange request when the
// account is in authentication error.
TEST_F(DiceResponseHandlerTest, Reauth) {
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      dice_params.signin_info->account_info.email, signin::ConsentLevel::kSync);
  dice_params.signin_info->account_info.gaia_id = account_info.gaia;
  CoreAccountId account_id = account_info.account_id;
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Check that a GaiaAuthFetcher has been created.
  GaiaAuthConsumer* consumer = signin_client_.GetAndClearConsumer();
  ASSERT_THAT(consumer, testing::NotNull());
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
  // Simulate GaiaAuthFetcher success.
  consumer->OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
      "refresh_token", "access_token", 10, /*is_child_account=*/false,
      /*is_under_advanced_protection=*/true, /*is_bound_to_key=*/false));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id));
  // Check HandleTokenExchangeSuccess parameters.
  EXPECT_EQ(token_exchange_account_id_, account_id);
  EXPECT_FALSE(token_exchange_is_new_account_);
}

// Checks that a GaiaAuthFetcher failure is handled correctly.
TEST_F(DiceResponseHandlerTest, SigninFailure) {
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
      "refresh_token", "access_token", 10, /*is_child_account=*/false,
      /*is_under_advanced_protection=*/false, /*is_bound_to_key=*/false));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id));
  EXPECT_FALSE(identity_manager()
                   ->FindExtendedAccountInfoByAccountId(account_id)
                   .is_under_advanced_protection);
}

// Checks that two SIGNIN requests can happen concurrently.
TEST_F(DiceResponseHandlerTest, SigninWithTwoAccounts) {
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
      "refresh_token", "access_token", 10, /*is_child_account=*/false,
      /*is_under_advanced_protection=*/true, /*is_bound_to_key=*/false));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id_1));
  EXPECT_TRUE(identity_manager()
                  ->FindExtendedAccountInfoByAccountId(account_id_1)
                  .is_under_advanced_protection);
  // Simulate GaiaAuthFetcher success for the second request.
  consumer_2->OnClientOAuthSuccess(GaiaAuthConsumer::ClientOAuthResult(
      "refresh_token", "access_token", 10, /*is_child_account=*/false,
      /*is_under_advanced_protection=*/false, /*is_bound_to_key=*/false));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id_2));
  EXPECT_FALSE(identity_manager()
                   ->FindExtendedAccountInfoByAccountId(account_id_2)
                   .is_under_advanced_protection);
  // Check that the reconcilor was blocked and unblocked exactly once.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(1, reconcilor_unblocked_count_);
}

// Checks that a ENABLE_SYNC action received after the refresh token is added
// to the token service, triggers a call to enable sync on the delegate.
TEST_F(DiceResponseHandlerTest, SigninEnableSyncAfterRefreshTokenFetched) {
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
      "refresh_token", "access_token", 10, /*is_child_account=*/false,
      /*is_under_advanced_protection=*/false, /*is_bound_to_key=*/false));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id));
  // Check HandleTokenExchangeSuccess parameters.
  EXPECT_EQ(token_exchange_account_id_, account_id);
  EXPECT_TRUE(token_exchange_is_new_account_);
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
      "refresh_token", "access_token", 10, /*is_child_account=*/false,
      /*is_under_advanced_protection=*/false, /*is_bound_to_key=*/false));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id));
  // Check HandleTokenExchangeSuccess parameters.
  EXPECT_EQ(token_exchange_account_id_, account_id);
  EXPECT_TRUE(token_exchange_is_new_account_);
  // Check that delegate was called to enable sync.
  EXPECT_EQ(account_id, enable_sync_account_id_);
}

TEST_F(DiceResponseHandlerTest, Timeout) {
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
      base::Seconds(kDiceTokenFetchTimeoutSeconds + 1));
  EXPECT_EQ(
      0u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Check that the token has not been inserted in the token service.
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id));
  // Check that the reconcilor was blocked and unblocked exactly once.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(1, reconcilor_unblocked_count_);
}

// Checks that there is no crash if the DiceResponseHandler is deleted before
// the timeout expires. Tests the scenario from https://crbug.com/1290214
TEST_F(DiceResponseHandlerTest, DeleteBeforeTimeout) {
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

  // Delete the handler.
  dice_response_handler_.reset();

  // Force a timeout, this should not crash.
  task_environment_.FastForwardBy(
      base::Seconds(kDiceTokenFetchTimeoutSeconds + 1));

  // Check that the token has not been inserted in the token service.
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id));
  // Check that the reconcilor was blocked and unblocked exactly once.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(1, reconcilor_unblocked_count_);
}

TEST_F(DiceResponseHandlerTest, SignoutMainAccount) {
  const char kSecondaryEmail[] = "other@gmail.com";
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  const auto& dice_account_info = dice_params.signout_info->account_infos[0];
  // User is signed in to Chrome, and has some refresh token for a secondary
  // account.
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      dice_account_info.email, signin::ConsentLevel::kSync);
  AccountInfo secondary_account_info =
      identity_test_env_.MakeAccountAvailable(kSecondaryEmail);
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      secondary_account_info.account_id));
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
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

  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  // Check that the reconcilor was not blocked.
  EXPECT_EQ(0, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
}

TEST_F(DiceResponseHandlerTest, SignoutSecondaryAccount) {
  const char kMainEmail[] = "main@gmail.com";
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  const auto& secondary_dice_account_info =
      dice_params.signout_info->account_infos[0];
  // User is signed in to Chrome, and has some refresh token for a secondary
  // account.
  AccountInfo main_account_info =
      identity_test_env_.MakePrimaryAccountAvailable(
          kMainEmail, signin::ConsentLevel::kSync);
  AccountInfo secondary_account_info = identity_test_env_.MakeAccountAvailable(
      secondary_dice_account_info.email);
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      secondary_account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      main_account_info.account_id));
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  // Receive signout response for the secondary account.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));

  // Only the token corresponding the the Dice parameter has been removed, and
  // the user is still signed in.
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(
      secondary_account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      main_account_info.account_id));
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
}

TEST_F(DiceResponseHandlerTest, SignoutWebOnly) {
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
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  // Receive signout response.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // Only the token corresponding the the Dice parameter has been removed.
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      secondary_account_info.account_id));
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
}

// Checks that signin in progress is canceled by a signout.
TEST_F(DiceResponseHandlerTest, SigninSignoutSameAccount) {
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  const auto& dice_account_info = dice_params.signout_info->account_infos[0];

  // User is signed in to Chrome.
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      dice_account_info.email, signin::ConsentLevel::kSync);
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
      "refresh_token", "access_token", 10, /*is_child_account=*/false,
      /*is_under_advanced_protection=*/false, /*is_bound_to_key=*/false));
  EXPECT_EQ(
      0u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Check that the right token is available.
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id_1));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id_2));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id_2));
}

TEST_F(DiceResponseHandlerTest,
       SignoutMainNonSyncAccountWithSignoutRestrictions) {
  signin_client_.set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);
  const char kSecondaryEmail[] = "other@gmail.com";
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  dice_params.signout_info->account_infos.push_back(
      GetDiceResponseParamsAccountInfo(kSecondaryEmail));
  const auto& dice_account_info = dice_params.signout_info->account_infos[0];
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      dice_account_info.email, signin::ConsentLevel::kSignin);
  AccountInfo secondary_account_info =
      identity_test_env_.MakeAccountAvailable(kSecondaryEmail);
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      secondary_account_info.account_id));
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  // Receive signout response.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, std::make_unique<TestProcessDiceHeaderDelegate>(this));
  // User is not signed out, token for the main account is now invalid.
  // Secondary account removed.
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
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(
      secondary_account_info.account_id));

  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  // Check that the reconcilor was not blocked.
  EXPECT_EQ(0, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
}

// Tests that the DiceResponseHandler is created for a normal profile but not
// for off-the-record profiles.
TEST(DiceResponseHandlerFactoryTest, NotInOffTheRecord) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  EXPECT_THAT(DiceResponseHandler::GetForProfile(&profile), testing::NotNull());
  EXPECT_THAT(DiceResponseHandler::GetForProfile(
                  profile.GetPrimaryOTRProfile(/*create_if_needed=*/true)),
              testing::IsNull());
  EXPECT_THAT(DiceResponseHandler::GetForProfile(profile.GetOffTheRecordProfile(
                  Profile::OTRProfileID::CreateUniqueForTesting(),
                  /*create_if_needed=*/true)),
              testing::IsNull());
}

}  // namespace
