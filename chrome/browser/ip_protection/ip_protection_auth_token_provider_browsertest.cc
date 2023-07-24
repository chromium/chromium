// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ip_protection/ip_protection_auth_token_provider.h"
#include "chrome/browser/ip_protection/ip_protection_auth_token_provider_factory.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "net/base/features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom-test-utils.h"
#include "services/network/public/mojom/network_context.mojom.h"

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

// Helper class to intercept `IpProtectionAuthTokenGetter::TryGetAuthTokens()`
// requests and return a fake token value and a fake expiration value for
// testing.
class IpProtectionAuthTokenGetterInterceptor
    : public network::mojom::IpProtectionAuthTokenGetterInterceptorForTesting {
 public:
  IpProtectionAuthTokenGetterInterceptor(IpProtectionAuthTokenProvider* getter,
                                         std::string token,
                                         base::Time expiration)
      : getter_(getter),
        token_(std::move(token)),
        expiration_(expiration),
        swapped_impl_(getter->receiver_for_testing(), this) {}

  ~IpProtectionAuthTokenGetterInterceptor() override = default;

  network::mojom::IpProtectionAuthTokenGetter* GetForwardingInterface()
      override {
    return getter_;
  }

  void TryGetAuthTokens(uint32_t batch_size,
                        TryGetAuthTokensCallback callback) override {
    // NOTE: We'll ignore batch size and just return one token.
    std::vector<network::mojom::BlindSignedAuthTokenPtr> tokens;
    auto token = network::mojom::BlindSignedAuthToken::New(token_, expiration_);
    tokens.push_back(std::move(token));
    std::move(callback).Run(std::move(tokens), base::Time());
  }

 private:
  raw_ptr<IpProtectionAuthTokenProvider> getter_;
  std::string token_;
  base::Time expiration_;
  mojo::test::ScopedSwapImplForTesting<
      mojo::Receiver<network::mojom::IpProtectionAuthTokenGetter>>
      swapped_impl_;
};
}  // namespace

class IpProtectionAuthTokenProviderBrowserTest : public InProcessBrowserTest {
 public:
  IpProtectionAuthTokenProviderBrowserTest()
      : profile_selections_(IpProtectionAuthTokenProviderFactory::GetInstance(),
                            IpProtectionAuthTokenProviderFactory::
                                CreateProfileSelectionsForTesting()) {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&IpProtectionAuthTokenProviderBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());

    identity_test_environment_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable("user@gmail.com",
                                      signin::ConsentLevel::kSignin);
    identity_test_environment_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

  void TearDownOnMainThread() override {
    identity_test_environment_adaptor_.reset();
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

 private:
  // Note that the order of initialization is important here - we want to set
  // the value of the features before anything else since it's used by the
  // `IpProtectionAuthTokenProviderFactory` logic.
  ScopedIpProtectionFeatureList scoped_ip_protection_feature_list_;
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      profile_selections_;

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(IpProtectionAuthTokenProviderBrowserTest,
                       NetworkServiceCanRequestTokens) {
  IpProtectionAuthTokenProvider* getter =
      IpProtectionAuthTokenProvider::Get(browser()->profile());
  ASSERT_TRUE(getter);

  std::string token = "best_token_ever";
  base::Time expiration = base::Time::Now() + base::Seconds(12345);
  auto auth_token_getter_interceptor_ =
      std::make_unique<IpProtectionAuthTokenGetterInterceptor>(getter, token,
                                                               expiration);

  network::mojom::NetworkContext* network_context =
      browser()->profile()->GetDefaultStoragePartition()->GetNetworkContext();

  // To test that the Network Service can successfully request tokens, use the
  // test method on NetworkContext that will have it request tokens and then
  // send back the first token that it receives.
  base::test::TestFuture<network::mojom::BlindSignedAuthTokenPtr> future;
  network_context->VerifyIpProtectionAuthTokenGetterForTesting(
      future.GetCallback());
  const network::mojom::BlindSignedAuthTokenPtr& result = future.Get();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->token, token);
  EXPECT_EQ(result->expiration, expiration);
}
