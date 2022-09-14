// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mojo_key_network_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/http/http_status_code.h"
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

}  // namespace

class MojoKeyNetworkDelegateTest : public testing::Test {
 public:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  void SetUp() override {
    network_delegate = std::make_unique<MojoKeyNetworkDelegate>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
  }

  HttpResponseCode SendRequest() {
    base::test::TestFuture<HttpResponseCode> future;
    network_delegate->SendPublicKeyToDmServer(
        GURL(kFakeDMServerUrl), kFakeDMToken, kFakeBody, future.GetCallback());
    return future.Get();
  }

  void AddResponse(net::HttpStatusCode http_status) {
    test_url_loader_factory_.AddResponse(kFakeDMServerUrl, "", http_status);
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<MojoKeyNetworkDelegate> network_delegate;
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

// Tests an upload request when no response headers were returned from
// the url loader.
TEST_F(MojoKeyNetworkDelegateTest, UploadRequest_EmptyHeader) {
  test_url_loader_factory_.AddResponse(
      GURL(kFakeDMServerUrl), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::OK),
      network::TestURLLoaderFactory::Redirects());
  EXPECT_EQ(0, SendRequest());
}

}  // namespace enterprise_connectors
