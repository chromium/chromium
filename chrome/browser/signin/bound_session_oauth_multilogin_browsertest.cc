// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/check_deref.h"
#include "base/no_destructor.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_impl.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_provider_config.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/renderer_configuration.mojom-forward.h"
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
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/session_params.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/device_bound_sessions.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;
using ::testing::Values;

constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kAcceptableAlgorithms[] = {crypto::SignatureVerifier::ECDSA_SHA256};
constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kUserBlocking;

constexpr std::string_view k1PSIDTSCookieName = "__Secure-1PSIDTS";
constexpr std::string_view k3PSIDTSCookieName = "__Secure-3PSIDTS";

const std::vector<base::test::FeatureRef>& GetPrototypeFeatures() {
  static const base::NoDestructor<std::vector<base::test::FeatureRef>>
      kPrototypeFeatures({switches::kEnableBoundSessionCredentials,
                          switches::kEnableOAuthMultiloginCookiesBinding});
  return *kPrototypeFeatures;
}

const std::vector<base::test::FeatureRef>& GetStandardFeatures() {
  static const base::NoDestructor<std::vector<base::test::FeatureRef>>
      kStandardFeatures(
          {net::features::kDeviceBoundSessions,
           switches::kEnableOAuthMultiloginStandardCookiesBinding,
           // This is required to ensure both the browser and the network
           // processes share the same underlying crypto implementation.
           // Otherwise, mocking `crypto::GetUnexportableKeyProvider` in the
           // tests (e.g. `crypto::ScopedFakeUnexportableKeyProvider`) would
           // not have any effect in the network process due to process
           // isolation.
           network::features::kUseUnexportableKeyServiceInBrowserProcess});
  return *kStandardFeatures;
}

net::device_bound_sessions::SessionParams CreateTestSessionParams() {
  GURL url("https://google.com/");
  net::device_bound_sessions::SessionParams::Scope scope;
  scope.include_site = true;
  scope.origin = url::Origin::Create(url).Serialize();
  net::device_bound_sessions::SessionParams params(
      /*id=*/"sidts_session", url,
      /*refresh_url=*/"/RotateBoundCookies", std::move(scope),
      /*creds=*/
      {net::device_bound_sessions::SessionParams::Credential{
          .name = "__Secure-1PSIDTS",
          .attributes = "Secure; HttpOnly; Domain=.google.com; "
                        "Path=/; SameSite=None"}},
      unexportable_keys::UnexportableKeyId(),
      /*allowed_refresh_initiators=*/{"*"});
  return params;
}

class DeviceBoundSessionAccessObserver
    : public network::mojom::DeviceBoundSessionAccessObserver {
 public:
  explicit DeviceBoundSessionAccessObserver(
      network::mojom::DeviceBoundSessionManager& device_bound_session_manager,
      base::RepeatingCallback<void(
          const net::device_bound_sessions::SessionAccess&)> on_access_callback)
      : on_access_callback_(std::move(on_access_callback)) {
    device_bound_session_manager.AddObserver(
        GURL(GaiaUrls::GetInstance()->gaia_url()),
        receiver_.BindNewPipeAndPassRemote());
  }

  // network::mojom::DeviceBoundSessionAccessObserver:
  void OnDeviceBoundSessionAccessed(
      const net::device_bound_sessions::SessionAccess& access) override {
    on_access_callback_.Run(access);
  }

  void Clone(
      mojo::PendingReceiver<network::mojom::DeviceBoundSessionAccessObserver>
          observer) override {
    NOTREACHED();
  }

 private:
  base::RepeatingCallback<void(
      const net::device_bound_sessions::SessionAccess&)>
      on_access_callback_;
  mojo::Receiver<network::mojom::DeviceBoundSessionAccessObserver> receiver_{
      this};
};

bound_session_credentials::Credential CreateCookieCredential(
    std::string_view name) {
  bound_session_credentials::Credential credential;
  bound_session_credentials::CookieCredential* cookie =
      credential.mutable_cookie_credential();
  cookie->set_name(name);
  cookie->set_domain(".google.com");
  cookie->set_path("/");
  return credential;
}

