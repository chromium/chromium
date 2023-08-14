// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ip_protection/blind_sign_http_impl.h"

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
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
  auto request_type = quiche::BlindSignHttpRequestType::kGetInitialData;
  std::string authorization_header = "token";
  std::string body = "body";

  // Set up the response to return from the mock.
  auto head = network::mojom::URLResponseHead::New();
  std::string response_body = "Response body";
  GURL test_url = GURL(
      base::StrCat({BlindSignHttpImpl::kIpProtectionServerUrl,
                    BlindSignHttpImpl::kIpProtectionServerGetInitialDataPath}));
  test_url_loader_factory_.AddResponse(
      test_url, std::move(head), response_body,
      network::URLLoaderCompletionStatus(net::OK));

  base::test::TestFuture<absl::StatusOr<quiche::BlindSignHttpResponse>>
      result_future;
  // Note: We use a lambda expression and `TestFuture::SetValue()` instead of
  // `TestFuture::GetCallback()` to avoid having to convert the
  // `base::OnceCallback` to a `quiche::SignedTokenCallback` (an
  // `absl::AnyInvocable` behind the scenes).
  auto callback =
      [&result_future](absl::StatusOr<quiche::BlindSignHttpResponse> response) {
        result_future.SetValue(std::move(response));
      };

  http_fetcher_->DoRequest(request_type, authorization_header, body,
                           std::move(callback));

  absl::StatusOr<quiche::BlindSignHttpResponse> result = result_future.Get();

  ASSERT_TRUE(result.ok());
  EXPECT_EQ("Response body", result->body());
}

TEST_F(BlindSignHttpImplTest, DoRequestFailsToConnectReturnsFailureStatus) {
  auto request_type = quiche::BlindSignHttpRequestType::kAuthAndSign;
  std::string authorization_header = "token";
  std::string body = "body";

  // Mock no response from Authentication Server (such as a network error).
  std::string response_body;
  auto head = network::mojom::URLResponseHead::New();
  GURL test_url = GURL(
      base::StrCat({BlindSignHttpImpl::kIpProtectionServerUrl,
                    BlindSignHttpImpl::kIpProtectionServerAuthAndSignPath}));
  test_url_loader_factory_.AddResponse(
      test_url, std::move(head), response_body,
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  base::test::TestFuture<absl::StatusOr<quiche::BlindSignHttpResponse>>
      result_future;
  auto callback =
      [&result_future](absl::StatusOr<quiche::BlindSignHttpResponse> response) {
        result_future.SetValue(std::move(response));
      };

  http_fetcher_->DoRequest(request_type, authorization_header, body,
                           std::move(callback));

  absl::StatusOr<quiche::BlindSignHttpResponse> result = result_future.Get();

  EXPECT_EQ("Failed Request to Authentication Server",
            result.status().message());
  EXPECT_EQ(absl::StatusCode::kInternal, result.status().code());
}

TEST_F(BlindSignHttpImplTest, DoRequestHttpFailureStatus) {
  auto request_type = quiche::BlindSignHttpRequestType::kAuthAndSign;
  std::string authorization_header = "token";
  std::string body = "body";

  // Mock a non-200 HTTP response from Authentication Server.
  std::string response_body;
  auto head = network::mojom::URLResponseHead::New();
  GURL test_url = GURL(
      base::StrCat({BlindSignHttpImpl::kIpProtectionServerUrl,
                    BlindSignHttpImpl::kIpProtectionServerAuthAndSignPath}));
  test_url_loader_factory_.AddResponse(test_url.spec(), response_body,
                                       net::HTTP_BAD_REQUEST);

  base::test::TestFuture<absl::StatusOr<quiche::BlindSignHttpResponse>>
      result_future;
  auto callback =
      [&result_future](absl::StatusOr<quiche::BlindSignHttpResponse> response) {
        result_future.SetValue(std::move(response));
      };

  http_fetcher_->DoRequest(request_type, authorization_header, body,
                           std::move(callback));

  absl::StatusOr<quiche::BlindSignHttpResponse> result = result_future.Get();

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(net::HTTP_BAD_REQUEST, result.value().status_code());
}
