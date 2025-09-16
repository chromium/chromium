// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/check_deref.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "components/signin/core/browser/test_account_reconcilor_observer.h"
#include "components/signin/public/base/session_binding_test_utils.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "content/public/test/browser_test.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "google_apis/gaia/bound_oauth_token.pb.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::Values;

constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kAcceptableAlgorithms[] = {crypto::SignatureVerifier::ECDSA_SHA256};
constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kUserBlocking;

class BoundSessionOAuthMultiloginTest : public MixinBasedInProcessBrowserTest {
 public:
  BoundSessionOAuthMultiloginTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{switches::kEnableBoundSessionCredentials,
                              switches::kEnableChromeRefreshTokenBinding,
                              switches::kEnableOAuthMultiloginCookiesBinding},
        /*disabled_features=*/{});
  }

  ~BoundSessionOAuthMultiloginTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    // This is needed to properly resolve `accounts.google.com` to fake Gaia
    // server.
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        "MAP accounts.google.com " +
            fake_gaia_mixin().gaia_server()->host_port_pair().ToString());
  }

  void SetUpOnMainThread() override {
    fake_gaia_mixin().set_initialize_configuration(false);
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  unexportable_keys::UnexportableKeyService& unexportable_key_service() {
    return CHECK_DEREF(
        UnexportableKeyServiceFactory::GetForProfile(browser()->profile()));
  }

  FakeGaia& fake_gaia() { return CHECK_DEREF(fake_gaia_.fake_gaia()); }

  FakeGaiaMixin& fake_gaia_mixin() { return fake_gaia_; }

  signin::IdentityManager& identity_manager() {
    return CHECK_DEREF(
        IdentityManagerFactory::GetForProfile(browser()->profile()));
  }

  BoundSessionCookieRefreshService& bound_session_cookie_refresh_service() {
    return CHECK_DEREF(BoundSessionCookieRefreshServiceFactory::GetForProfile(
        browser()->profile()));
  }

  unexportable_keys::UnexportableKeyId GenerateNewKey() {
    base::test::TestFuture<
        unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
        future;
    unexportable_key_service().GenerateSigningKeySlowlyAsync(
        kAcceptableAlgorithms, kTaskPriority, future.GetCallback());
    const unexportable_keys::ServiceErrorOr<
        unexportable_keys::UnexportableKeyId>
        key_id = future.Get();
    CHECK(key_id.has_value());
    return *key_id;
  }

  std::vector<uint8_t> GetWrappedKey(
      std::optional<unexportable_keys::UnexportableKeyId> key_id =
          std::nullopt) {
    if (!key_id.has_value()) {
      key_id = GenerateNewKey();
    }
    const unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> wrapped_key =
        unexportable_key_service().GetWrappedKey(*key_id);
    CHECK(wrapped_key.has_value());
    return *wrapped_key;
  }

  void SetBoundSessionParamsUpdatedCallback(base::RepeatingClosure callback) {
    bound_session_cookie_refresh_service()
        .SetBoundSessionParamsUpdatedCallbackForTesting(std::move(callback));
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  crypto::ScopedFakeUnexportableKeyProvider scoped_key_provider_;
  FakeGaiaMixin fake_gaia_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(BoundSessionOAuthMultiloginTest, StartsNewBoundSession) {
  const unexportable_keys::UnexportableKeyId key_id = GenerateNewKey();
  const std::vector<uint8_t> wrapped_key = GetWrappedKey(key_id);
  signin::MakeAccountAvailable(
      &identity_manager(),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithGaiaId(FakeGaiaMixin::kFakeUserGaiaId)
          .WithRefreshToken(FakeGaiaMixin::kFakeRefreshToken)
          .WithRefreshTokenBindingKey(wrapped_key)
          .Build(FakeGaiaMixin::kFakeUserEmail));

  ASSERT_TRUE(
      identity_manager().HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_EQ(identity_manager().GetWrappedBindingKey(), wrapped_key);
  ASSERT_THAT(
      bound_session_cookie_refresh_service().GetBoundSessionThrottlerParams(),
      IsEmpty());

  // This makes sure that eventually OAML will return bound cookies, at the same
  // time `/ListAccounts` WON'T return primary account triggering OAML - it
  // simulates similar scenario to cookies being cleared.
  fake_gaia_mixin().SetupFakeGaiaForLoginWithDefaults();
  FakeGaia::Configuration config;
  config.session_sid_cookie = "fake_sid";
  config.session_lsid_cookie = "fake_lsid";
  config.session_1p_sidts_cookie = "fake_1p_sidts";
  config.session_3p_sidts_cookie = "fake_3p_sidts";
  fake_gaia().SetConfiguration(config);

  base::RunLoop run_loop;
  SetBoundSessionParamsUpdatedCallback(run_loop.QuitClosure());

  // Enforce initial `/ListAccounts`.
  signin::SetFreshnessOfAccountsInGaiaCookie(&identity_manager(),
                                             /*accounts_are_fresh=*/false);

  run_loop.Run();

  base::queue<FakeGaia::MultiloginCall> multilogin_calls =
      fake_gaia().GetAndResetMultiloginCalls();

  ASSERT_THAT(multilogin_calls, SizeIs(2));

  const auto& first_call = multilogin_calls.front();
  // OAML first returns the challenge to sign.
  ASSERT_EQ(first_call.action,
            FakeGaia::MultiloginCall::Action::kReturnBindingChallenge);
  multilogin_calls.pop();

  // After the challenge is received and signed, OAML returns the bound cookies.
  const auto& second_call = multilogin_calls.front();
  ASSERT_EQ(second_call.action,
            FakeGaia::MultiloginCall::Action::kReturnBoundCookies);

  // Verify that the challenge was signed correctly.
  const std::optional<gaia::MultiOAuthHeader> header = second_call.header;
  ASSERT_TRUE(header.has_value());
  ASSERT_THAT(header->account_requests(), SizeIs(1));
  EXPECT_TRUE(signin::VerifyJwtSignature(
      header->account_requests().at(0).token_binding_assertion(),
      *unexportable_key_service().GetAlgorithm(key_id),
      *unexportable_key_service().GetSubjectPublicKeyInfo(key_id)));

  // Verify that the new bound session started.
  EXPECT_THAT(
      bound_session_cookie_refresh_service().GetBoundSessionThrottlerParams(),
      SizeIs(1));
}

class BoundSessionOAuthMultiloginPersistentErrorTest
    : public BoundSessionOAuthMultiloginTest,
      public testing::WithParamInterface<OAuthMultiloginResponseStatus> {};

IN_PROC_BROWSER_TEST_P(BoundSessionOAuthMultiloginPersistentErrorTest,
                       RefreshTokensBoundToDifferentKeys) {
  const std::string email_1 = "user1@gmail.com";
  const GaiaId::Literal fake_gaia_id_1("fake-gaia-id-1");
  const std::string refresh_token_1 = "refresh-token-1";
  const CoreAccountInfo account_info_1 = signin::MakeAccountAvailable(
      &identity_manager(), signin::AccountAvailabilityOptionsBuilder()
                               .AsPrimary(signin::ConsentLevel::kSignin)
                               .WithGaiaId(fake_gaia_id_1)
                               .WithRefreshToken(refresh_token_1)
                               .WithRefreshTokenBindingKey(GetWrappedKey())
                               .Build(email_1));
  ASSERT_EQ(
      identity_manager().GetPrimaryAccountInfo(signin::ConsentLevel::kSignin),
      account_info_1);
  ASSERT_TRUE(identity_manager().HasAccountWithBoundRefreshToken(
      account_info_1.account_id));

  const std::string email_2 = "user2@gmail.com";
  const GaiaId::Literal fake_gaia_id_2("fake-gaia-id-2");
  const std::string refresh_token_2 = "refresh-token-2";
  const CoreAccountInfo account_info_2 = signin::MakeAccountAvailable(
      &identity_manager(), signin::AccountAvailabilityOptionsBuilder()
                               .WithGaiaId(fake_gaia_id_2)
                               .WithRefreshToken(refresh_token_2)
                               .WithRefreshTokenBindingKey(GetWrappedKey())
                               .Build(email_2));
  ASSERT_TRUE(identity_manager().HasAccountWithBoundRefreshToken(
      account_info_2.account_id));

  ASSERT_FALSE(identity_manager().AllBoundTokensShareSameBindingKey());

  fake_gaia_mixin().SetupFakeGaiaForLogin(email_1, fake_gaia_id_1,
                                          refresh_token_1);
  ASSERT_TRUE(fake_gaia().HasAccessTokenForAuthToken(refresh_token_1));
  fake_gaia_mixin().SetupFakeGaiaForLogin(email_2, fake_gaia_id_2,
                                          refresh_token_2);
  ASSERT_TRUE(fake_gaia().HasAccessTokenForAuthToken(refresh_token_2));

  // This makes sure that OAML will return `INVALID_INPUT` error. At the same
  // time, `/ListAccounts` WON'T return accounts, which will trigger OAML -
  // it simulates similar scenario to cookies being cleared.
  FakeGaia::Configuration config;
  config.oauth_multilogin_response_status = GetParam();
  fake_gaia().SetConfiguration(config);

  signin::TestIdentityManagerObserver identity_manager_observer(
      &identity_manager());

  TestAccountReconcilorObserver account_reconcilor_observer(
      AccountReconcilorFactory::GetForProfile(browser()->profile()),
      /*wait_state=*/signin_metrics::AccountReconcilorState::kOk);

  // Enforce initial `/ListAccounts`.
  signin::SetFreshnessOfAccountsInGaiaCookie(&identity_manager(),
                                             /*accounts_are_fresh=*/false);

  // Wait until the reconcilor recovers and reaches the
  // `signin_metrics::AccountReconcilorState::kOk` state.
  account_reconcilor_observer.WaitForStateChange();

  // Secondary account(s) are removed.
  EXPECT_FALSE(
      identity_manager().HasAccountWithRefreshToken(account_info_2.account_id));
  // The primary account is put in the error state.
  EXPECT_TRUE(
      identity_manager().HasAccountWithRefreshTokenInPersistentErrorState(
          account_info_1.account_id));
  EXPECT_EQ(
      identity_manager_observer
          .TokenOperationSourceFromErrorStateOfRefreshTokenUpdatedCallback(),
      signin_metrics::SourceForRefreshTokenOperation::
          kDiceAccountReconcilorDelegate_RefreshTokensBoundToDifferentKeys);
  // Both refresh tokens are revoked.
  EXPECT_FALSE(fake_gaia().HasAccessTokenForAuthToken(refresh_token_1));
  EXPECT_FALSE(fake_gaia().HasAccessTokenForAuthToken(refresh_token_2));
}

IN_PROC_BROWSER_TEST_P(BoundSessionOAuthMultiloginPersistentErrorTest,
                       RefreshTokensBoundToSameKey) {
  const std::vector<uint8_t> wrapped_key = GetWrappedKey();

  const std::string email_1 = "user1@gmail.com";
  const GaiaId::Literal fake_gaia_id_1("fake-gaia-id-1");
  const std::string refresh_token_1 = "refresh-token-1";
  const CoreAccountInfo account_info_1 = signin::MakeAccountAvailable(
      &identity_manager(), signin::AccountAvailabilityOptionsBuilder()
                               .AsPrimary(signin::ConsentLevel::kSignin)
                               .WithGaiaId(fake_gaia_id_1)
                               .WithRefreshToken(refresh_token_1)
                               .WithRefreshTokenBindingKey(wrapped_key)
                               .Build(email_1));
  ASSERT_EQ(
      identity_manager().GetPrimaryAccountInfo(signin::ConsentLevel::kSignin),
      account_info_1);
  ASSERT_TRUE(identity_manager().HasAccountWithBoundRefreshToken(
      account_info_1.account_id));

  const std::string email_2 = "user2@gmail.com";
  const GaiaId::Literal fake_gaia_id_2("fake-gaia-id-2");
  const std::string refresh_token_2 = "refresh-token-2";
  const CoreAccountInfo account_info_2 = signin::MakeAccountAvailable(
      &identity_manager(), signin::AccountAvailabilityOptionsBuilder()
                               .WithGaiaId(fake_gaia_id_2)
                               .WithRefreshToken(refresh_token_2)
                               .WithRefreshTokenBindingKey(wrapped_key)
                               .Build(email_2));
  ASSERT_TRUE(identity_manager().HasAccountWithBoundRefreshToken(
      account_info_2.account_id));

  ASSERT_TRUE(identity_manager().AllBoundTokensShareSameBindingKey());

  fake_gaia_mixin().SetupFakeGaiaForLogin(email_1, fake_gaia_id_1,
                                          refresh_token_1);
  ASSERT_TRUE(fake_gaia().HasAccessTokenForAuthToken(refresh_token_1));
  fake_gaia_mixin().SetupFakeGaiaForLogin(email_2, fake_gaia_id_2,
                                          refresh_token_2);
  ASSERT_TRUE(fake_gaia().HasAccessTokenForAuthToken(refresh_token_2));

  // This makes sure that OAML will return `INVALID_INPUT` error. At the same
  // time, `/ListAccounts` WON'T return accounts, which will trigger OAML -
  // it simulates similar scenario to cookies being cleared.
  FakeGaia::Configuration config;
  config.oauth_multilogin_response_status = GetParam();
  fake_gaia().SetConfiguration(config);

  TestAccountReconcilorObserver observer(
      AccountReconcilorFactory::GetForProfile(browser()->profile()),
      /*wait_state=*/signin_metrics::AccountReconcilorState::kError);

  // Enforce initial `/ListAccounts`.
  signin::SetFreshnessOfAccountsInGaiaCookie(&identity_manager(),
                                             /*accounts_are_fresh=*/false);

  // Wait until the reconcilor reaches the
  // `signin_metrics::AccountReconcilorState::kError` state (it can't recover
  // without a user action).
  observer.WaitForStateChange();

  // Secondary account(s) are NOT removed.
  EXPECT_TRUE(identity_manager().HasAccountWithBoundRefreshToken(
      account_info_2.account_id));
  // The primary account is NOT put in the error state.
  EXPECT_TRUE(identity_manager().HasAccountWithBoundRefreshToken(
      account_info_1.account_id));
  EXPECT_FALSE(
      identity_manager().HasAccountWithRefreshTokenInPersistentErrorState(
          account_info_1.account_id));
  // None of the refresh tokens is revoked.
  EXPECT_TRUE(fake_gaia().HasAccessTokenForAuthToken(refresh_token_1));
  EXPECT_TRUE(fake_gaia().HasAccessTokenForAuthToken(refresh_token_2));
}

INSTANTIATE_TEST_SUITE_P(,
                         BoundSessionOAuthMultiloginPersistentErrorTest,
                         Values(OAuthMultiloginResponseStatus::kInvalidInput,
                                OAuthMultiloginResponseStatus::kError));
