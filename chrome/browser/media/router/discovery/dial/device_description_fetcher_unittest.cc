// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/media/router/discovery/dial/device_description_fetcher.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "net/base/ip_address.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::HasSubstr;
using testing::NiceMock;

namespace media_router {

class DeviceDescriptionFetcherTest : public testing::Test {
 public:
  DeviceDescriptionFetcherTest() : url_("http://127.0.0.1/description.xml") {}
  DeviceDescriptionFetcherTest(DeviceDescriptionFetcherTest&) = delete;
  DeviceDescriptionFetcherTest& operator=(DeviceDescriptionFetcherTest&) =
      delete;

  void StartRequest() {
    DialDeviceData device_data;
    device_data.set_device_description_url(url_);
    device_data.set_ip_address(net::IPAddress::IPv4Localhost());
    description_fetcher_ = std::make_unique<TestDeviceDescriptionFetcher>(
        device_data,
        base::BindOnce(&DeviceDescriptionFetcherTest::OnSuccess,
                       base::Unretained(this)),
        base::BindOnce(&DeviceDescriptionFetcherTest::OnError,
                       base::Unretained(this)),
        &loader_factory_);
    description_fetcher_->Start();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  MOCK_METHOD1(OnSuccess, void(const DialDeviceDescriptionData&));
  MOCK_METHOD1(OnError, void(const std::string&));

  base::test::TaskEnvironment environment_;
  const GURL url_;
  network::TestURLLoaderFactory loader_factory_;
  std::unique_ptr<TestDeviceDescriptionFetcher> description_fetcher_;
};

TEST_F(DeviceDescriptionFetcherTest, FetchSuccessful) {
  std::string body("<xml>description</xml>");
  EXPECT_CALL(*this, OnSuccess(DialDeviceDescriptionData(
                         body, GURL("http://127.0.0.1/apps"))));
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  head->headers->AddHeader("Application-URL", "http://127.0.0.1/apps");
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = body.size();
  loader_factory_.AddResponse(url_, std::move(head), body, status);
  StartRequest();
}

TEST_F(DeviceDescriptionFetcherTest, FetchSuccessfulAppUrlWithTrailingSlash) {
  std::string body("<xml>description</xml>");
  EXPECT_CALL(*this, OnSuccess(DialDeviceDescriptionData(
                         body, GURL("http://127.0.0.1/apps"))));
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  head->headers->AddHeader("Application-URL", "http://127.0.0.1/apps/");
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = body.size();
  loader_factory_.AddResponse(url_, std::move(head), body, status);
  StartRequest();
}

TEST_F(DeviceDescriptionFetcherTest, FetchFailsOnMissingDescription) {
  EXPECT_CALL(*this, OnError(HasSubstr("404")));
  loader_factory_.AddResponse(
      url_, network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
  StartRequest();
}

TEST_F(DeviceDescriptionFetcherTest, FetchFailsOnMissingAppUrl) {
  std::string body("<xml>description</xml>");
  EXPECT_CALL(*this, OnError(HasSubstr("Missing or empty Application-URL:")));
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = body.size();
  loader_factory_.AddResponse(url_, network::mojom::URLResponseHead::New(),
                              body, status);
  StartRequest();
}

TEST_F(DeviceDescriptionFetcherTest, FetchFailsOnEmptyAppUrl) {
  EXPECT_CALL(*this, OnError(HasSubstr("Missing or empty Application-URL:")));
  std::string body("<xml>description</xml>");
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  head->headers->AddHeader("Application-URL", "");
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = body.size();
  loader_factory_.AddResponse(url_, std::move(head), body, status);
  StartRequest();
}

TEST_F(DeviceDescriptionFetcherTest, FetchFailsOnInvalidAppUrl) {
  EXPECT_CALL(*this, OnError(HasSubstr("Invalid Application-URL:")));
  std::string body("<xml>description</xml>");
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  head->headers->AddHeader("Application-URL", "http://www.example.com");
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = body.size();
  loader_factory_.AddResponse(url_, std::move(head), body, status);
  StartRequest();
}

TEST_F(DeviceDescriptionFetcherTest, FetchFailsOnEmptyDescription) {
  EXPECT_CALL(*this, OnError(HasSubstr("Missing or empty response")));
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  head->headers->AddHeader("Application-URL", "http://127.0.0.1/apps");

  loader_factory_.AddResponse(url_, std::move(head), "",
                              network::URLLoaderCompletionStatus());
  StartRequest();
}

TEST_F(DeviceDescriptionFetcherTest, FetchFailsOnBadDescription) {
  EXPECT_CALL(*this, OnError(HasSubstr("Invalid response encoding")));
  std::string body("\xfc\x9c\xbf\x80\xbf\x80");
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  head->headers->AddHeader("Application-URL", "http://127.0.0.1/apps");
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = body.size();
  loader_factory_.AddResponse(url_, std::move(head), body, status);
  StartRequest();
}

}  // namespace media_router
