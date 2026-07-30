// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_request_helper.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

class RemoteActorRequestTest : public testing::Test {
 public:
  RemoteActorRequestTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

TEST_F(RemoteActorRequestTest, RequestSuccess) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::RunLoop run_loop;
  bool callback_called = false;
  bool request_success = false;

  RemoteActorRequest request(
      identity_test_env_.identity_manager(), GURL("https://example.com/api"),
      "POST", "post_data",
      signin::OAuthConsumerId::kRemoteActorLoginCredentialsService,
      net::DefineNetworkTrafficAnnotation("test", "test"),
      test_shared_loader_factory_,
      base::BindLambdaForTesting([&](RemoteActorRequest* req, bool success) {
        callback_called = true;
        request_success = success;
        run_loop.Quit();
      }));

  request.Start();

  // 1. Respond to Access Token request
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "test_token", base::Time::Max());

  // 2. Verify URL Loader request is created with correct headers
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);
  EXPECT_EQ(pending_request->request.url, GURL("https://example.com/api"));
  EXPECT_EQ(pending_request->request.method, "POST");

  EXPECT_EQ(pending_request->request.headers.GetHeader(
                net::HttpRequestHeaders::kAuthorization),
            "Bearer test_token");

  EXPECT_EQ(pending_request->request.headers.GetHeader("X-Developer-Key"),
            GaiaUrls::GetInstance()->oauth2_chrome_client_id());

  EXPECT_EQ(pending_request->request.headers.GetHeader(
                net::HttpRequestHeaders::kAccept),
            "application/json");

  std::string upload_body = network::GetUploadData(pending_request->request);
  EXPECT_EQ(upload_body, "post_data");

  // Respond with 200 OK
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "{}");

  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(request_success);
}

TEST_F(RemoteActorRequestTest, AccessTokenFetchFailure) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::RunLoop run_loop;
  bool callback_called = false;
  bool request_success = false;

  RemoteActorRequest request(
      identity_test_env_.identity_manager(), GURL("https://example.com/api"),
      "GET", "", signin::OAuthConsumerId::kRemoteActorLoginCredentialsService,
      net::DefineNetworkTrafficAnnotation("test", "test"),
      test_shared_loader_factory_,
      base::BindLambdaForTesting([&](RemoteActorRequest* req, bool success) {
        callback_called = true;
        request_success = success;
        run_loop.Quit();
      }));

  request.Start();

  // Fail to fetch token
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));

  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(request_success);
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}

TEST_F(RemoteActorRequestTest, HttpError) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::RunLoop run_loop;
  bool callback_called = false;
  bool request_success = false;

  RemoteActorRequest request(
      identity_test_env_.identity_manager(), GURL("https://example.com/api"),
      "GET", "", signin::OAuthConsumerId::kRemoteActorLoginCredentialsService,
      net::DefineNetworkTrafficAnnotation("test", "test"),
      test_shared_loader_factory_,
      base::BindLambdaForTesting([&](RemoteActorRequest* req, bool success) {
        callback_called = true;
        request_success = success;
        run_loop.Quit();
      }));

  request.Start();

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "test_token", base::Time::Max());

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);

  // Respond with 500 Internal Server Error (first attempt)
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "", net::HTTP_INTERNAL_SERVER_ERROR);

  // Since kMaxRetries = 1, SimpleURLLoader will retry once on 5xx.
  // We need to respond to the retry request as well.
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* retry_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(retry_request);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      retry_request->request.url.spec(), "", net::HTTP_INTERNAL_SERVER_ERROR);

  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(request_success);
}

}  // namespace password_manager
