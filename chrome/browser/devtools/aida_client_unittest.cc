// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/aida_client.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_context_client.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kEmail[] = "alice@example.com";
const char kEndpointUrlWithPath[] = "https://example.com/foo";
const char kEndpointUrl[] = "https://example.com/";
const char kScope[] = "bar";

class AidaClientTest : public testing::Test {
 public:
  AidaClientTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        profile_(IdentityTestEnvironmentProfileAdaptor::
                     CreateProfileForIdentityTestEnvironment()),
        identity_test_env_adaptor_(
            std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
                profile_.get())),
        identity_test_env_(identity_test_env_adaptor_->identity_test_env()) {
    content::GetNetworkService();
    content::RunAllPendingInMessageLoop(content::BrowserThread::IO);

    identity_test_env_->MakePrimaryAccountAvailable(
        kEmail, signin::ConsentLevel::kSync);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<network::mojom::NetworkContextClient> network_context_client_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  signin::IdentityTestEnvironment* identity_test_env_;
  base::HistogramTester histogram_tester_;
};

class Delegate {
 public:
  Delegate() = default;

  void FinishCallback(
      base::RunLoop* run_loop,
      absl::variant<network::ResourceRequest, std::string> response) {
    response_ = response;
    succeed_ = absl::holds_alternative<network::ResourceRequest>(response);
    if (succeed_) {
      url_ = absl::get<network::ResourceRequest>(response).url;
      ASSERT_TRUE(
          absl::get<network::ResourceRequest>(response).headers.GetHeader(
              net::HttpRequestHeaders::kAuthorization, &authorization_header_));
    } else {
      error_ = absl::get<std::string>(response);
    }
    if (run_loop) {
      run_loop->Quit();
    }
  }

  bool was_called_;
  bool succeed_;
  GURL url_;
  std::string authorization_header_;
  std::string error_;
  absl::variant<network::ResourceRequest, std::string> response_;
};

constexpr char kOAuthToken[] = "5678";

TEST_F(AidaClientTest, DoesNothingIfNoScope) {
  Delegate delegate;

  AidaClient aida_client(profile_.get());
  aida_client.OverrideAidaEndpointAndScopeForTesting("", "");
  aida_client.PrepareRequestOrFail(base::BindOnce(
      &Delegate::FinishCallback, base::Unretained(&delegate), nullptr));
  EXPECT_EQ(R"({"error": "AIDA scope is not configured"})",
            absl::get<std::string>(delegate.response_));
}

TEST_F(AidaClientTest, FailsIfNotAuthorized) {
  base::RunLoop run_loop;
  Delegate delegate;

  AidaClient aida_client(profile_.get());
  aida_client.OverrideAidaEndpointAndScopeForTesting("https://example.com/foo",
                                                     kScope);
  aida_client.PrepareRequestOrFail(base::BindOnce(
      &Delegate::FinishCallback, base::Unretained(&delegate), &run_loop));
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  EXPECT_EQ(
      R"({"error": "Cannot get OAuth credentials", "detail": "Request canceled."})",
      absl::get<std::string>(delegate.response_));
}

TEST_F(AidaClientTest, NotAvailableIfFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(::features::kDevToolsConsoleInsights);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(AidaClient::CanUseAida(profile_.get()));
#else
  EXPECT_FALSE(AidaClient::CanUseAida(profile_.get()));
#endif
  feature_list.Reset();
  feature_list.InitAndDisableFeature(::features::kDevToolsConsoleInsights);
  EXPECT_FALSE(AidaClient::CanUseAida(profile_.get()));
}

TEST_F(AidaClientTest, Succeeds) {
  base::RunLoop run_loop;
  Delegate delegate;

  AidaClient aida_client(profile_.get());
  aida_client.OverrideAidaEndpointAndScopeForTesting(kEndpointUrlWithPath,
                                                     kScope);
  aida_client.PrepareRequestOrFail(base::BindOnce(
      &Delegate::FinishCallback, base::Unretained(&delegate), &run_loop));
  identity_test_env_
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          kOAuthToken, base::Time::Now() + base::Seconds(10),
          std::string() /*id_token*/, signin::ScopeSet{kScope});
  run_loop.Run();

  EXPECT_EQ(kEndpointUrl, delegate.url_);
}

TEST_F(AidaClientTest, ReusesOAuthToken) {
  base::RunLoop run_loop;
  Delegate delegate;

  AidaClient aida_client(profile_.get());
  aida_client.OverrideAidaEndpointAndScopeForTesting(kEndpointUrl, kScope);
  aida_client.PrepareRequestOrFail(base::BindOnce(
      &Delegate::FinishCallback, base::Unretained(&delegate), &run_loop));
  identity_test_env_
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          kOAuthToken, base::Time::Now() + base::Seconds(10),
          std::string() /*id_token*/, signin::ScopeSet{kScope});
  run_loop.Run();

  EXPECT_TRUE(delegate.succeed_);
  std::string authorization_header = delegate.authorization_header_;

  base::RunLoop run_loop2;
  aida_client.PrepareRequestOrFail(base::BindOnce(
      &Delegate::FinishCallback, base::Unretained(&delegate), &run_loop2));
  run_loop2.Run();
  EXPECT_TRUE(
      absl::holds_alternative<network::ResourceRequest>(delegate.response_));
  std::string another_authorization_header;
  EXPECT_EQ(authorization_header, delegate.authorization_header_);
}

TEST_F(AidaClientTest, RefetchesTokenWhenExpired) {
  base::RunLoop run_loop;
  Delegate delegate;

  AidaClient aida_client(profile_.get());
  aida_client.OverrideAidaEndpointAndScopeForTesting(kEndpointUrl, kScope);
  aida_client.PrepareRequestOrFail(base::BindOnce(
      &Delegate::FinishCallback, base::Unretained(&delegate), &run_loop));
  identity_test_env_
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          kOAuthToken, base::Time::Now() - base::Seconds(10),
          std::string() /*id_token*/, signin::ScopeSet{kScope});
  run_loop.Run();

  EXPECT_TRUE(
      absl::holds_alternative<network::ResourceRequest>(delegate.response_));
  std::string authorization_header = delegate.authorization_header_;

  base::RunLoop run_loop2;
  const char kAnotherOAuthToken[] = "another token";

  aida_client.PrepareRequestOrFail(base::BindOnce(
      &Delegate::FinishCallback, base::Unretained(&delegate), &run_loop2));
  identity_test_env_
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          kAnotherOAuthToken, base::Time::Now() + base::Seconds(10),
          std::string() /*id_token*/, signin::ScopeSet{kScope});

  run_loop2.Run();
  EXPECT_TRUE(delegate.succeed_);
  EXPECT_NE(authorization_header, delegate.authorization_header_);
}
