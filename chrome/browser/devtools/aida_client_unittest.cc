// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/aida_client.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/network_service.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
}  // namespace

const char kEmail[] = "alice@example.com";
const char kEndpointPath[] = "/foo";
const char kScope[] = "bar";
const char kRequest[] =
    R"({"input": "What does this code do: 1+1", "client": "GENERAL"})";
const char kResponse[] =
    R"([{"textChunk":{"text":"The function `foo()` takes no arguments and returns nothing."}}])";

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

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
            network::NetworkService::GetNetworkServiceForTesting());

    identity_test_env_->MakePrimaryAccountAvailable(
        kEmail, signin::ConsentLevel::kSync);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<network::mojom::NetworkContextClient> network_context_client_;
  scoped_refptr<network::TestSharedURLLoaderFactory> shared_url_loader_factory_;
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  signin::IdentityTestEnvironment* identity_test_env_;
};

class Delegate {
 public:
  Delegate() = default;

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    request_ = request.content;
    authorization_header_ =
        request.headers.at(net::HttpRequestHeaders::kAuthorization);

    auto http_response = std::make_unique<BasicHttpResponse>();
    http_response->set_code(api_response_code_);
    http_response->set_content(api_response_);
    http_response->set_content_type("application/json");
    return http_response;
  }

  void FinishCallback(base::RunLoop* run_loop, const std::string& response) {
    response_ = response;
    if (run_loop) {
      run_loop->Quit();
    }
  }

  std::string request_;
  std::string api_response_ = kResponse;
  net::HttpStatusCode api_response_code_ = net::HTTP_OK;
  std::string response_;
  std::string authorization_header_;
};

constexpr char kOAuthToken[] = "5678";

TEST_F(AidaClientTest, DoesNothingIfNoScope) {
  Delegate delegate;
  test_server_.RegisterRequestHandler(base::BindRepeating(
      &Delegate::HandleRequest, base::Unretained(&delegate)));

  AidaClient aida_client(profile_.get(), shared_url_loader_factory_);
  aida_client.OverrideAidaEndpointAndScopeForTesting("", "");
  aida_client.DoConversation(
      kRequest, base::BindOnce(&Delegate::FinishCallback,
                               base::Unretained(&delegate), nullptr));
  EXPECT_EQ("", delegate.request_);
  EXPECT_EQ(R"([{"error": "AIDA scope is not configured"}])",
            delegate.response_);
}

TEST_F(AidaClientTest, FailsIfNotAuthorized) {
  base::RunLoop run_loop;
  Delegate delegate;

  AidaClient aida_client(profile_.get(), shared_url_loader_factory_);
  aida_client.OverrideAidaEndpointAndScopeForTesting("https://example.com/foo",
                                                     kScope);
  aida_client.DoConversation(
      kRequest, base::BindOnce(&Delegate::FinishCallback,
                               base::Unretained(&delegate), &run_loop));
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  EXPECT_EQ("", delegate.request_);
  EXPECT_EQ(
      R"([{"error": "Cannot get OAuth credentials", "detail": "Request canceled."}])",
      delegate.response_);
}

TEST_F(AidaClientTest, Succeeds) {
  base::RunLoop run_loop;
  Delegate delegate;
  test_server_.RegisterRequestHandler(base::BindRepeating(
      &Delegate::HandleRequest, base::Unretained(&delegate)));

  ASSERT_TRUE(test_server_.Start());

  GURL endpoint_url = test_server_.GetURL(kEndpointPath);
  AidaClient aida_client(profile_.get(), shared_url_loader_factory_);
  aida_client.OverrideAidaEndpointAndScopeForTesting(endpoint_url.spec(),
                                                     kScope);
  aida_client.DoConversation(
      kRequest, base::BindOnce(&Delegate::FinishCallback,
                               base::Unretained(&delegate), &run_loop));
  identity_test_env_
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          kOAuthToken, base::Time::Now() + base::Seconds(10),
          std::string() /*id_token*/, signin::ScopeSet{kScope});
  run_loop.Run();

  EXPECT_EQ(kRequest, delegate.request_);
  EXPECT_EQ(kResponse, delegate.response_);
}

