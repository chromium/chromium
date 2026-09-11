// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/oauth_http_fetcher.h"

#include "ash/quick_pair/common/mock_quick_pair_browser_delegate.h"
#include "base/byte_size.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/signin/fake_identity_manager_provider.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kBody[] = "body";
constexpr char kTestUrl[] = "http://www.test.com/";
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

namespace {

constexpr auto kTestAccountId =
    AccountId::Literal::FromUserEmailGaiaId("1@mail.com",
                                            GaiaId::Literal("fake-gaia-id"));

}  // namespace

class OAuthHttpFetcherTest : public testing::Test {
 public:
  OAuthHttpFetcherTest()
      : account_id_(kTestAccountId), identity_test_env_(&url_loader_factory_) {
    identity_test_env_.MakePrimaryAccountAvailable(account_id_.GetUserEmail(),
                                                   signin::ConsentLevel::kSync);
  }

  void SetUp() override {
    http_fetcher_ = std::make_unique<OAuthHttpFetcher>(
        kTrafficAnnotation, signin::OAuthConsumerId::kFastPair);
    browser_delegate_ = std::make_unique<MockQuickPairBrowserDelegate>();
    ON_CALL(*browser_delegate_, GetURLLoaderFactory())
        .WillByDefault(
            testing::Return(url_loader_factory_.GetSafeWeakWrapper()));
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
    identity_manager_provider_->SetIdentityManagerForAccount(
        account_id_, identity_test_env_.identity_manager());

    // OAuthHttpFetcher looks up the active session's AccountId to find an
    // IdentityManager (see TODO(crbug.com/546860700) on that lookup).
    // ash::test::UserSessionTestEnvironment owns the UserManager/SessionManager
    // pair that lookup goes through; both are standalone singletons
    // independent of ash::Shell, so this test doesn't need to bring up the
    // rest of Ash just to satisfy it.
    ash::test::UserSessionTestEnvironment::RegisterLocalStatePrefs(
        local_state_.registry());
    user_session_test_environment_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(&local_state_);
    CHECK(user_session_test_environment_->AddRegularUser(account_id_));
    user_session_test_environment_->LogIn(account_id_);
  }

  void TearDown() override {
    url_loader_factory_.ClearResponses();
    user_session_test_environment_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  AccountId account_id_;
  TestingPrefServiceSimple local_state_;
  // Declared after `local_state_` so it's destroyed first: it owns the
  // UserManager, which holds `local_state_`.
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  std::unique_ptr<OAuthHttpFetcher> http_fetcher_;
  std::unique_ptr<MockQuickPairBrowserDelegate> browser_delegate_;
  network::TestURLLoaderFactory url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  // Declared after `identity_test_env_` so it's destroyed first: it holds
  // non-owning pointers into the IdentityManager `identity_test_env_` owns,
  // and must not outlive it.
  std::unique_ptr<FakeIdentityManagerProvider> identity_manager_provider_ =
      std::make_unique<FakeIdentityManagerProvider>();
};

TEST_F(OAuthHttpFetcherTest, ExecuteGetRequest_Success) {
  GURL url(kTestUrl);
  std::string body(kBody);
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(""));
  head->headers->GetMimeType(&head->mime_type);
  network::URLLoaderCompletionStatus status(net::Error::OK);
  status.decoded_body_length = base::ByteSize(body.size());
  url_loader_factory_.AddResponse(url, std::move(head), body, status);

  http_fetcher_->ExecuteGetRequest(
      url, base::BindOnce([](std::optional<std::string> response,
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
      base::BindOnce([](std::optional<std::string> response,
                        std::unique_ptr<FastPairHttpResult> result) {
        ASSERT_EQ(std::nullopt, response);
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
      base::BindOnce([](std::optional<std::string> response,
                        std::unique_ptr<FastPairHttpResult> result) {
        ASSERT_EQ(std::nullopt, response);
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
      base::BindOnce([](std::optional<std::string> response,
                        std::unique_ptr<FastPairHttpResult> result) {
        ASSERT_EQ(std::nullopt, response);
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
      base::BindOnce([](std::optional<std::string> response,
                        std::unique_ptr<FastPairHttpResult> result) {
        ASSERT_EQ(std::nullopt, response);
        ASSERT_EQ(nullptr, result);
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(OAuthHttpFetcherTest, ExecuteGetRequest_NoIdentityManager) {
  identity_manager_provider_->SetIdentityManagerForAccount(account_id_,
                                                           nullptr);

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
  status.decoded_body_length = base::ByteSize(body.size());
  url_loader_factory_.AddResponse(url, std::move(head), body, status);

  http_fetcher_->ExecutePostRequest(
      url, kBody,
      base::BindOnce([](std::optional<std::string> response,
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
  status.decoded_body_length = base::ByteSize(body.size());
  url_loader_factory_.AddResponse(url, std::move(head), body, status);

  http_fetcher_->ExecuteDeleteRequest(
      url, base::BindOnce([](std::optional<std::string> response,
                             std::unique_ptr<FastPairHttpResult> result) {
        ASSERT_TRUE(result->IsSuccess());
      }));
  task_environment_.RunUntilIdle();
}

}  // namespace quick_pair
}  // namespace ash
