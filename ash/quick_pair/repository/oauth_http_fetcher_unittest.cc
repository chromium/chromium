// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/oauth_http_fetcher.h"

#include "ash/quick_pair/common/mock_quick_pair_browser_delegate.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kBody[] = "body";
constexpr char kTestUrl[] = "http://www.test.com/";
constexpr char kTestScope[] = "http://www.test.com/scope";
const net::PartialNetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefinePartialNetworkTrafficAnnotation("test_request",
                                               "oauth2_api_call_flow",
                                               R"(
      semantics {
          sender: "Test Request"
        description:
            "Test request."
        trigger:
          "Test request."
        data: "Test Request."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
          cookies_allowed: NO
          setting:
            "Test Request."
        })");

}  // namespace

namespace ash {
namespace quick_pair {

class OAuthHttpFetcherTest : public testing::Test {
 public:
  OAuthHttpFetcherTest() : identity_test_env_(&url_loader_factory_) {
    identity_test_env_.MakePrimaryAccountAvailable("1@mail.com",
                                                   signin::ConsentLevel::kSync);
  }

  void SetUp() override {
    http_fetcher_ =
        std::make_unique<OAuthHttpFetcher>(kTrafficAnnotation, kTestScope);
    browser_delegate_ = std::make_unique<MockQuickPairBrowserDelegate>();
    ON_CALL(*browser_delegate_, GetURLLoaderFactory())
        .WillByDefault(
            testing::Return(url_loader_factory_.GetSafeWeakWrapper()));
    ON_CALL(*browser_delegate_, GetIdentityManager())
        .WillByDefault(testing::Return(identity_test_env_.identity_manager()));
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  void TearDown() override { url_loader_factory_.ClearResponses(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<OAuthHttpFetcher> http_fetcher_;
  std::unique_ptr<MockQuickPairBrowserDelegate> browser_delegate_;
  network::TestURLLoaderFactory url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(OAuthHttpFetcherTest, ExecuteGetRequest_Success) {
  GURL url(kTestUrl);
  std::string body(kBody);
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(""));
  head->headers->GetMimeType(&head->mime_type);
  network::URLLoaderCompletionStatus status(net::Error::OK);
  status.decoded_body_length = body.size();
  url_loader_factory_.AddResponse(url, std::move(head), body, status);

  http_fetcher_->ExecuteGetRequest(
      url, base::BindOnce([](std::unique_ptr<std::string> response,
                             std::unique_ptr<FastPairHttpResult> result) {
        ASSERT_EQ(kBody, *response);
        ASSERT_TRUE(result->IsSuccess());
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(OAuthHttpFetcherTest, ExecuteGetRequest_Failure) {
  url_loader_factory_.AddResponse(kTestUrl, "",
                                  net::HTTP_INTERNAL_SERVER_ERROR);

  http_fetcher_->ExecuteGetRequest(
      GURL(kTestUrl),
      base::BindOnce([](std::unique_ptr<std::string> response,
                        std::unique_ptr<FastPairHttpResult> result) {
        ASSERT_EQ(nullptr, response);
        ASSERT_FALSE(result->IsSuccess());
        ASSERT_EQ(result->http_response_error(),
                  net::HTTP_INTERNAL_SERVER_ERROR);
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(OAuthHttpFetcherTest, ExecuteGetRequest_MultipleCalls) {
  url_loader_factory_.AddResponse(kTestUrl, "",
                                  net::HTTP_INTERNAL_SERVER_ERROR);

  http_fetcher_->ExecuteGetRequest(
      GURL(kTestUrl),
      base::BindOnce([](std::unique_ptr<std::string> response,
                        std::unique_ptr<FastPairHttpResult> result) {
        ASSERT_EQ(nullptr, response);
        ASSERT_FALSE(result->IsSuccess());
        ASSERT_EQ(result->http_response_error(),
                  net::HTTP_INTERNAL_SERVER_ERROR);
      }));
  EXPECT_DEATH(
      http_fetcher_->ExecuteGetRequest(GURL(kTestUrl), base::DoNothing()), "");
  task_environment_.RunUntilIdle();
}

TEST_F(OAuthHttpFetcherTest, ExecuteGetRequest_NoToken) {
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);
  url_loader_factory_.AddResponse(kTestUrl, "",
                                  net::HTTP_INTERNAL_SERVER_ERROR);
  http_fetcher_->ExecuteGetRequest(
      GURL(kTestUrl),
      base::BindOnce([](std::unique_ptr<std::string> response,
                        std::unique_ptr<FastPairHttpResult> result) {
        ASSERT_EQ(nullptr, response);
        ASSERT_EQ(nullptr, result);
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(OAuthHttpFetcherTest, ExecuteGetRequest_NoUrlFactory) {
  ON_CALL(*browser_delegate_, GetURLLoaderFactory())
      .WillByDefault(testing::Return(nullptr));
  url_loader_factory_.AddResponse(kTestUrl, "",
                                  net::HTTP_INTERNAL_SERVER_ERROR);
  http_fetcher_->ExecuteGetRequest(
      GURL(kTestUrl),
      base::BindOnce([](std::unique_ptr<std::string> response,
                        std::unique_ptr<FastPairHttpResult> result) {
        ASSERT_EQ(nullptr, response);
        ASSERT_EQ(nullptr, result);
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(OAuthHttpFetcherTest, ExecuteGetRequest_NoIdentityManager) {
  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(nullptr));

  EXPECT_DEATH(
      http_fetcher_->ExecuteGetRequest(GURL(kTestUrl), base::DoNothing()), "");

  task_environment_.RunUntilIdle();
}

TEST_F(OAuthHttpFetcherTest, ExecuteGetRequest_MultipleRaceCondition) {
  http_fetcher_->ExecuteGetRequest(GURL(kTestUrl), base::DoNothing());
  EXPECT_DEATH(
      http_fetcher_->ExecuteGetRequest(GURL(kTestUrl), base::DoNothing()), "");
  task_environment_.RunUntilIdle();
}

TEST_F(OAuthHttpFetcherTest, ExecutePostRequest_Success) {
  GURL url(kTestUrl);
  std::string body(kBody);
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(""));
  head->headers->GetMimeType(&head->mime_type);
  network::URLLoaderCompletionStatus status(net::Error::OK);
  status.decoded_body_length = body.size();
  url_loader_factory_.AddResponse(url, std::move(head), body, status);

  http_fetcher_->ExecutePostRequest(
      url, kBody,
      base::BindOnce([](std::unique_ptr<std::string> response,
                        std::unique_ptr<FastPairHttpResult> result) {
        ASSERT_TRUE(result->IsSuccess());
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(OAuthHttpFetcherTest, ExecuteDeleteRequest_Success) {
  GURL url(kTestUrl);
  std::string body(kBody);
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(""));
  head->headers->GetMimeType(&head->mime_type);
  network::URLLoaderCompletionStatus status(net::Error::OK);
  status.decoded_body_length = body.size();
  url_loader_factory_.AddResponse(url, std::move(head), body, status);

  http_fetcher_->ExecuteDeleteRequest(
      url, base::BindOnce([](std::unique_ptr<std::string> response,
                             std::unique_ptr<FastPairHttpResult> result) {
        ASSERT_TRUE(result->IsSuccess());
      }));
  task_environment_.RunUntilIdle();
}

}  // namespace quick_pair
}  // namespace ash
