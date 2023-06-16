// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ip_protection/blind_sign_http_impl.h"

#include "base/test/task_environment.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

class BlindSignHttpImplTest : public testing::Test {
 protected:
  void SetUp() override {
    http_fetcher_ = std::make_unique<BlindSignHttpImpl>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<BlindSignHttpImpl> http_fetcher_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

TEST_F(BlindSignHttpImplTest, DoRequestSendsCorrectRequest) {
  std::string path_and_query = "/api/test";
  std::string authorization_header = "token";
  std::string body = "body";

  // Set up the response to return from the mock.
  auto head = network::mojom::URLResponseHead::New();
  std::string response_body = "Response body";
  test_url_loader_factory_.AddResponse(
      GURL(path_and_query), std::move(head), response_body,
      network::URLLoaderCompletionStatus(net::OK));

  absl::StatusOr<quiche::BlindSignHttpResponse> result;

  auto callback =
      [&result](absl::StatusOr<quiche::BlindSignHttpResponse> response) {
        result = std::move(response);
      };

  http_fetcher_->DoRequest(path_and_query, authorization_header, body,
                           std::move(callback));

  // Use RunUntilIdle to make sure the response has been processed.
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(result.ok());
  EXPECT_EQ("Response body", result->body());
}

TEST_F(BlindSignHttpImplTest, DoRequestFailsToConnectReturnsFailureStatus) {
  std::string path_and_query = "/api/test2";
  std::string authorization_header = "token";
  std::string body = "body";

  // Mock no response from Authentication Server.
  std::string response_body;
  auto head = network::mojom::URLResponseHead::New();
  test_url_loader_factory_.AddResponse(
      GURL(path_and_query), std::move(head), response_body,
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  absl::StatusOr<quiche::BlindSignHttpResponse> result;
  auto callback =
      [&result](absl::StatusOr<quiche::BlindSignHttpResponse> response) {
        result = std::move(response);
      };

  http_fetcher_->DoRequest(path_and_query, authorization_header, body,
                           std::move(callback));

  // Use RunUntilIdle to make sure the response has been processed.
  task_environment_.RunUntilIdle();

  EXPECT_EQ("Failed Request to Authentication Server",
            result.status().message());
  EXPECT_EQ(absl::StatusCode::kInternal, result.status().code());
}
