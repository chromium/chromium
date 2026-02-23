// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_ai/private_ai_service.h"

#include "base/base64.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/private_ai/private_ai_service_factory.h"
#include "chrome/browser/private_ai/test_private_ai_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/token_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/time/time.h"

namespace private_ai {

class PrivateAiServiceBrowserTest : public InProcessBrowserTest {
 public:
  PrivateAiServiceBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        kPrivateAi, {{kPrivateAiApiKey.name, "test-api-key"}});
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    PrivateAiServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(context);
          auto test_bsa_factory = std::make_unique<TestBlindSignAuthFactory>();
          auto* test_bsa_factory_ptr = test_bsa_factory.get();
          return std::make_unique<TestPrivateAiService>(
              IdentityManagerFactory::GetForProfile(profile),
              profile->GetPrefs(), profile, test_bsa_factory_ptr,
              std::move(test_bsa_factory));
        }));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  }

  void TearDownOnMainThread() override {
    PrivateAiServiceFactory::GetInstance()->SetTestingFactory(profile(), {});

    identity_test_env_adaptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  Profile* profile() { return browser()->profile(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

IN_PROC_BROWSER_TEST_F(PrivateAiServiceBrowserTest,
                       IsTokenFetchEnabledAfterAccountChanges) {
  PrivateAiService* host = PrivateAiServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(host);

  // No account, so token fetch should be disabled.
  EXPECT_FALSE(host->IsTokenFetchEnabled());

  // Sign in.
  identity_test_env()->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  // Token fetch should be enabled now.
  EXPECT_TRUE(host->IsTokenFetchEnabled());

#if !BUILDFLAG(IS_CHROMEOS)
  // Sign out.
  //
  // This functionality is not available on ChromeOS.
  identity_test_env()->ClearPrimaryAccount();

  // Token fetch should be disabled again.
  EXPECT_FALSE(host->IsTokenFetchEnabled());
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

IN_PROC_BROWSER_TEST_F(PrivateAiServiceBrowserTest, GetAuthToken) {
  TestPrivateAiService* test_private_ai_service =
      static_cast<TestPrivateAiService*>(
          PrivateAiServiceFactory::GetForProfile(profile()));
  ASSERT_TRUE(test_private_ai_service);

  auto* token_manager = test_private_ai_service->GetTokenManager();
  ASSERT_TRUE(test_private_ai_service->mock_bsa());

  identity_test_env()->MakePrimaryAccountAvailable("test@example.com",
                                                   signin::ConsentLevel::kSync);

  // Setup tokens.
  std::vector<quiche::BlindSignToken> tokens;
  privacy::ppn::PrivacyPassTokenData privacy_pass_token_data;
  privacy_pass_token_data.set_token(base::Base64Encode("test_token"));
  privacy_pass_token_data.set_encoded_extensions(base::Base64Encode("ext"));

  quiche::BlindSignToken bsa_token;
  bsa_token.token = privacy_pass_token_data.SerializeAsString();
  bsa_token.expiration =
      absl::FromTimeT((base::Time::Now() + base::Hours(1)).ToTimeT());
  tokens.push_back(std::move(bsa_token));
  test_private_ai_service->mock_bsa()->set_tokens(std::move(tokens));

  base::test::TestFuture<std::optional<phosphor::BlindSignedAuthToken>> future;

  // First call is async and starts the fetch.
  token_manager->GetAuthToken(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  // Wait for the async fetch to complete.
  std::optional<phosphor::BlindSignedAuthToken> token = future.Get();
  ASSERT_TRUE(token.has_value());

  EXPECT_EQ(token->token, base::Base64Encode("test_token"));
}

IN_PROC_BROWSER_TEST_F(PrivateAiServiceBrowserTest,
                       GetAuthToken_SyncAfterAsync) {
  TestPrivateAiService* test_private_ai_service =
      static_cast<TestPrivateAiService*>(
          PrivateAiServiceFactory::GetForProfile(profile()));
  ASSERT_TRUE(test_private_ai_service);

  auto* token_manager = test_private_ai_service->GetTokenManager();
  ASSERT_TRUE(test_private_ai_service->mock_bsa());

  identity_test_env()->MakePrimaryAccountAvailable("test@example.com",
                                                   signin::ConsentLevel::kSync);

  // Setup tokens.
  std::vector<quiche::BlindSignToken> tokens;
  for (int i = 0; i < 2; ++i) {
    privacy::ppn::PrivacyPassTokenData privacy_pass_token_data;
    privacy_pass_token_data.set_token(base::Base64Encode("test_token"));
    privacy_pass_token_data.set_encoded_extensions(base::Base64Encode("ext"));

    quiche::BlindSignToken bsa_token;
    bsa_token.token = privacy_pass_token_data.SerializeAsString();
    bsa_token.expiration =
        absl::FromTimeT((base::Time::Now() + base::Hours(1)).ToTimeT());
    tokens.push_back(std::move(bsa_token));
  }
  test_private_ai_service->mock_bsa()->set_tokens(std::move(tokens));

  // First call is async and starts the fetch.
  base::test::TestFuture<std::optional<phosphor::BlindSignedAuthToken>> future;
  token_manager->GetAuthToken(future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());

  // Second call should return a token from the cache.
  base::test::TestFuture<std::optional<phosphor::BlindSignedAuthToken>> future2;
  token_manager->GetAuthToken(future2.GetCallback());
  EXPECT_FALSE(future2.IsReady());
  EXPECT_TRUE(future2.Get().has_value());
}

}  // namespace private_ai
