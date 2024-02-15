// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_config_provider.h"

#include "base/callback_list.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ip_protection/ip_protection_config_provider_factory.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
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
  IpProtectionConfigGetterInterceptor(IpProtectionConfigProvider* getter,
                                      std::string token,
                                      base::Time expiration,
                                      bool should_intercept = true)
      : getter_(getter),
        receiver_id_(getter_->receiver_id_for_testing()),
        token_(std::move(token)),
        expiration_(expiration),
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
      // NOTE: We'll ignore batch size and just return one token.
      std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens;
      auto token =
          network::mojom::BlindSignedAuthToken::New(token_, expiration_);
      tokens.push_back(std::move(token));
      std::move(callback).Run(std::move(tokens), base::Time());
      return;
    }
    GetForwardingInterface()->TryGetAuthTokens(batch_size, proxy_layer,
                                               std::move(callback));
  }

  std::string token() const { return token_; }

  base::Time expiration() const { return expiration_; }

  void EnableInterception() { should_intercept_ = true; }
  void DisableInterception() { should_intercept_ = false; }

 private:
  raw_ptr<IpProtectionConfigProvider> getter_;
  // `getter_->receiver_id_for_testing()` will return the `mojo::ReceiverId` of
  // the most recently added receiver, so save this value off to ensure that on
  // destruction we restore the implementation for the correct receiver (for
  // cases where we have multiple receivers and use more than one interceptor).
  mojo::ReceiverId receiver_id_;
  std::string token_;
  base::Time expiration_;
  bool should_intercept_;
};

constexpr char kTestEmail[] = "test@example.com";
constexpr base::Time kDontRetry = base::Time::Max();
}  // namespace

class IpProtectionConfigProviderBrowserTest : public InProcessBrowserTest {
 public:
  IpProtectionConfigProviderBrowserTest()
      : profile_selections_(IpProtectionConfigProviderFactory::GetInstance(),
                            IpProtectionConfigProviderFactory::
                                CreateProfileSelectionsForTesting()) {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&IpProtectionConfigProviderBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());

    MakePrimaryAccountAvailable();

    identity_test_environment_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

  void TearDownOnMainThread() override {
    IpProtectionConfigProvider::Get(browser()->profile())->Shutdown();
    identity_test_environment_adaptor_.reset();
  }

  signin::IdentityManager* IdentityManager() {
    return identity_test_environment_adaptor_->identity_test_env()
        ->identity_manager();
  }

  void MakePrimaryAccountAvailable() {
    identity_test_environment_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable(kTestEmail,
                                      signin::ConsentLevel::kSignin);
  }

  void ClearPrimaryAccount() {
    identity_test_environment_adaptor_->identity_test_env()
        ->ClearPrimaryAccount();
  }

  void CreateIncognitoNetworkContextAndInterceptors() {
    IpProtectionConfigProvider* provider =
        IpProtectionConfigProvider::Get(browser()->profile());
    ASSERT_TRUE(provider);
    ASSERT_EQ(provider->receivers_for_testing().size(), 1U);

    std::string token = "best_token_ever";
    base::Time expiration = base::Time::Now() + base::Seconds(12345);
    main_profile_auth_token_getter_interceptor_ =
        std::make_unique<IpProtectionConfigGetterInterceptor>(
            provider, token, expiration, /*should_intercept=*/false);

    network::mojom::NetworkContext* main_profile_network_context =
        browser()->profile()->GetDefaultStoragePartition()->GetNetworkContext();
    main_profile_ipp_proxy_delegate_ = provider->last_remote_for_testing();

    incognito_profile_browser_ = CreateIncognitoBrowser(browser()->profile());
    ASSERT_EQ(provider->receivers_for_testing().size(), 2U);
    network::mojom::NetworkContext* incognito_profile_network_context =
        incognito_profile_browser_->profile()
            ->GetDefaultStoragePartition()
            ->GetNetworkContext();
    incognito_profile_ipp_proxy_delegate_ = provider->last_remote_for_testing();
    ASSERT_NE(main_profile_network_context, incognito_profile_network_context);
    ASSERT_NE(main_profile_ipp_proxy_delegate_,
              incognito_profile_ipp_proxy_delegate_);

    incognito_profile_auth_token_getter_interceptor_ =
        std::make_unique<IpProtectionConfigGetterInterceptor>(
            provider, token, expiration, /*should_intercept=*/false);
  }