bound_session_credentials::BoundSessionParams CreateSIDTSBoundSessionParams(
    std::string_view wrapped_key) {
  bound_session_credentials::BoundSessionParams params;
  params.set_site("https://google.com/");
  params.set_session_id("sidts_session");
  params.set_wrapped_key(wrapped_key);
  *params.add_credentials() = CreateCookieCredential(k1PSIDTSCookieName);
  *params.add_credentials() = CreateCookieCredential(k3PSIDTSCookieName);
  params.set_refresh_url(
      GaiaUrls::GetInstance()->rotate_bound_cookies_url().spec());
  return params;
}

}  // namespace

class BoundSessionOAuthMultiloginBaseTest
    : public MixinBasedInProcessBrowserTest {
 public:
  // Enables `enabled_features` and disables `disabled_features`.
  //
  // `switches::kEnableChromeRefreshTokenBinding` is enabled by default for all
  // tests being a prerequisite for any (prototype and standard) OAML cookie
  // binding.
  BoundSessionOAuthMultiloginBaseTest(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features) {
    std::vector<base::test::FeatureRef> all_enabled_features = enabled_features;
    all_enabled_features.push_back(switches::kEnableChromeRefreshTokenBinding);
    feature_list_.InitWithFeatures(all_enabled_features, disabled_features);
  }

  ~BoundSessionOAuthMultiloginBaseTest() override = default;

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

  void TearDownOnMainThread() override {
    test_identity_manager_observer_.reset();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  unexportable_keys::UnexportableKeyService& unexportable_key_service() {
    return CHECK_DEREF(UnexportableKeyServiceFactory::GetForProfileAndPurpose(
        browser()->profile(),
        unexportable_keys::KeyPurpose::kRefreshTokenBinding));
  }

  FakeGaia& fake_gaia() { return CHECK_DEREF(fake_gaia_.fake_gaia()); }

  FakeGaiaMixin& fake_gaia_mixin() { return fake_gaia_; }

  signin::IdentityManager& identity_manager() {
    return CHECK_DEREF(
        IdentityManagerFactory::GetForProfile(browser()->profile()));
  }

  network::mojom::DeviceBoundSessionManager& device_bound_session_manager() {
    return CHECK_DEREF(ChromeSigninClientFactory::GetForProfile(GetProfile())
                           ->GetDeviceBoundSessionManager());
  }

  BoundSessionCookieRefreshService& bound_session_cookie_refresh_service() {
    return CHECK_DEREF(BoundSessionCookieRefreshServiceFactory::GetForProfile(
        browser()->profile()));
  }

  void SetBoundSessionParamsUpdatedCallback(base::RepeatingClosure callback) {
    bound_session_cookie_refresh_service()
        .SetBoundSessionParamsUpdatedCallbackForTesting(std::move(callback));
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

  void UpdateFakeGaiaConfigOnSetOnAccountsInCookieUpdated(
      const FakeGaia::Configuration& config) {
    if (!test_identity_manager_observer_) {
      test_identity_manager_observer_ =
          std::make_unique<signin::TestIdentityManagerObserver>(
              &identity_manager());
    }
    test_identity_manager_observer_->SetOnAccountsInCookieUpdatedCallback(
        base::BindLambdaForTesting([&, config_updated = false]() mutable {
          if (config_updated) {
            return;
          }
          fake_gaia().SetConfiguration(config);
          config_updated = true;
        }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  crypto::ScopedFakeUnexportableKeyProvider scoped_key_provider_;
  FakeGaiaMixin fake_gaia_{&mixin_host_};

  std::unique_ptr<signin::TestIdentityManagerObserver>
      test_identity_manager_observer_;
};

class BoundSessionOAuthMultiloginPrototypeTest
    : public BoundSessionOAuthMultiloginBaseTest {
 public:
  BoundSessionOAuthMultiloginPrototypeTest()
      : BoundSessionOAuthMultiloginBaseTest(GetPrototypeFeatures(),
                                            GetStandardFeatures()) {}
};

IN_PROC_BROWSER_TEST_F(BoundSessionOAuthMultiloginPrototypeTest,
                       ReuseExistingSession) {
  base::HistogramTester histogram_tester;

  const unexportable_keys::UnexportableKeyId key_id = GenerateNewKey();
  const std::vector<uint8_t> wrapped_key = GetWrappedKey(key_id);

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

  bound_session_cookie_refresh_service().RegisterNewBoundSession(
      CreateSIDTSBoundSessionParams(
          std::string(wrapped_key.begin(), wrapped_key.end())));
  ASSERT_THAT(
      bound_session_cookie_refresh_service().GetBoundSessionThrottlerParams(),
      SizeIs(1));

  // Configure `FakeGaia`:
  // - make `ListAccounts` return only the primary account making Chrome to
  // trigger OAML,
  // - make OAML return reuse bound session response,
  // - make `RotateBoundCookies` return success (to ensure the session is not
  // terminated).
  fake_gaia_mixin().SetupFakeGaiaForLogin(email_1, fake_gaia_id_1,
                                          refresh_token_1);
  fake_gaia_mixin().SetupFakeGaiaForLogin(email_2, fake_gaia_id_2,
                                          refresh_token_2);
  FakeGaia::Configuration config;
  config.session_sid_cookie = "fake_sid";
  config.session_lsid_cookie = "fake_lsid";
  config.session_1p_sidts_cookie = "fake_1p_sidts";
  config.session_3p_sidts_cookie = "fake_3p_sidts";
  config.emails = {email_1};
  config.reuse_bound_session = true;
  config.rotated_cookies = {std::string(k1PSIDTSCookieName),
                            std::string(k3PSIDTSCookieName)};
  fake_gaia().SetConfiguration(config);

  // Make sure that subsequent `/ListAccounts` returns both accounts.
  config.emails = {email_1, email_2};
  UpdateFakeGaiaConfigOnSetOnAccountsInCookieUpdated(config);

  TestAccountReconcilorObserver account_reconcilor_observer(
      AccountReconcilorFactory::GetForProfile(browser()->profile()),
      /*wait_state=*/signin_metrics::AccountReconcilorState::kOk);

  // Enforce initial `/ListAccounts`.
  signin::SetFreshnessOfAccountsInGaiaCookie(&identity_manager(),
                                             /*accounts_are_fresh=*/false);

  // Wait until the reconcilor reaches the
  // `signin_metrics::AccountReconcilorState::kOk` state.
  account_reconcilor_observer.WaitForStateChange();

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

  // Verify that the bound session is present and it hasn't been
  // terminated.
  EXPECT_THAT(
      bound_session_cookie_refresh_service().GetBoundSessionThrottlerParams(),
      SizeIs(1));
  histogram_tester.ExpectTotalCount(
      "Signin.BoundSessionCredentials.SessionTerminationTrigger",
      /*expected_count=*/0);
}

class BoundSessionOAuthMultiloginPrototypeNewSessionTest
    : public BoundSessionOAuthMultiloginPrototypeTest,
      public testing::WithParamInterface<bool> {};

IN_PROC_BROWSER_TEST_P(BoundSessionOAuthMultiloginPrototypeNewSessionTest,
                       StartsNewBoundSession) {
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
  config.spec_compliant_device_bound_session = GetParam();
  config.session_sid_cookie = "fake_sid";
  config.session_lsid_cookie = "fake_lsid";
  config.session_1p_sidts_cookie = "fake_1p_sidts";
  config.session_3p_sidts_cookie = "fake_3p_sidts";
  fake_gaia().SetConfiguration(config);

  // Make sure that subsequent `/ListAccounts` returns the missing primary
  // account, this prevents triggering OAML indefinitely.
  config.emails = {FakeGaiaMixin::kFakeUserEmail};
  UpdateFakeGaiaConfigOnSetOnAccountsInCookieUpdated(config);

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

IN_PROC_BROWSER_TEST_P(BoundSessionOAuthMultiloginPrototypeNewSessionTest,
                       DoesNotStartYoutubeSession) {
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
  config.spec_compliant_device_bound_session = GetParam();
  config.session_sid_cookie = "fake_sid";
  config.session_lsid_cookie = "fake_lsid";
  config.session_1p_sidts_cookie = "fake_1p_sidts";
  config.session_3p_sidts_cookie = "fake_3p_sidts";
  config.include_youtube_bound_session = true;
  fake_gaia().SetConfiguration(config);

  // Make sure that subsequent `/ListAccounts` returns the missing primary
  // account, this prevents triggering OAML indefinitely.
  config.emails = {FakeGaiaMixin::kFakeUserEmail};
  UpdateFakeGaiaConfigOnSetOnAccountsInCookieUpdated(config);

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

  // Verify that only google.com bound session started.
  EXPECT_THAT(
      bound_session_cookie_refresh_service().GetBoundSessionThrottlerParams(),
      ElementsAre(AllOf(
          Pointee(Field(&chrome::mojom::BoundSessionThrottlerParams::domain,
                        "google.com")))));
}

IN_PROC_BROWSER_TEST_P(BoundSessionOAuthMultiloginPrototypeNewSessionTest,
                       OverrideExistingSession) {
  base::HistogramTester histogram_tester;

  const unexportable_keys::UnexportableKeyId key_id = GenerateNewKey();
  const std::vector<uint8_t> wrapped_key = GetWrappedKey(key_id);

  const std::string email = "user1@gmail.com";
  const GaiaId::Literal fake_gaia_id("fake-gaia-id-1");
  const std::string refresh_token = "refresh-token-1";
  const CoreAccountInfo account_info = signin::MakeAccountAvailable(
      &identity_manager(), signin::AccountAvailabilityOptionsBuilder()
                               .AsPrimary(signin::ConsentLevel::kSignin)
                               .WithGaiaId(fake_gaia_id)
                               .WithRefreshToken(refresh_token)
                               .WithRefreshTokenBindingKey(wrapped_key)
                               .Build(email));
  ASSERT_EQ(
      identity_manager().GetPrimaryAccountInfo(signin::ConsentLevel::kSignin),
      account_info);
  ASSERT_TRUE(identity_manager().HasAccountWithBoundRefreshToken(
      account_info.account_id));

  bound_session_cookie_refresh_service().RegisterNewBoundSession(
      CreateSIDTSBoundSessionParams(
          std::string(wrapped_key.begin(), wrapped_key.end())));
  ASSERT_THAT(
      bound_session_cookie_refresh_service().GetBoundSessionThrottlerParams(),
      SizeIs(1));

  // Configure `FakeGaia`:
  // - make `ListAccounts` return two accounts making Chrome to trigger OAML,
  // - make OAML eventually return the bound cookies,
  // - make `RotateBoundCookies` return success (to ensure the session is not
  // terminated).
  fake_gaia_mixin().SetupFakeGaiaForLogin(email, fake_gaia_id, refresh_token);
  FakeGaia::Configuration config;
  config.spec_compliant_device_bound_session = GetParam();
  config.session_sid_cookie = "fake_sid";
  config.session_lsid_cookie = "fake_lsid";
  config.session_1p_sidts_cookie = "fake_1p_sidts";
  config.session_3p_sidts_cookie = "fake_3p_sidts";
  config.emails = {email, "deleted_user@gmail.com"};
  config.rotated_cookies = {std::string(k1PSIDTSCookieName),
                            std::string(k3PSIDTSCookieName)};
  fake_gaia().SetConfiguration(config);

  // Make sure that subsequent `/ListAccounts` returns the primary account
  // only.
  config.emails = {email};
  UpdateFakeGaiaConfigOnSetOnAccountsInCookieUpdated(config);

  TestAccountReconcilorObserver account_reconcilor_observer(
      AccountReconcilorFactory::GetForProfile(browser()->profile()),
      /*wait_state=*/signin_metrics::AccountReconcilorState::kOk);

  // Enforce initial `/ListAccounts`.
  signin::SetFreshnessOfAccountsInGaiaCookie(&identity_manager(),
                                             /*accounts_are_fresh=*/false);

  // Wait until the reconcilor reaches the
  // `signin_metrics::AccountReconcilorState::kOk` state.
  account_reconcilor_observer.WaitForStateChange();

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

  // Verify that the bound session is present and it has been overridden.
  EXPECT_THAT(
      bound_session_cookie_refresh_service().GetBoundSessionThrottlerParams(),
      SizeIs(1));
  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.SessionTerminationTrigger",
      BoundSessionCookieRefreshServiceImpl::SessionTerminationTrigger::
          kSessionOverride,
      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(,
                         BoundSessionOAuthMultiloginPrototypeNewSessionTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "SpecCompliantServerResponse"
                                             : "PrototypeServerResponse";
                         });

struct PersistentErrorTestParam {
  OAuthMultiloginResponseStatus oauth_multilogin_response_status =
      OAuthMultiloginResponseStatus::kUnknownStatus;
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  std::string test_suffix;
};

class BoundSessionOAuthMultiloginPersistentErrorTest
    : public BoundSessionOAuthMultiloginBaseTest,
      public testing::WithParamInterface<PersistentErrorTestParam> {
 public:
  BoundSessionOAuthMultiloginPersistentErrorTest()
      : BoundSessionOAuthMultiloginBaseTest(GetParam().enabled_features,
                                            GetParam().disabled_features) {}
};

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

  // This makes sure that OAML will return a given error. At the same
  // time, `/ListAccounts` WON'T return accounts, which will trigger OAML -
  // it simulates similar scenario to cookies being cleared.
  FakeGaia::Configuration config;
  config.oauth_multilogin_response_status =
      GetParam().oauth_multilogin_response_status;
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

  // This makes sure that OAML will return a given error. At the same
  // time, `/ListAccounts` WON'T return accounts, which will trigger OAML -
  // it simulates similar scenario to cookies being cleared.
  FakeGaia::Configuration config;
  config.oauth_multilogin_response_status =
      GetParam().oauth_multilogin_response_status;
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

INSTANTIATE_TEST_SUITE_P(
    ,
    BoundSessionOAuthMultiloginPersistentErrorTest,
    Values(
        PersistentErrorTestParam{
            .oauth_multilogin_response_status =
                OAuthMultiloginResponseStatus::kInvalidInput,
            .enabled_features = GetPrototypeFeatures(),
            .disabled_features = GetStandardFeatures(),
            .test_suffix = "PrototypeInvalidInput"},
        PersistentErrorTestParam{.oauth_multilogin_response_status =
                                     OAuthMultiloginResponseStatus::kError,
                                 .enabled_features = GetPrototypeFeatures(),
                                 .disabled_features = GetStandardFeatures(),
                                 .test_suffix = "PrototypeError"},
        PersistentErrorTestParam{
            .oauth_multilogin_response_status =
                OAuthMultiloginResponseStatus::kInvalidInput,
            .enabled_features = GetStandardFeatures(),
            .disabled_features = GetPrototypeFeatures(),
            .test_suffix = "StandardInvalidInput"},
        PersistentErrorTestParam{.oauth_multilogin_response_status =
                                     OAuthMultiloginResponseStatus::kError,
                                 .enabled_features = GetStandardFeatures(),
                                 .disabled_features = GetPrototypeFeatures(),
                                 .test_suffix = "StandardError"}),
    [](const testing::TestParamInfo<PersistentErrorTestParam>& info) {
      return info.param.test_suffix;
    });

class BoundSessionOAuthMultiloginStandardTest
    : public BoundSessionOAuthMultiloginBaseTest {
 public:
  BoundSessionOAuthMultiloginStandardTest()
      : BoundSessionOAuthMultiloginBaseTest(GetStandardFeatures(),
                                            GetPrototypeFeatures()) {}
};

IN_PROC_BROWSER_TEST_F(BoundSessionOAuthMultiloginStandardTest,
                       StartsNewBoundSession) {
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

  {
    // Verify that there are no bound sessions before OAML.
    base::test::TestFuture<
        const std::vector<net::device_bound_sessions::SessionKey>&>
        sessions_future;
    device_bound_session_manager().GetAllSessions(
        sessions_future.GetCallback());
    ASSERT_THAT(sessions_future.Get(), IsEmpty());
  }

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

  // Make sure that `/ListAccounts` return the primary account to avoid
  // triggering OAML indefinitely.
  config.emails = {FakeGaiaMixin::kFakeUserEmail};
  UpdateFakeGaiaConfigOnSetOnAccountsInCookieUpdated(config);

  base::RunLoop run_loop;
  DeviceBoundSessionAccessObserver observer(
      device_bound_session_manager(),
      base::IgnoreArgs<const net::device_bound_sessions::SessionAccess&>(
          run_loop.QuitClosure()));

  // Enforce initial `/ListAccounts`.
  signin::SetFreshnessOfAccountsInGaiaCookie(&identity_manager(),
                                             /*accounts_are_fresh=*/false);

  // Wait until the observer receives the notification about the new session.
  run_loop.Run();

  base::queue<FakeGaia::MultiloginCall> multilogin_calls =
      fake_gaia().GetAndResetMultiloginCalls();

  ASSERT_THAT(multilogin_calls, SizeIs(2));

  const auto& first_call = multilogin_calls.front();
  // OAML first returns the challenge to sign.
  ASSERT_EQ(first_call.action,
            FakeGaia::MultiloginCall::Action::kReturnBindingChallenge);
  multilogin_calls.pop();

  // After the challenge is received and signed, OAML returns the bound
  // cookies.
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

  base::test::TestFuture<
      const std::vector<net::device_bound_sessions::SessionKey>&>
      sessions_future;
  device_bound_session_manager().GetAllSessions(sessions_future.GetCallback());
  EXPECT_THAT(
      sessions_future.Get(),
      UnorderedElementsAre(AllOf(
          Field(&net::device_bound_sessions::SessionKey::id,
                net::device_bound_sessions::SessionKey::Id("sidts_session")),
          Field(&net::device_bound_sessions::SessionKey::site,
                net::SchemefulSite::Deserialize("https://google.com")))));
}

IN_PROC_BROWSER_TEST_F(BoundSessionOAuthMultiloginStandardTest,
                       StartsMultipleSessions) {
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

  {
    // Verify that there are no bound sessions before OAML.
    base::test::TestFuture<
        const std::vector<net::device_bound_sessions::SessionKey>&>
        sessions_future;
    device_bound_session_manager().GetAllSessions(
        sessions_future.GetCallback());
    ASSERT_THAT(sessions_future.Get(), IsEmpty());
  }

  // This makes sure that eventually OAML will return bound cookies, at the same
  // time `/ListAccounts` WON'T return primary account triggering OAML - it
  // simulates similar scenario to cookies being cleared.
  fake_gaia_mixin().SetupFakeGaiaForLoginWithDefaults();
  FakeGaia::Configuration config;
  config.session_sid_cookie = "fake_sid";
  config.session_lsid_cookie = "fake_lsid";
  config.session_1p_sidts_cookie = "fake_1p_sidts";
  config.session_3p_sidts_cookie = "fake_3p_sidts";
  config.include_youtube_bound_session = true;
  fake_gaia().SetConfiguration(config);

  // Make sure that `/ListAccounts` return the primary account to avoid
  // triggering OAML indefinitely.
  config.emails = {FakeGaiaMixin::kFakeUserEmail};
  UpdateFakeGaiaConfigOnSetOnAccountsInCookieUpdated(config);

  base::RunLoop run_loop;
  DeviceBoundSessionAccessObserver observer(
      device_bound_session_manager(),
      base::IgnoreArgs<const net::device_bound_sessions::SessionAccess&>(
          run_loop.QuitClosure()));

  // Enforce initial `/ListAccounts`.
  signin::SetFreshnessOfAccountsInGaiaCookie(&identity_manager(),
                                             /*accounts_are_fresh=*/false);

  // Wait until the observer receives the notification about the new session.
  run_loop.Run();

  base::queue<FakeGaia::MultiloginCall> multilogin_calls =
      fake_gaia().GetAndResetMultiloginCalls();

  ASSERT_THAT(multilogin_calls, SizeIs(2));

  const auto& first_call = multilogin_calls.front();
  // OAML first returns the challenge to sign.
  ASSERT_EQ(first_call.action,
            FakeGaia::MultiloginCall::Action::kReturnBindingChallenge);
  multilogin_calls.pop();

  // After the challenge is received and signed, OAML returns the bound
  // cookies.
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

  base::test::TestFuture<
      const std::vector<net::device_bound_sessions::SessionKey>&>
      sessions_future;
  device_bound_session_manager().GetAllSessions(sessions_future.GetCallback());
  EXPECT_THAT(
      sessions_future.Get(),
      UnorderedElementsAre(
          AllOf(Field(&net::device_bound_sessions::SessionKey::id,
                      net::device_bound_sessions::SessionKey::Id(
                          "sidts_session")),
                Field(&net::device_bound_sessions::SessionKey::site,
                      net::SchemefulSite::Deserialize("https://google.com"))),
          AllOf(
              Field(
                  &net::device_bound_sessions::SessionKey::id,
                  net::device_bound_sessions::SessionKey::Id("sidts_session")),
              Field(&net::device_bound_sessions::SessionKey::site,
                    net::SchemefulSite::Deserialize("https://youtube.com")))));
}

IN_PROC_BROWSER_TEST_F(BoundSessionOAuthMultiloginStandardTest,
                       ReuseExistingSession) {
  const unexportable_keys::UnexportableKeyId key_id = GenerateNewKey();
  const std::vector<uint8_t> wrapped_key = GetWrappedKey(key_id);

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

  // Configure `FakeGaia`:
  // - make `ListAccounts` return only the primary account making Chrome to
  // trigger OAML,
  // - make OAML return reuse bound session response,
  // - make `RotateBoundCookies` return success (to ensure the session is not
  // terminated).
  fake_gaia_mixin().SetupFakeGaiaForLogin(email_1, fake_gaia_id_1,
                                          refresh_token_1);
  fake_gaia_mixin().SetupFakeGaiaForLogin(email_2, fake_gaia_id_2,
                                          refresh_token_2);
  FakeGaia::Configuration config;
  config.session_sid_cookie = "fake_sid";
  config.session_lsid_cookie = "fake_lsid";
  config.session_1p_sidts_cookie = "fake_1p_sidts";
  config.session_3p_sidts_cookie = "fake_3p_sidts";
  config.emails = {email_1};
  config.reuse_bound_session = true;
  config.rotated_cookies = {std::string(k1PSIDTSCookieName),
                            std::string(k3PSIDTSCookieName)};
  fake_gaia().SetConfiguration(config);

  // Make sure that subsequent `/ListAccounts` returns both accounts.
  config.emails = {email_1, email_2};
  UpdateFakeGaiaConfigOnSetOnAccountsInCookieUpdated(config);

  // Create a session to reuse.
  base::test::TestFuture<
      const std::vector<net::device_bound_sessions::SessionError::ErrorType>&,
      std::vector<net::CookieInclusionStatus>>
      create_future;
  std::vector<net::device_bound_sessions::SessionParams> params;
  params.push_back(CreateTestSessionParams());
  const std::string session_id = params[0].session_id;
  const net::SchemefulSite session_site =
      net::SchemefulSite(params[0].fetcher_url);
  device_bound_session_manager().CreateBoundSessions(
      std::move(params), wrapped_key, /*cookies_to_set=*/{},
      net::CookieOptions(), create_future.GetCallback());
  ASSERT_THAT(
      create_future.Get<0>(),
      UnorderedElementsAre(
          net::device_bound_sessions::SessionError::ErrorType::kSuccess));

  bool session_created = false;
  DeviceBoundSessionAccessObserver observer(
      device_bound_session_manager(),
      base::BindLambdaForTesting([&session_created](
                                     const net::device_bound_sessions::
                                         SessionAccess& access) {
        if (access.access_type ==
            net::device_bound_sessions::SessionAccess::AccessType::kCreation) {
          session_created = true;
        }
      }));

  TestAccountReconcilorObserver account_reconcilor_observer(
      AccountReconcilorFactory::GetForProfile(browser()->profile()),
      /*wait_state=*/signin_metrics::AccountReconcilorState::kOk);

  // Enforce initial `/ListAccounts`.
  signin::SetFreshnessOfAccountsInGaiaCookie(&identity_manager(),
                                             /*accounts_are_fresh=*/false);

  // Wait until the reconcilor reaches the
  // `signin_metrics::AccountReconcilorState::kOk` state.
  account_reconcilor_observer.WaitForStateChange();

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

  // Verify that the bound session is present and it hasn't been
  // terminated.
  ASSERT_FALSE(session_created);
  base::test::TestFuture<
      const std::vector<net::device_bound_sessions::SessionKey>&>
      sessions_future;
  device_bound_session_manager().GetAllSessions(sessions_future.GetCallback());
  EXPECT_THAT(
      sessions_future.Get(),
      UnorderedElementsAre(AllOf(
          Field(&net::device_bound_sessions::SessionKey::id,
                net::device_bound_sessions::SessionKey::Id(session_id)),
          Field(&net::device_bound_sessions::SessionKey::site, session_site))));
}

IN_PROC_BROWSER_TEST_F(BoundSessionOAuthMultiloginStandardTest,
                       OverrideExistingSession) {
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

  // Make sure that subsequent `/ListAccounts` return the missing primary
  // account to avoid triggering OAML indefinitely.
  config.emails = {FakeGaiaMixin::kFakeUserEmail};
  UpdateFakeGaiaConfigOnSetOnAccountsInCookieUpdated(config);

  base::test::TestFuture<
      const std::vector<net::device_bound_sessions::SessionError::ErrorType>&,
      std::vector<net::CookieInclusionStatus>>
      create_future;
  std::vector<net::device_bound_sessions::SessionParams> params;
  params.push_back(CreateTestSessionParams());
  const std::string session_id = params[0].session_id;
  const net::SchemefulSite session_site =
      net::SchemefulSite(params[0].fetcher_url);
  device_bound_session_manager().CreateBoundSessions(
      std::move(params), wrapped_key, /*cookies_to_set=*/{},
      net::CookieOptions(), create_future.GetCallback());
  ASSERT_THAT(
      create_future.Get<0>(),
      UnorderedElementsAre(
          net::device_bound_sessions::SessionError::ErrorType::kSuccess));

  base::RunLoop run_loop;
  DeviceBoundSessionAccessObserver observer(
      device_bound_session_manager(),
      base::BindLambdaForTesting([&run_loop](const net::device_bound_sessions::
                                                 SessionAccess& access) {
        // Wait until the observer receives the notification about the new
        // session (override).
        if (access.access_type ==
            net::device_bound_sessions::SessionAccess::AccessType::kCreation) {
          run_loop.Quit();
        }
      }));

  // Enforce initial `/ListAccounts`.
  signin::SetFreshnessOfAccountsInGaiaCookie(&identity_manager(),
                                             /*accounts_are_fresh=*/false);

  // Wait until the observer receives the notification about the new session.
  run_loop.Run();

  base::queue<FakeGaia::MultiloginCall> multilogin_calls =
      fake_gaia().GetAndResetMultiloginCalls();

  ASSERT_THAT(multilogin_calls, SizeIs(2));

  const auto& first_call = multilogin_calls.front();
  // OAML first returns the challenge to sign.
  ASSERT_EQ(first_call.action,
            FakeGaia::MultiloginCall::Action::kReturnBindingChallenge);
  multilogin_calls.pop();

  // After the challenge is received and signed, OAML returns the bound
  // cookies.
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

  base::test::TestFuture<
      const std::vector<net::device_bound_sessions::SessionKey>&>
      sessions_future;
  device_bound_session_manager().GetAllSessions(sessions_future.GetCallback());
  EXPECT_THAT(
      sessions_future.Get(),
      UnorderedElementsAre(AllOf(
          Field(&net::device_bound_sessions::SessionKey::id,
                net::device_bound_sessions::SessionKey::Id(session_id)),
          Field(&net::device_bound_sessions::SessionKey::site, session_site))));
}

class BoundSessionOAuthMultiloginStandardWithPrototypeFallbackTest
    : public BoundSessionOAuthMultiloginBaseTest {
 public:
  // Enables both standard and prototype features except a dedicated feature
  // flag for OAML to use DBSC standard. This should trigger the prototype
  // fallback for OAML.
  BoundSessionOAuthMultiloginStandardWithPrototypeFallbackTest()
      : BoundSessionOAuthMultiloginBaseTest(
            /*enabled_features=*/
            {net::features::kDeviceBoundSessions,
             network::features::kUseUnexportableKeyServiceInBrowserProcess,
             switches::kEnableBoundSessionCredentials,
             switches::kEnableOAuthMultiloginCookiesBinding},
            /*disabled_features=*/{
                switches::kEnableOAuthMultiloginStandardCookiesBinding}) {}
};

IN_PROC_BROWSER_TEST_F(
    BoundSessionOAuthMultiloginStandardWithPrototypeFallbackTest,
    StartsNewBoundSession) {
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
  // Verify that there are no bound sessions for DBSC prototype before OAML.
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

  // Make sure that subsequent `/ListAccounts` return the missing primary
  // account to avoid triggering OAML indefinitely.
  config.emails = {FakeGaiaMixin::kFakeUserEmail};
  UpdateFakeGaiaConfigOnSetOnAccountsInCookieUpdated(config);

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

  // Verify that the new bound session started for the DBSC prototype.
  EXPECT_THAT(
      bound_session_cookie_refresh_service().GetBoundSessionThrottlerParams(),
      SizeIs(1));

  // Verify that no session is created for the DBSC standard.
  base::test::TestFuture<
      const std::vector<net::device_bound_sessions::SessionKey>&>
      sessions_future;
  device_bound_session_manager().GetAllSessions(sessions_future.GetCallback());
  EXPECT_THAT(sessions_future.Get(), IsEmpty());
}
