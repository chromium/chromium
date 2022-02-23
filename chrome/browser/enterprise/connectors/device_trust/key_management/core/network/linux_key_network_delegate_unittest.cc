// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/linux_key_network_delegate.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr char kFakeBody[] = "fake-body";
constexpr char kFakeDMServerUrl[] =
    "https://example.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";
constexpr char kFakeDMToken[] = "fake-browser-dm-token";

}  // namespace

class LinuxKeyNetworkDelegateTest : public testing::Test {
 public:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  void SetUp() override {
    test_url_loader_factory.Clone(
        remote_url_loader_factory.InitWithNewPipeAndPassReceiver());
    network_delegate = std::make_unique<LinuxKeyNetworkDelegate>(
        std::move(remote_url_loader_factory));
  }

  KeyNetworkDelegate::HttpResponseCode SendRequest() {
    return network_delegate->SendPublicKeyToDmServerSync(
        GURL(kFakeDMServerUrl), kFakeDMToken, kFakeBody);
  }

  void AddResponse(net::HttpStatusCode http_status) {
    test_url_loader_factory.AddResponse(kFakeDMServerUrl, "", http_status);
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory;
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      remote_url_loader_factory;
  std::unique_ptr<LinuxKeyNetworkDelegate> network_delegate;
};

// Tests a successful upload request.
TEST_F(LinuxKeyNetworkDelegateTest, UploadRequest_Success) {
  AddResponse(net::HTTP_OK);
  EXPECT_EQ(200, SendRequest());
}

// Tests two separate sequential requests.
TEST_F(LinuxKeyNetworkDelegateTest, UploadRequest_MultipleSequentialRequests) {
  AddResponse(net::HTTP_NOT_FOUND);
  EXPECT_EQ(404, SendRequest());

  AddResponse(net::HTTP_OK);
  EXPECT_EQ(200, SendRequest());
}

// Tests an upload request when no response headers were returned from
// the url loader.
TEST_F(LinuxKeyNetworkDelegateTest, UploadRequest_EmptyHeader) {
  test_url_loader_factory.AddResponse(
      GURL(kFakeDMServerUrl), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::OK),
      network::TestURLLoaderFactory::Redirects());
  EXPECT_EQ(0, SendRequest());
}

}  // namespace enterprise_connectors