TEST_F(AidaClientTest, ReusesOAuthToken) {
  base::RunLoop run_loop;
  Delegate delegate;
  test_server_.RegisterRequestHandler(base::BindRepeating(
      &Delegate::HandleRequest, base::Unretained(&delegate)));

  ASSERT_TRUE(test_server_.Start());

  GURL endpoint_url = test_server_.GetURL(kEndpointPath);
  AidaClient aida_client(profile_.get(), shared_url_loader_factory_);
  aida_client.OverrideAidaEndpointAndScopeForTesting(endpoint_url.spec(),
                                                     kScope);
  aida_client.DoConversation(
      kRequest, base::BindOnce(&Delegate::FinishCallback,
                               base::Unretained(&delegate), &run_loop));
  identity_test_env_
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          kOAuthToken, base::Time::Now() + base::Seconds(10),
          std::string() /*id_token*/, signin::ScopeSet{kScope});
  run_loop.Run();

  EXPECT_EQ(kRequest, delegate.request_);
  EXPECT_EQ(kResponse, delegate.response_);
  std::string authorization_header = delegate.authorization_header_;

  const char kAnotherRequest[] = "another request";
  const char kAnotherResponse[] = "another response";
  delegate.api_response_ = kAnotherResponse;
  base::RunLoop run_loop2;
  aida_client.DoConversation(
      kAnotherRequest, base::BindOnce(&Delegate::FinishCallback,
                                      base::Unretained(&delegate), &run_loop2));
  run_loop2.Run();
  EXPECT_EQ(kAnotherRequest, delegate.request_);
  EXPECT_EQ(kAnotherResponse, delegate.response_);
  EXPECT_EQ(authorization_header, delegate.authorization_header_);
}

TEST_F(AidaClientTest, RefetchesTokenIfUnauthorized) {
  base::RunLoop run_loop;
  Delegate delegate;
  test_server_.RegisterRequestHandler(base::BindRepeating(
      &Delegate::HandleRequest, base::Unretained(&delegate)));

  ASSERT_TRUE(test_server_.Start());

  GURL endpoint_url = test_server_.GetURL(kEndpointPath);
  AidaClient aida_client(profile_.get(), shared_url_loader_factory_);
  aida_client.OverrideAidaEndpointAndScopeForTesting(endpoint_url.spec(),
                                                     kScope);
  aida_client.DoConversation(
      kRequest, base::BindOnce(&Delegate::FinishCallback,
                               base::Unretained(&delegate), &run_loop));
  identity_test_env_
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          kOAuthToken, base::Time::Now() + base::Seconds(10),
          std::string() /*id_token*/, signin::ScopeSet{kScope});
  run_loop.Run();

  EXPECT_EQ(kRequest, delegate.request_);
  EXPECT_EQ(kResponse, delegate.response_);
  std::string authorization_header = delegate.authorization_header_;

  delegate.api_response_code_ = net::HTTP_UNAUTHORIZED;
  base::RunLoop run_loop2;
  const char kAnotherRequest[] = "another request";
  const char kAnotherResponse[] = "another response";
  const char kAnotherOAuthToken[] = "another token";

  aida_client.DoConversation(
      kAnotherRequest, base::BindOnce(&Delegate::FinishCallback,
                                      base::Unretained(&delegate), &run_loop2));
  identity_test_env_
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          kAnotherOAuthToken, base::Time::Now() + base::Seconds(10),
          std::string() /*id_token*/, signin::ScopeSet{kScope});
  delegate.api_response_code_ = net::HTTP_OK;
  delegate.api_response_ = kAnotherResponse;

  run_loop2.Run();
  EXPECT_EQ(kAnotherRequest, delegate.request_);
  EXPECT_EQ(kAnotherResponse, delegate.response_);
  EXPECT_NE(authorization_header, delegate.authorization_header_);
}
