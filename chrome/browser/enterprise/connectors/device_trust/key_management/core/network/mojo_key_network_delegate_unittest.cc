// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mojo_key_network_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

using HttpResponseCode = KeyNetworkDelegate::HttpResponseCode;

namespace {

constexpr char kFakeBody[] = "fake-body";
constexpr char kFakeDMServerUrl[] =
    "https://example.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";
constexpr char kFakeDMToken[] = "fake-browser-dm-token";
constexpr HttpResponseCode kSuccessCode = 200;
constexpr HttpResponseCode kHardFailureCode = 404;

bool CompareResponseProtos(
    const enterprise_management::DeviceManagementResponse& response1,
    const enterprise_management::DeviceManagementResponse& response2) {
  return response1.SerializeAsString() == response2.SerializeAsString();
}

}  // namespace

class MojoKeyNetworkDelegateTest : public testing::Test {
 protected:
  void SetUp() override {
    network_delegate_ = std::make_unique<MojoKeyNetworkDelegate>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
  }

  HttpResponseCode SendRequest() {
    base::test::TestFuture<HttpResponseCode> future;
    network_delegate_->SendPublicKeyToDmServer(
        GURL(kFakeDMServerUrl), kFakeDMToken, kFakeBody, future.GetCallback());
    return future.Get();
  }

  void AddResponse(net::HttpStatusCode http_status,
                   const std::string& body = "") {
    test_url_loader_factory_.AddResponse(kFakeDMServerUrl, body, http_status);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<MojoKeyNetworkDelegate> network_delegate_;
};

// Tests a successful upload request.
TEST_F(MojoKeyNetworkDelegateTest, UploadRequest_Success) {
  AddResponse(net::HTTP_OK);
  EXPECT_EQ(kSuccessCode, SendRequest());
}

// Tests two separate sequential requests.
TEST_F(MojoKeyNetworkDelegateTest, UploadRequest_MultipleSequentialRequests) {
  AddResponse(net::HTTP_NOT_FOUND);
  EXPECT_EQ(kHardFailureCode, SendRequest());

  AddResponse(net::HTTP_OK);
  EXPECT_EQ(kSuccessCode, SendRequest());
}

// Tests two parallel requests.
TEST_F(MojoKeyNetworkDelegateTest, UploadRequest_ParallelSuccess) {
  base::test::TestFuture<HttpResponseCode> first_future;
  network_delegate_->SendPublicKeyToDmServer(GURL(kFakeDMServerUrl),
                                             kFakeDMToken, kFakeBody,
                                             first_future.GetCallback());

  base::test::TestFuture<HttpResponseCode> second_future;
  network_delegate_->SendPublicKeyToDmServer(GURL(kFakeDMServerUrl),
                                             kFakeDMToken, kFakeBody,
                                             second_future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 2);

  auto* first_pending_request = test_url_loader_factory_.GetPendingRequest(0U);
  auto* second_pending_request = test_url_loader_factory_.GetPendingRequest(1U);

  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      first_pending_request,
      network::CreateURLResponseHead(net::HTTP_NOT_FOUND), "",
      network::URLLoaderCompletionStatus(net::OK));
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      second_pending_request, network::CreateURLResponseHead(net::HTTP_OK), "",
      network::URLLoaderCompletionStatus(net::OK));

  EXPECT_EQ(first_future.Get(), kHardFailureCode);
  EXPECT_EQ(second_future.Get(), kSuccessCode);
}

// Tests an upload request when no response headers were returned from
// the url loader.
TEST_F(MojoKeyNetworkDelegateTest, UploadRequest_EmptyHeader) {
  test_url_loader_factory_.AddResponse(
      GURL(kFakeDMServerUrl), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::OK),
      network::TestURLLoaderFactory::Redirects());
  EXPECT_EQ(0, SendRequest());
}

TEST_F(MojoKeyNetworkDelegateTest, SendRequest_Success) {
  enterprise_management::DeviceManagementRequest request_body;
  request_body.mutable_browser_public_key_upload_request()
      ->set_provision_certificate(true);
  const auto& request_body_string = request_body.SerializeAsString();

  enterprise_management::DeviceManagementResponse response_body;
  response_body.mutable_browser_public_key_upload_response()
      ->set_pem_encoded_certificate("1234");
  const auto& response_body_string = response_body.SerializeAsString();

  base::test::TestFuture<
      int, std::optional<enterprise_management::DeviceManagementResponse>>
      test_future;
  network_delegate_->SendRequest(GURL(kFakeDMServerUrl), kFakeDMToken,
                                 request_body, test_future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0U);
  ASSERT_TRUE(pending_request);

  // Verify the request properties.
  const auto& resource_request = pending_request->request;
  EXPECT_EQ(network::GetUploadData(resource_request), request_body_string);
  EXPECT_EQ(resource_request.method, "POST");
  EXPECT_EQ(resource_request.url, GURL(kFakeDMServerUrl));

  // Send the response.
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      pending_request, network::CreateURLResponseHead(net::HTTP_OK),
      response_body_string, network::URLLoaderCompletionStatus(net::OK));

  auto [response_code, response] = test_future.Take();

  EXPECT_EQ(response_code, kSuccessCode);
  ASSERT_TRUE(response.has_value());
  EXPECT_TRUE(CompareResponseProtos(response.value(), response_body));
}

TEST_F(MojoKeyNetworkDelegateTest, SendRequest_Fail) {
  enterprise_management::DeviceManagementRequest request_body;
  request_body.mutable_browser_public_key_upload_request()
      ->set_provision_certificate(true);

  AddResponse(net::HTTP_NOT_FOUND);

  base::test::TestFuture<
      int, std::optional<enterprise_management::DeviceManagementResponse>>
      test_future;
  network_delegate_->SendRequest(GURL(kFakeDMServerUrl), kFakeDMToken,
                                 request_body, test_future.GetCallback());

  auto [response_code, response] = test_future.Take();

  EXPECT_EQ(response_code, kHardFailureCode);
  EXPECT_FALSE(response.has_value());
}

}  // namespace enterprise_connectors