  void EnableInterception() {
    main_profile_auth_token_getter_interceptor_->EnableInterception();
    incognito_profile_auth_token_getter_interceptor_->EnableInterception();
  }

  void DestroyIncognitoNetworkContextAndInterceptors() {
    ASSERT_TRUE(incognito_profile_browser_)
        << "CreateNetworkContextsAndInterceptors() must have been called first";

    incognito_profile_auth_token_getter_interceptor_ = nullptr;
    main_profile_auth_token_getter_interceptor_ = nullptr;

    main_profile_ipp_proxy_delegate_ = nullptr;
    incognito_profile_ipp_proxy_delegate_ = nullptr;

    Browser* incognito_profile_browser = incognito_profile_browser_;
    incognito_profile_browser_ = nullptr;
    CloseBrowserSynchronously(incognito_profile_browser);
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  raw_ptr<Browser> incognito_profile_browser_ = nullptr;
  raw_ptr<network::mojom::IpProtectionProxyDelegate>
      main_profile_ipp_proxy_delegate_ = nullptr;
  raw_ptr<network::mojom::IpProtectionProxyDelegate>
      incognito_profile_ipp_proxy_delegate_ = nullptr;
  std::unique_ptr<IpProtectionConfigGetterInterceptor>
      main_profile_auth_token_getter_interceptor_;
  std::unique_ptr<IpProtectionConfigGetterInterceptor>
      incognito_profile_auth_token_getter_interceptor_;

 private:
  // Note that the order of initialization is important here - we want to set
  // the value of the features before anything else since it's used by the
  // `IpProtectionConfigProviderFactory` logic.
  ScopedIpProtectionFeatureList scoped_ip_protection_feature_list_;
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      profile_selections_;

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(IpProtectionConfigProviderBrowserTest,
                       NetworkServiceCanRequestTokens) {
  IpProtectionConfigProvider* getter =
      IpProtectionConfigProvider::Get(browser()->profile());
  ASSERT_TRUE(getter);

  std::string token = "best_token_ever";
  base::Time expiration = base::Time::Now() + base::Seconds(12345);
  ASSERT_EQ(getter->receivers_for_testing().size(), 1U);
  auto auth_token_getter_interceptor_ =
      std::make_unique<IpProtectionConfigGetterInterceptor>(getter, token,
                                                            expiration);

  // To test that the Network Service can successfully request tokens, use the
  // test method on NetworkContext that will have it request tokens and then
  // send back the first token that it receives.
  base::test::TestFuture<network::mojom::BlindSignedAuthTokenPtr,
                         std::optional<base::Time>>
      future;
  auto* ipp_proxy_delegate = getter->last_remote_for_testing();
  ipp_proxy_delegate->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const network::mojom::BlindSignedAuthTokenPtr& result =
      future.Get<network::mojom::BlindSignedAuthTokenPtr>();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->token, token);
  // Expiration is "fuzzed" backward in time, so expect less-than.
  EXPECT_LT(result->expiration, expiration);

  // Now create a new incognito mode profile (with a different associated
  // network context) and see whether we can request tokens from that.
  CreateIncognitoBrowser(browser()->profile());

  // We need a new interceptor that will intercept messages corresponding to the
  // incognito mode network context's `mojo::Receiver()`.
  ASSERT_EQ(getter->receivers_for_testing().size(), 2U);
  auto incognito_auth_token_getter_interceptor_ =
      std::make_unique<IpProtectionConfigGetterInterceptor>(getter, token,
                                                            expiration);

  // Verify that we can get tokens from the incognito mode profile.
  future.Clear();
  auto* incognito_ipp_proxy_delegate = getter->last_remote_for_testing();
  ASSERT_NE(incognito_ipp_proxy_delegate, ipp_proxy_delegate);
  incognito_ipp_proxy_delegate->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const network::mojom::BlindSignedAuthTokenPtr& incognito_result =
      future.Get<network::mojom::BlindSignedAuthTokenPtr>();
  ASSERT_TRUE(incognito_result);
  EXPECT_EQ(incognito_result->token, token);
  EXPECT_LT(incognito_result->expiration, expiration);

  // Ensure that we can still get tokens from the main profile.
  future.Clear();
  ipp_proxy_delegate->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const network::mojom::BlindSignedAuthTokenPtr& second_attempt_result =
      future.Get<network::mojom::BlindSignedAuthTokenPtr>();
  ASSERT_TRUE(second_attempt_result);
  EXPECT_EQ(second_attempt_result->token, token);
  EXPECT_LT(second_attempt_result->expiration, expiration);
}

IN_PROC_BROWSER_TEST_F(IpProtectionConfigProviderBrowserTest,
                       ExpectedReceiverSetStateAfterNetworkServiceCrash) {
  IpProtectionConfigProvider* getter =
      IpProtectionConfigProvider::Get(browser()->profile());
  ASSERT_TRUE(getter);
  ASSERT_EQ(getter->receivers_for_testing().size(), 1U);

  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
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
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();
  incognito_browser->profile()
      ->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();
  // Ensure that any lingering receivers have had time to be removed.
  getter->receivers_for_testing().FlushForTesting();

  ASSERT_EQ(getter->receivers_for_testing().size(), 2U);
}

IN_PROC_BROWSER_TEST_F(IpProtectionConfigProviderBrowserTest,
                       BackoffTimeResetAfterProfileAvailabilityChange) {
  CreateIncognitoNetworkContextAndInterceptors();
  // Simulate logging the user out, which should make the provider indicate that
  // `TryGetAuthTokens()` calls should not be retried on the next request.
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  ClearPrimaryAccount();
#else
  IpProtectionConfigProvider* provider =
      IpProtectionConfigProvider::Get(browser()->profile());
  ASSERT_TRUE(provider);

  // On ChromeOS, `ClearPrimaryAccount()` either isn't supported (Ash) or
  // doesn't seem to work well (Lacros), but we can still test that our
  // implementation works by mocking up the account status change event.
  provider->OnPrimaryAccountChanged(signin::PrimaryAccountChangeEvent(
      signin::PrimaryAccountChangeEvent::State(
          IdentityManager()->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin),
          signin::ConsentLevel::kSignin),
      signin::PrimaryAccountChangeEvent::State()));
#endif

