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
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
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
    getter_->receivers_for_testing().SwapImplForTesting(receiver_id_, this);
  }

  ~IpProtectionConfigGetterInterceptor() override {
    getter_->receivers_for_testing().SwapImplForTesting(receiver_id_, getter_);
  }

  network::mojom::IpProtectionConfigGetter* GetForwardingInterface() override {
    return getter_;
  }

  void TryGetAuthTokens(uint32_t batch_size,
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
    GetForwardingInterface()->TryGetAuthTokens(batch_size, std::move(callback));
  }

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

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

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

  network::mojom::NetworkContext* network_context =
      browser()->profile()->GetDefaultStoragePartition()->GetNetworkContext();

  // To test that the Network Service can successfully request tokens, use the
  // test method on NetworkContext that will have it request tokens and then
  // send back the first token that it receives.
  base::test::TestFuture<network::mojom::BlindSignedAuthTokenPtr,
                         absl::optional<base::Time>>
      future;
  network_context->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const network::mojom::BlindSignedAuthTokenPtr& result =
      future.Get<network::mojom::BlindSignedAuthTokenPtr>();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->token, token);
  EXPECT_EQ(result->expiration, expiration);

  // Now create a new incognito mode profile (with a different associated
  // network context) and see whether we can request tokens from that.
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  network::mojom::NetworkContext* incognito_network_context =
      incognito_browser->profile()
          ->GetDefaultStoragePartition()
          ->GetNetworkContext();
  ASSERT_NE(network_context, incognito_network_context);

  // We need a new interceptor that will intercept messages corresponding to the
  // incognito mode network context's `mojo::Receiver()`.
  ASSERT_EQ(getter->receivers_for_testing().size(), 2U);
  auto incognito_auth_token_getter_interceptor_ =
      std::make_unique<IpProtectionConfigGetterInterceptor>(getter, token,
                                                            expiration);

  // Verify that we can get tokens from the incognito mode profile.
  future.Clear();
  incognito_network_context->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const network::mojom::BlindSignedAuthTokenPtr& incognito_result =
      future.Get<network::mojom::BlindSignedAuthTokenPtr>();
  ASSERT_TRUE(incognito_result);
  EXPECT_EQ(incognito_result->token, token);
  EXPECT_EQ(incognito_result->expiration, expiration);

  // Ensure that we can still get tokens from the main profile.
  future.Clear();
  network_context->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const network::mojom::BlindSignedAuthTokenPtr& second_attempt_result =
      future.Get<network::mojom::BlindSignedAuthTokenPtr>();
  ASSERT_TRUE(second_attempt_result);
  EXPECT_EQ(second_attempt_result->token, token);
  EXPECT_EQ(second_attempt_result->expiration, expiration);
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
  IpProtectionConfigProvider* provider =
      IpProtectionConfigProvider::Get(browser()->profile());
  ASSERT_TRUE(provider);
  ASSERT_EQ(provider->receivers_for_testing().size(), 1U);

  std::string token = "best_token_ever";
  base::Time expiration = base::Time::Now() + base::Seconds(12345);
  auto main_profile_auth_token_getter_interceptor =
      std::make_unique<IpProtectionConfigGetterInterceptor>(
          provider, token, expiration, /*should_intercept=*/false);

  network::mojom::NetworkContext* main_profile_network_context =
      browser()->profile()->GetDefaultStoragePartition()->GetNetworkContext();

  Browser* incognito_profile_browser =
      CreateIncognitoBrowser(browser()->profile());
  ASSERT_EQ(provider->receivers_for_testing().size(), 2U);
  network::mojom::NetworkContext* incognito_profile_network_context =
      incognito_profile_browser->profile()
          ->GetDefaultStoragePartition()
          ->GetNetworkContext();
  ASSERT_NE(main_profile_network_context, incognito_profile_network_context);

  auto incognito_profile_auth_token_getter_interceptor =
      std::make_unique<IpProtectionConfigGetterInterceptor>(
          provider, token, expiration, /*should_intercept=*/false);

// Request tokens from both contexts and ensure that the "don't retry"
// cooldown time is returned. The provider should do this itself, so the
// interceptors won't be used for this part.

// Simulate logging the user out, which should make the provider indicate that
// `TryGetAuthTokens()` calls should not be retried on the next request.
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  ClearPrimaryAccount();
#else
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

  base::Time dont_retry = base::Time::Max();
  base::test::TestFuture<network::mojom::BlindSignedAuthTokenPtr,
                         absl::optional<base::Time>>
      future;
  main_profile_network_context->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const absl::optional<base::Time>& main_profile_first_attempt_result =
      future.Get<absl::optional<base::Time>>();
  EXPECT_EQ(main_profile_first_attempt_result.value(), dont_retry);

  future.Clear();
  incognito_profile_network_context->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const absl::optional<base::Time>& incognito_profile_first_attempt_result =
      future.Get<absl::optional<base::Time>>();
  EXPECT_EQ(incognito_profile_first_attempt_result.value(), dont_retry);

  // Make the interceptors return tokens now so that if the network service
  // isn't respecting the cooldown, tokens will be returned and the test will
  // fail below.
  main_profile_auth_token_getter_interceptor->EnableInterception();
  incognito_profile_auth_token_getter_interceptor->EnableInterception();

  // Run the test again and check that the network service is still in a
  // cooldown phase.
  future.Clear();
  main_profile_network_context->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const absl::optional<base::Time>& main_profile_second_attempt_result =
      future.Get<absl::optional<base::Time>>();
  EXPECT_EQ(main_profile_second_attempt_result.value(), dont_retry);

  future.Clear();
  incognito_profile_network_context->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const absl::optional<base::Time>& incognito_profile_second_attempt_result =
      future.Get<absl::optional<base::Time>>();
  EXPECT_EQ(incognito_profile_second_attempt_result.value(), dont_retry);

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
  main_profile_network_context->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const network::mojom::BlindSignedAuthTokenPtr&
      main_profile_third_attempt_result =
          future.Get<network::mojom::BlindSignedAuthTokenPtr>();
  ASSERT_TRUE(main_profile_third_attempt_result);
  EXPECT_EQ(main_profile_third_attempt_result->token, token);
  EXPECT_EQ(main_profile_third_attempt_result->expiration, expiration);

  future.Clear();
  incognito_profile_network_context->VerifyIpProtectionConfigGetterForTesting(
      future.GetCallback());
  const network::mojom::BlindSignedAuthTokenPtr&
      incognito_profile_third_attempt_result =
          future.Get<network::mojom::BlindSignedAuthTokenPtr>();
  ASSERT_TRUE(incognito_profile_third_attempt_result);
  EXPECT_EQ(incognito_profile_third_attempt_result->token, token);
  EXPECT_EQ(incognito_profile_third_attempt_result->expiration, expiration);
}
