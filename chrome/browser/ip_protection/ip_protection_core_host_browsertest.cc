// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_core_host.h"

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ip_protection/ip_protection_core_host_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom-test-utils.h"
#include "services/network/public/mojom/network_context.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#endif

using ::ip_protection::BlindSignedAuthToken;
using ::ip_protection::GeoHint;

namespace {
class ScopedIpProtectionFeatureList {
 public:
  ScopedIpProtectionFeatureList() {
    feature_list_.InitWithFeatures({net::features::kEnableIpProtectionProxy,
                                    network::features::kMaskedDomainList},
                                   {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Helper class to intercept `IpProtectionConfigGetter::TryGetAuthTokens()`
// requests and return a fake token value and a fake expiration value for
// testing.
class IpProtectionConfigGetterInterceptor
    : public network::mojom::IpProtectionConfigGetterInterceptorForTesting {
 public:
  IpProtectionConfigGetterInterceptor(IpProtectionCoreHost* getter,
                                      std::string token,
                                      base::Time expiration,
                                      GeoHint geo_hint,
                                      bool should_intercept = true)
      : getter_(getter),
        receiver_id_(getter_->receiver_id_for_testing()),
        token_(std::move(token)),
        expiration_(expiration),
        geo_hint_(geo_hint),
        should_intercept_(should_intercept) {
    auto* old_impl =
        getter_->receivers_for_testing().SwapImplForTesting(receiver_id_, this);
    // We should only ever be replacing `getter` as the impl.
    CHECK_EQ(getter, old_impl);
  }

  ~IpProtectionConfigGetterInterceptor() override {
    std::ignore = getter_->receivers_for_testing().SwapImplForTesting(
        receiver_id_, getter_);
  }

  network::mojom::IpProtectionConfigGetter* GetForwardingInterface() override {
    return getter_;
  }

  void TryGetAuthTokens(uint32_t batch_size,
                        network::mojom::IpProtectionProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override {
    if (should_intercept_) {
      std::vector<BlindSignedAuthToken> tokens;
      for (uint32_t i = 0; i < batch_size; i++) {
        BlindSignedAuthToken token = {
            .token = token_, .expiration = expiration_, .geo_hint = geo_hint_};
        tokens.push_back(std::move(token));
      }
      std::move(callback).Run(std::move(tokens), base::Time());
      return;
    }
    GetForwardingInterface()->TryGetAuthTokens(batch_size, proxy_layer,
                                               std::move(callback));
  }

  std::string token() const { return token_; }

  base::Time expiration() const { return expiration_; }

  GeoHint geo_hint() const { return geo_hint_; }

  void EnableInterception() { should_intercept_ = true; }
  void DisableInterception() { should_intercept_ = false; }

 private:
  raw_ptr<IpProtectionCoreHost> getter_;
  // `getter_->receiver_id_for_testing()` will return the `mojo::ReceiverId` of
  // the most recently added receiver, so save this value off to ensure that on
  // destruction we restore the implementation for the correct receiver (for
  // cases where we have multiple receivers and use more than one interceptor).
  mojo::ReceiverId receiver_id_;
  std::string token_;
  base::Time expiration_;
  GeoHint geo_hint_;
  bool should_intercept_;
};

constexpr base::Time kDontRetry = base::Time::Max();
}  // namespace

class IpProtectionCoreHostBrowserTest : public PlatformBrowserTest {
 public:
  IpProtectionCoreHostBrowserTest()
      : profile_selections_(
            IpProtectionCoreHostFactory::GetInstance(),
            IpProtectionCoreHostFactory::CreateProfileSelectionsForTesting()) {}

  void TearDownOnMainThread() override {
    PlatformBrowserTest::TearDownOnMainThread();

    IpProtectionCoreHost::Get(GetProfile())->Shutdown();
  }

  void CreateIncognitoNetworkContextAndInterceptors() {
    IpProtectionCoreHost* provider = IpProtectionCoreHost::Get(GetProfile());
    ASSERT_TRUE(provider);
    ASSERT_EQ(provider->receivers_for_testing().size(), 1U);

    std::string token = "best_token_ever";
    base::Time expiration = base::Time::Now() + base::Seconds(12345);
    GeoHint geo_hint = {
        .country_code = "US", .iso_region = "US-AL", .city_name = "ALABASTER"};
    main_profile_auth_token_getter_interceptor_ =
        std::make_unique<IpProtectionConfigGetterInterceptor>(
            provider, token, expiration, geo_hint, /*should_intercept=*/false);

    network::mojom::NetworkContext* main_profile_network_context =
        GetProfile()->GetDefaultStoragePartition()->GetNetworkContext();
    main_profile_ipp_control_ = provider->last_remote_for_testing();

    incognito_profile_ =
        GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    ASSERT_EQ(provider->receivers_for_testing().size(), 2U);
    network::mojom::NetworkContext* incognito_profile_network_context =
        incognito_profile_->GetDefaultStoragePartition()->GetNetworkContext();
    incognito_profile_ipp_control_ = provider->last_remote_for_testing();
    ASSERT_NE(main_profile_network_context, incognito_profile_network_context);
    ASSERT_NE(main_profile_ipp_control_, incognito_profile_ipp_control_);

    incognito_profile_auth_token_getter_interceptor_ =
        std::make_unique<IpProtectionConfigGetterInterceptor>(
            provider, token, expiration, geo_hint, /*should_intercept=*/false);
  }

  void EnableInterception() {
    main_profile_auth_token_getter_interceptor_->EnableInterception();
    incognito_profile_auth_token_getter_interceptor_->EnableInterception();
  }

  void DestroyIncognitoNetworkContextAndInterceptors() {
    ASSERT_TRUE(incognito_profile_)
        << "CreateNetworkContextsAndInterceptors() must have been called first";

    incognito_profile_auth_token_getter_interceptor_ = nullptr;
    main_profile_auth_token_getter_interceptor_ = nullptr;

    main_profile_ipp_control_ = nullptr;
    incognito_profile_ipp_control_ = nullptr;

    Profile* incognito_profile = incognito_profile_;
    incognito_profile_ = nullptr;
    GetProfile()->DestroyOffTheRecordProfile(incognito_profile);
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

 protected:
  raw_ptr<network::mojom::IpProtectionControl> main_profile_ipp_control_ =
      nullptr;
  std::unique_ptr<IpProtectionConfigGetterInterceptor>
      main_profile_auth_token_getter_interceptor_;

  raw_ptr<Profile> incognito_profile_ = nullptr;
  raw_ptr<network::mojom::IpProtectionControl> incognito_profile_ipp_control_ =
      nullptr;
  std::unique_ptr<IpProtectionConfigGetterInterceptor>
      incognito_profile_auth_token_getter_interceptor_;

 private:
  // Note that the order of initialization is important here - we want to set
  // the value of the features before anything else since it's used by the
  // `IpProtectionCoreHostFactory` logic.
  ScopedIpProtectionFeatureList scoped_ip_protection_feature_list_;
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      profile_selections_;
};

IN_PROC_BROWSER_TEST_F(IpProtectionCoreHostBrowserTest,
                       NetworkServiceCanRequestTokens) {
  IpProtectionCoreHost* getter = IpProtectionCoreHost::Get(GetProfile());
  ASSERT_TRUE(getter);

  std::string token = "best_token_ever";
  base::Time expiration = base::Time::Now() + base::Seconds(12345);
  GeoHint geo_hint = {
      .country_code = "US", .iso_region = "US-AL", .city_name = "ALABASTER"};
  ASSERT_EQ(getter->receivers_for_testing().size(), 1U);
  auto auth_token_getter_interceptor_ =
      std::make_unique<IpProtectionConfigGetterInterceptor>(
          getter, token, expiration, geo_hint);

  // To test that the Network Service can successfully request tokens, use the
  // test method on NetworkContext that will have it request tokens and then
  // send back the first token that it receives.
  base::test::TestFuture<const std::optional<BlindSignedAuthToken>&,
                         std::optional<base::Time>>
      future;
  auto* ipp_control = getter->last_remote_for_testing();
  ipp_control->VerifyIpProtectionConfigGetterForTesting(future.GetCallback());
  const std::optional<BlindSignedAuthToken>& result =
      future.Get<std::optional<BlindSignedAuthToken>>();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->token, token);
  // Expiration is "fuzzed" backward in time, so expect less-than.
  EXPECT_LT(result->expiration, expiration);

  // Now create a new incognito mode profile (with a different associated
  // network context) and see whether we can request tokens from that.
  GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  // We need a new interceptor that will intercept messages corresponding to the
  // incognito mode network context's `mojo::Receiver()`.
  ASSERT_EQ(getter->receivers_for_testing().size(), 2U);
  auto incognito_auth_token_getter_interceptor_ =
      std::make_unique<IpProtectionConfigGetterInterceptor>(
          getter, token, expiration, geo_hint);

  // Verify that we can get tokens from the incognito mode profile.
  future.Clear();
  auto* incognito_ipp_control = getter->last_remote_for_testing();
  ASSERT_NE(incognito_ipp_control, ipp_control);
  incognito_ipp_control->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const std::optional<BlindSignedAuthToken>& incognito_result =
      future.Get<std::optional<BlindSignedAuthToken>>();
  ASSERT_TRUE(incognito_result);
  EXPECT_EQ(incognito_result->token, token);
  EXPECT_LT(incognito_result->expiration, expiration);

  // Ensure that we can still get tokens from the main profile.
  future.Clear();
  ipp_control->VerifyIpProtectionConfigGetterForTesting(future.GetCallback());
  const std::optional<BlindSignedAuthToken>& second_attempt_result =
      future.Get<std::optional<BlindSignedAuthToken>>();
  ASSERT_TRUE(second_attempt_result);
  EXPECT_EQ(second_attempt_result->token, token);
  EXPECT_LT(second_attempt_result->expiration, expiration);
}

IN_PROC_BROWSER_TEST_F(IpProtectionCoreHostBrowserTest,
                       ExpectedReceiverSetStateAfterNetworkServiceCrash) {
  IpProtectionCoreHost* getter = IpProtectionCoreHost::Get(GetProfile());
  ASSERT_TRUE(getter);
  ASSERT_EQ(getter->receivers_for_testing().size(), 1U);

  Profile* incognito_profile =
      GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_EQ(getter->receivers_for_testing().size(), 2U);

  // If the network service isn't out-of-process then we can't crash it.
  if (!content::IsOutOfProcessNetworkService()) {
    return;
  }

  // Crash the Network Service process and ensure that the error notifications
  // get propagated. When the network contexts are recreated, they will both
  // attempt to get tokens and we should ensure that nothing crashes as a
  // result.
  SimulateNetworkServiceCrash();
  GetProfile()->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();
  incognito_profile->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();
  // Ensure that any lingering receivers have had time to be removed.
  getter->receivers_for_testing().FlushForTesting();

  ASSERT_EQ(getter->receivers_for_testing().size(), 2U);
}

#if !BUILDFLAG(IS_ANDROID)
class IpProtectionCoreHostIdentityBrowserTest
    : public IpProtectionCoreHostBrowserTest {
 public:
  IpProtectionCoreHostIdentityBrowserTest() {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&IpProtectionCoreHostIdentityBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(GetProfile());

    MakePrimaryAccountAvailable();

    identity_test_environment_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

  void TearDownOnMainThread() override {
    IpProtectionCoreHostBrowserTest::TearDownOnMainThread();

    identity_test_environment_adaptor_.reset();
  }

  signin::IdentityManager* IdentityManager() {
    return identity_test_environment_adaptor_->identity_test_env()
        ->identity_manager();
  }

  void MakePrimaryAccountAvailable() {
    identity_test_environment_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable("test@example.com",
                                      signin::ConsentLevel::kSignin);
  }

  void ClearPrimaryAccount() {
    identity_test_environment_adaptor_->identity_test_env()
        ->ClearPrimaryAccount();
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(IpProtectionCoreHostIdentityBrowserTest,
                       BackoffTimeResetAfterProfileAvailabilityChange) {
  CreateIncognitoNetworkContextAndInterceptors();
  // Simulate logging the user out, which should make the provider indicate that
  // `TryGetAuthTokens()` calls should not be retried on the next request.
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  ClearPrimaryAccount();
#else
  IpProtectionCoreHost* provider = IpProtectionCoreHost::Get(GetProfile());
  ASSERT_TRUE(provider);

  // On ChromeOS, `ClearPrimaryAccount()` either isn't supported (Ash) or
  // doesn't seem to work well (Lacros), but we can still test that our
  // implementation works by mocking up the account status change event.
  provider->OnPrimaryAccountChanged(signin::PrimaryAccountChangeEvent(
      signin::PrimaryAccountChangeEvent::State(
          IdentityManager()->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin),
          signin::ConsentLevel::kSignin),
      signin::PrimaryAccountChangeEvent::State(),
      signin_metrics::ProfileSignout::kTest));
#endif

  // Request tokens from both contexts and ensure that the "don't retry"
  // cooldown time is returned. The provider should do this itself, so the
  // interceptors won't be used for this part.
  base::test::TestFuture<const std::optional<BlindSignedAuthToken>&,
                         std::optional<base::Time>>
      future;
  main_profile_ipp_control_->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const std::optional<base::Time>& main_profile_first_attempt_result =
      future.Get<std::optional<base::Time>>();
  EXPECT_EQ(main_profile_first_attempt_result.value(), kDontRetry);

  future.Clear();
  incognito_profile_ipp_control_->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const std::optional<base::Time>& incognito_profile_first_attempt_result =
      future.Get<std::optional<base::Time>>();
  EXPECT_EQ(incognito_profile_first_attempt_result.value(), kDontRetry);

  // Make the interceptors return tokens now so that if the network service
  // isn't respecting the cooldown, tokens will be returned and the test will
  // fail below.
  EnableInterception();

  // Run the test again and check that the network service is still in a
  // cooldown phase.
  future.Clear();
  main_profile_ipp_control_->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const std::optional<base::Time>& main_profile_second_attempt_result =
      future.Get<std::optional<base::Time>>();
  EXPECT_EQ(main_profile_second_attempt_result.value(), kDontRetry);

  future.Clear();
  incognito_profile_ipp_control_->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const std::optional<base::Time>& incognito_profile_second_attempt_result =
      future.Get<std::optional<base::Time>>();
  EXPECT_EQ(incognito_profile_second_attempt_result.value(), kDontRetry);

  // Simulate the account becoming active again, which should cause `provider`
  // to notify the network contexts. No need to flush the remotes after, since
  // the messages generated will be in order with the
  // `VerifyIpProtectionConfigGetterForTesting()` messages used below.
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  MakePrimaryAccountAvailable();
#else
  provider->OnPrimaryAccountChanged(signin::PrimaryAccountChangeEvent(
      signin::PrimaryAccountChangeEvent::State(),
      signin::PrimaryAccountChangeEvent::State(
          IdentityManager()->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin),
          signin::ConsentLevel::kSignin),
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN));
#endif

  // Verify that cooldown timers in the network context have been reset and
  // that we can now request tokens successfully.
  future.Clear();
  main_profile_ipp_control_->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const std::optional<BlindSignedAuthToken>& main_profile_third_attempt_result =
      future.Get<std::optional<BlindSignedAuthToken>>();
  ASSERT_TRUE(main_profile_third_attempt_result);
  EXPECT_EQ(main_profile_third_attempt_result->token,
            main_profile_auth_token_getter_interceptor_->token());
  EXPECT_LT(main_profile_third_attempt_result->expiration,
            main_profile_auth_token_getter_interceptor_->expiration());

  future.Clear();
  incognito_profile_ipp_control_->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const std::optional<BlindSignedAuthToken>&
      incognito_profile_third_attempt_result =
          future.Get<std::optional<BlindSignedAuthToken>>();
  ASSERT_TRUE(incognito_profile_third_attempt_result);
  EXPECT_EQ(incognito_profile_third_attempt_result->token,
            incognito_profile_auth_token_getter_interceptor_->token());
  EXPECT_LT(incognito_profile_third_attempt_result->expiration,
            incognito_profile_auth_token_getter_interceptor_->expiration());

  DestroyIncognitoNetworkContextAndInterceptors();
}
#endif

class IpProtectionCoreHostUserSettingBrowserTest
    : public IpProtectionCoreHostBrowserTest {
 public:
  IpProtectionCoreHostUserSettingBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(privacy_sandbox::kIpProtectionV1);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(IpProtectionCoreHostUserSettingBrowserTest,
                       OnIpProtectionEnabledChanged) {
  CreateIncognitoNetworkContextAndInterceptors();

  IpProtectionCoreHost* provider = IpProtectionCoreHost::Get(GetProfile());
  ASSERT_TRUE(provider);

  // Simulate the user disabling the IP Protection setting.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kIpProtectionEnabled, false);
  provider->OnIpProtectionEnabledChanged();

  // Check that network contexts got notified that IP Protection should be
  // disabled.
  base::test::TestFuture<bool> main_profile_is_enabled_future;
  base::test::TestFuture<bool> incognito_profile_is_enabled_future;

  main_profile_ipp_control_->IsIpProtectionEnabledForTesting(
      main_profile_is_enabled_future.GetCallback());
  incognito_profile_ipp_control_->IsIpProtectionEnabledForTesting(
      incognito_profile_is_enabled_future.GetCallback());

  EXPECT_FALSE(main_profile_is_enabled_future.Get());
  EXPECT_FALSE(incognito_profile_is_enabled_future.Get());

  // Request tokens from both contexts and ensure that the "don't retry"
  // cooldown time is returned.
  base::test::TestFuture<const std::optional<BlindSignedAuthToken>&,
                         std::optional<base::Time>>
      main_profile_verification_future;
  base::test::TestFuture<const std::optional<BlindSignedAuthToken>&,
                         std::optional<base::Time>>
      incognito_profile_verification_future;

  main_profile_ipp_control_->VerifyIpProtectionConfigGetterForTesting(
      main_profile_verification_future.GetCallback());
  incognito_profile_ipp_control_->VerifyIpProtectionConfigGetterForTesting(
      incognito_profile_verification_future.GetCallback());

  const std::optional<base::Time>& main_profile_first_attempt_result =
      main_profile_verification_future.Get<std::optional<base::Time>>();
  const std::optional<base::Time>& incognito_profile_first_attempt_result =
      incognito_profile_verification_future.Get<std::optional<base::Time>>();

  EXPECT_EQ(main_profile_first_attempt_result.value(), kDontRetry);
  EXPECT_EQ(incognito_profile_first_attempt_result.value(), kDontRetry);

  // Re-enable the setting and ensure that the network contexts got notified
  // accordingly.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kIpProtectionEnabled, true);
  provider->OnIpProtectionEnabledChanged();

  main_profile_is_enabled_future.Clear();
  incognito_profile_is_enabled_future.Clear();

  main_profile_ipp_control_->IsIpProtectionEnabledForTesting(
      main_profile_is_enabled_future.GetCallback());
  incognito_profile_ipp_control_->IsIpProtectionEnabledForTesting(
      incognito_profile_is_enabled_future.GetCallback());

  EXPECT_TRUE(main_profile_is_enabled_future.Get());
  EXPECT_TRUE(incognito_profile_is_enabled_future.Get());

  // Return tokens for testing so that the calls below will complete
  // successfully if the backoff time was successfully reset.
  EnableInterception();

  main_profile_verification_future.Clear();
  incognito_profile_verification_future.Clear();

  // Verify that cooldown timers in the network context have been reset and
  // that we can now request tokens successfully.
  main_profile_ipp_control_->VerifyIpProtectionConfigGetterForTesting(
      main_profile_verification_future.GetCallback());
  incognito_profile_ipp_control_->VerifyIpProtectionConfigGetterForTesting(
      incognito_profile_verification_future.GetCallback());

  const std::optional<BlindSignedAuthToken>&
      main_profile_second_attempt_result =
          main_profile_verification_future
              .Get<std::optional<BlindSignedAuthToken>>();
  const std::optional<BlindSignedAuthToken>&
      incognito_profile_second_attempt_result =
          incognito_profile_verification_future
              .Get<std::optional<BlindSignedAuthToken>>();

  ASSERT_TRUE(main_profile_second_attempt_result);
  ASSERT_TRUE(incognito_profile_second_attempt_result);

  EXPECT_EQ(main_profile_second_attempt_result->token,
            main_profile_auth_token_getter_interceptor_->token());
  EXPECT_LT(main_profile_second_attempt_result->expiration,
            main_profile_auth_token_getter_interceptor_->expiration());

  EXPECT_EQ(incognito_profile_second_attempt_result->token,
            incognito_profile_auth_token_getter_interceptor_->token());
  EXPECT_LT(incognito_profile_second_attempt_result->expiration,
            incognito_profile_auth_token_getter_interceptor_->expiration());

  DestroyIncognitoNetworkContextAndInterceptors();
}

class IpProtectionCoreHostPolicyBrowserTest : public policy::PolicyTest {
 public:
  IpProtectionCoreHostPolicyBrowserTest()
      : profile_selections_(
            IpProtectionCoreHostFactory::GetInstance(),
            IpProtectionCoreHostFactory::CreateProfileSelectionsForTesting()) {}

  void UpdateIpProtectionEnterpisePolicyValue(bool enabled) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kPrivacySandboxIpProtectionEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(enabled), nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  void UnsetIpProtectionEnterprisePolicyValue() {
    policy::PolicyMap policies;
    provider_.UpdateChromePolicy(policies);
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

  // Note that the order of initialization is important here.
  ScopedIpProtectionFeatureList scoped_ip_protection_feature_list_;
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      profile_selections_;
};

IN_PROC_BROWSER_TEST_F(IpProtectionCoreHostPolicyBrowserTest,
                       IpProtectionEnterprisePolicyDisableAndEnable) {
  IpProtectionCoreHost* provider = IpProtectionCoreHost::Get(GetProfile());

  ASSERT_TRUE(provider->IsIpProtectionEnabled());

  // Setting the enterprise policy value to "Disabled" should change the default
  // IP Protection feature state.
  UpdateIpProtectionEnterpisePolicyValue(/*enabled=*/false);
  EXPECT_FALSE(provider->IsIpProtectionEnabled());

  // Setting the enterprise policy value to "Enabled" should re-enable IP
  // Protection.
  UpdateIpProtectionEnterpisePolicyValue(/*enabled=*/true);
  EXPECT_TRUE(provider->IsIpProtectionEnabled());
}

// Test transitioning from the policy being set to the policy being unset - the
// pref value should no longer be considered managed and should effectively be
// reset to its initial state.
IN_PROC_BROWSER_TEST_F(IpProtectionCoreHostPolicyBrowserTest,
                       IpProtectionEnterprisePolicyUnsetAfterSet) {
  IpProtectionCoreHost* provider = IpProtectionCoreHost::Get(GetProfile());

  UpdateIpProtectionEnterpisePolicyValue(/*enabled=*/false);
  EXPECT_FALSE(provider->IsIpProtectionEnabled());

  UnsetIpProtectionEnterprisePolicyValue();
  EXPECT_TRUE(provider->IsIpProtectionEnabled());
}