  // Request tokens from both contexts and ensure that the "don't retry"
  // cooldown time is returned. The provider should do this itself, so the
  // interceptors won't be used for this part.
  base::test::TestFuture<network::mojom::BlindSignedAuthTokenPtr,
                         std::optional<base::Time>>
      future;
  main_profile_ipp_proxy_delegate_->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const std::optional<base::Time>& main_profile_first_attempt_result =
      future.Get<std::optional<base::Time>>();
  EXPECT_EQ(main_profile_first_attempt_result.value(), kDontRetry);

  future.Clear();
  incognito_profile_ipp_proxy_delegate_
      ->VerifyIpProtectionConfigGetterForTesting(future.GetCallback());
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
  main_profile_ipp_proxy_delegate_->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const std::optional<base::Time>& main_profile_second_attempt_result =
      future.Get<std::optional<base::Time>>();
  EXPECT_EQ(main_profile_second_attempt_result.value(), kDontRetry);

  future.Clear();
  incognito_profile_ipp_proxy_delegate_
      ->VerifyIpProtectionConfigGetterForTesting(future.GetCallback());
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
          signin::ConsentLevel::kSignin)));
#endif

  // Verify that cooldown timers in the network context have been reset and
  // that we can now request tokens successfully.
  future.Clear();
  main_profile_ipp_proxy_delegate_->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const network::mojom::BlindSignedAuthTokenPtr&
      main_profile_third_attempt_result =
          future.Get<network::mojom::BlindSignedAuthTokenPtr>();
  ASSERT_TRUE(main_profile_third_attempt_result);
  EXPECT_EQ(main_profile_third_attempt_result->token,
            main_profile_auth_token_getter_interceptor_->token());
  EXPECT_LT(main_profile_third_attempt_result->expiration,
            main_profile_auth_token_getter_interceptor_->expiration());

  future.Clear();
  incognito_profile_ipp_proxy_delegate_
      ->VerifyIpProtectionConfigGetterForTesting(future.GetCallback());
  const network::mojom::BlindSignedAuthTokenPtr&
      incognito_profile_third_attempt_result =
          future.Get<network::mojom::BlindSignedAuthTokenPtr>();
  ASSERT_TRUE(incognito_profile_third_attempt_result);
  EXPECT_EQ(incognito_profile_third_attempt_result->token,
            incognito_profile_auth_token_getter_interceptor_->token());
  EXPECT_LT(incognito_profile_third_attempt_result->expiration,
            incognito_profile_auth_token_getter_interceptor_->expiration());

  DestroyIncognitoNetworkContextAndInterceptors();
}

