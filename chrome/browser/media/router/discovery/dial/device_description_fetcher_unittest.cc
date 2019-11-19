// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/media/router/discovery/dial/device_description_fetcher.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::HasSubstr;

namespace media_router {

class TestDeviceDescriptionFetcher : public DeviceDescriptionFetcher {
 public:
  TestDeviceDescriptionFetcher(
      const GURL& device_description_url,
      base::OnceCallback<void(const DialDeviceDescriptionData&)> success_cb,
      base::OnceCallback<void(const std::string&)> error_cb,
      network::TestURLLoaderFactory* factory)
      : DeviceDescriptionFetcher(device_description_url,
                                 std::move(success_cb),
                                 std::move(error_cb)),
        factory_(factory) {}
  ~TestDeviceDescriptionFetcher() override = default;

  void Start() override {
    fetcher_ = std::make_unique<TestDialURLFetcher>(
        base::BindOnce(&DeviceDescriptionFetcher::ProcessResponse,
                       base::Unretained(this)),
        base::BindOnce(&DeviceDescriptionFetcher::ReportError,
                       base::Unretained(this)),
        factory_);
    fetcher_->Get(device_description_url_);
  }

 private:
  network::TestURLLoaderFactory* const factory_;
};

class DeviceDescriptionFetcherTest : public testing::Test {
 public:
  DeviceDescriptionFetcherTest() : url_("http://127.0.0.1/description.xml") {}

  void StartRequest() {
    description_fetcher_ = std::make_unique<TestDeviceDescriptionFetcher>(
        url_,
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

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceDescriptionFetcherTest);
};

TEST_F(DeviceDescriptionFetcherTest, FetchSuccessful) {
  std::string body("<xml>description</xml>");
  EXPECT_CALL(*this, OnSuccess(DialDeviceDescriptionData(
                         body, GURL("http://127.0.0.1/apps"))));
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  head->headers->AddHeader("Application-URL: http://127.0.0.1/apps");
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
  head->headers->AddHeader("Application-URL: http://127.0.0.1/apps/");
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
  head->headers->AddHeader("Application-URL:");
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
  head->headers->AddHeader("Application-URL: http://www.example.com");
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = body.size();
  loader_factory_.AddResponse(url_, std::move(head), body, status);
  StartRequest();
}

TEST_F(DeviceDescriptionFetcherTest, FetchFailsOnEmptyDescription) {
  EXPECT_CALL(*this, OnError(HasSubstr("Missing or empty response")));
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  head->headers->AddHeader("Application-URL: http://127.0.0.1/apps");

  loader_factory_.AddResponse(url_, std::move(head), "",
                              network::URLLoaderCompletionStatus());
  StartRequest();
}

TEST_F(DeviceDescriptionFetcherTest, FetchFailsOnBadDescription) {
  EXPECT_CALL(*this, OnError(HasSubstr("Invalid response encoding")));
  std::string body("\xfc\x9c\xbf\x80\xbf\x80");
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  head->headers->AddHeader("Application-URL: http://127.0.0.1/apps");
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = body.size();
  loader_factory_.AddResponse(url_, std::move(head), body, status);
  StartRequest();
}

}  // namespace media_router
