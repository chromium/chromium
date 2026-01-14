// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/legion/private_ai_service.h"

#include "base/base64.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/legion/private_ai_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/legion/features.h"
#include "components/legion/phosphor/blind_sign_auth_factory.h"
#include "components/legion/phosphor/mock_blind_sign_auth.h"
#include "components/legion/phosphor/token_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/time/time.h"

namespace legion {

namespace {

class TestBlindSignAuthFactory : public phosphor::BlindSignAuthFactory {
 public:
  TestBlindSignAuthFactory() = default;
  ~TestBlindSignAuthFactory() override = default;

  std::unique_ptr<quiche::BlindSignAuthInterface> CreateBlindSignAuth(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory) override {
    auto bsa = std::make_unique<phosphor::MockBlindSignAuth>();
    bsa_ = bsa.get();
    return bsa;
  }

  phosphor::MockBlindSignAuth* mock_bsa() { return bsa_; }

 private:
  raw_ptr<phosphor::MockBlindSignAuth> bsa_;
};

class TestPrivateAiService : public legion::PrivateAiService {
 public:
  TestPrivateAiService(
      signin::IdentityManager* identity_manager,
      PrefService* pref_service,
      Profile* profile,
      // This factory is owned by `PrivateAiService`, so we need to keep a
      // raw pointer to it.
      TestBlindSignAuthFactory* test_bsa_factory,
      std::unique_ptr<phosphor::BlindSignAuthFactory> bsa_factory);

  ~TestPrivateAiService() override = default;

  legion::phosphor::MockBlindSignAuth* mock_bsa() {
    return test_bsa_factory_->mock_bsa();
  }

 private:
  raw_ptr<TestBlindSignAuthFactory> test_bsa_factory_;
};

TestPrivateAiService::TestPrivateAiService(
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    Profile* profile,
    TestBlindSignAuthFactory* test_bsa_factory,
    std::unique_ptr<phosphor::BlindSignAuthFactory> bsa_factory)
    : legion::PrivateAiService(identity_manager,
                               pref_service,
                               profile,
                               std::move(bsa_factory)),
      test_bsa_factory_(test_bsa_factory) {}

}  // namespace

class PrivateAiServiceBrowserTest : public InProcessBrowserTest {
 public:
  PrivateAiServiceBrowserTest()
      : profile_selections_(
            PrivateAiServiceFactory::GetInstance(),
            PrivateAiServiceFactory::CreateProfileSelectionsForTesting()) {}

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
  base::test::ScopedFeatureList feature_list_{legion::kLegion};
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      profile_selections_;
};

IN_PROC_BROWSER_TEST_F(PrivateAiServiceBrowserTest,
                       IsTokenFetchEnabledAfterAccountChanges) {
  legion::PrivateAiService* host =
      PrivateAiServiceFactory::GetForProfile(profile());
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

  base::RunLoop run_loop;
  // This quit closure is thread-safe and will signal the main loop to wake up
  // after the background thread has processed the mock call.
  test_private_ai_service->mock_bsa()->set_on_get_tokens_callback(
      run_loop.QuitClosure());

  // First call returns null and starts the fetch.
  std::optional<legion::phosphor::BlindSignedAuthToken> token =
      token_manager->GetAuthToken(
          legion::proto::FeatureName::
              FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION);
  EXPECT_FALSE(token.has_value());

  // Wait for the background MockBlindSignAuth call to complete.
  // When this returns, the result task has been posted to the main thread.
  run_loop.Run();

  // Second call should return a token.
  token = token_manager->GetAuthToken(
      legion::proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION);
  ASSERT_TRUE(token.has_value());

  EXPECT_THAT(token->token,
              testing::HasSubstr(base::Base64Encode("test_token")));
}

}  // namespace legion