class IpProtectionConfigProviderUserSettingBrowserTest
    : public IpProtectionConfigProviderBrowserTest {
 public:
  IpProtectionConfigProviderUserSettingBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(privacy_sandbox::kIpProtectionV1);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(IpProtectionConfigProviderUserSettingBrowserTest,
                       OnIpProtectionEnabledChanged) {
  CreateIncognitoNetworkContextAndInterceptors();

  IpProtectionConfigProvider* provider =
      IpProtectionConfigProvider::Get(browser()->profile());
  ASSERT_TRUE(provider);

  // Simulate the user disabling the IP Protection setting.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kIpProtectionEnabled,
                                               false);
  provider->OnIpProtectionEnabledChanged();

  // Check that network contexts got notified that IP Protection should be
  // disabled.
  base::test::TestFuture<bool> main_profile_is_enabled_future;
  base::test::TestFuture<bool> incognito_profile_is_enabled_future;

  main_profile_ipp_proxy_delegate_->IsIpProtectionEnabledForTesting(
      main_profile_is_enabled_future.GetCallback());
  incognito_profile_ipp_proxy_delegate_->IsIpProtectionEnabledForTesting(
      incognito_profile_is_enabled_future.GetCallback());

  EXPECT_FALSE(main_profile_is_enabled_future.Get());
  EXPECT_FALSE(incognito_profile_is_enabled_future.Get());

  // Request tokens from both contexts and ensure that the "don't retry"
  // cooldown time is returned.
  base::test::TestFuture<network::mojom::BlindSignedAuthTokenPtr,
                         std::optional<base::Time>>
      main_profile_verification_future;
  base::test::TestFuture<network::mojom::BlindSignedAuthTokenPtr,
                         std::optional<base::Time>>
      incognito_profile_verification_future;

  main_profile_ipp_proxy_delegate_->VerifyIpProtectionConfigGetterForTesting(
      main_profile_verification_future.GetCallback());
  incognito_profile_ipp_proxy_delegate_
      ->VerifyIpProtectionConfigGetterForTesting(
          incognito_profile_verification_future.GetCallback());

  const std::optional<base::Time>& main_profile_first_attempt_result =
      main_profile_verification_future.Get<std::optional<base::Time>>();
  const std::optional<base::Time>& incognito_profile_first_attempt_result =
      incognito_profile_verification_future.Get<std::optional<base::Time>>();

  EXPECT_EQ(main_profile_first_attempt_result.value(), kDontRetry);
  EXPECT_EQ(incognito_profile_first_attempt_result.value(), kDontRetry);

  // Re-enable the setting and ensure that the network contexts got notified
  // accordingly.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kIpProtectionEnabled,
                                               true);
  provider->OnIpProtectionEnabledChanged();

  main_profile_is_enabled_future.Clear();
  incognito_profile_is_enabled_future.Clear();

  main_profile_ipp_proxy_delegate_->IsIpProtectionEnabledForTesting(
      main_profile_is_enabled_future.GetCallback());
  incognito_profile_ipp_proxy_delegate_->IsIpProtectionEnabledForTesting(
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
  main_profile_ipp_proxy_delegate_->VerifyIpProtectionConfigGetterForTesting(
      main_profile_verification_future.GetCallback());
  incognito_profile_ipp_proxy_delegate_
      ->VerifyIpProtectionConfigGetterForTesting(
          incognito_profile_verification_future.GetCallback());

  const network::mojom::BlindSignedAuthTokenPtr&
      main_profile_second_attempt_result =
          main_profile_verification_future
              .Get<network::mojom::BlindSignedAuthTokenPtr>();
  const network::mojom::BlindSignedAuthTokenPtr&
      incognito_profile_second_attempt_result =
          incognito_profile_verification_future
              .Get<network::mojom::BlindSignedAuthTokenPtr>();

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
