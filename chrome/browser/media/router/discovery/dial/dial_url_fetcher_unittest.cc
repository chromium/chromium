// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "chrome/browser/media/router/discovery/dial/dial_url_fetcher.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::HasSubstr;
using testing::NiceMock;

namespace media_router {

class DialURLFetcherTest : public testing::Test {
 public:
  DialURLFetcherTest() : url_("http://127.0.0.1/app/Youtube") {}
  DialURLFetcherTest(DialURLFetcherTest&) = delete;
  DialURLFetcherTest& operator=(DialURLFetcherTest&) = delete;

  void StartGetRequest() {
    fetcher_ = std::make_unique<NiceMock<TestDialURLFetcher>>(
        base::BindOnce(&DialURLFetcherTest::OnSuccess, base::Unretained(this)),
        base::BindOnce(&DialURLFetcherTest::OnError, base::Unretained(this)),
        &loader_factory_);
    fetcher_->SetSavedRequestForTest(&request_);
    fetcher_->Get(url_);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  MOCK_METHOD(void, OnSuccess, (const std::string&));
  MOCK_METHOD(void, OnError, (const std::string&, std::optional<int>));

  base::test::TaskEnvironment environment_;
  network::TestURLLoaderFactory loader_factory_;
  const GURL url_;
  std::unique_ptr<TestDialURLFetcher> fetcher_;
  network::ResourceRequest request_;
};

TEST_F(DialURLFetcherTest, FetchSuccessful) {
  std::string body("<xml>appInfo</xml>");
  EXPECT_CALL(*this, OnSuccess(body));
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = body.size();
  loader_factory_.AddResponse(url_, network::mojom::URLResponseHead::New(),
                              body, status);
  StartGetRequest();

  // Verify the request parameters.
  EXPECT_EQ(request_.url, url_);
  EXPECT_EQ(request_.method, "GET");
  EXPECT_THAT(request_.headers.GetHeader("Origin"),
              testing::Optional(testing::StartsWith("package:")));
  EXPECT_TRUE(request_.load_flags & net::LOAD_BYPASS_PROXY);
  EXPECT_TRUE(request_.load_flags & net::LOAD_DISABLE_CACHE);
  EXPECT_EQ(request_.credentials_mode, network::mojom::CredentialsMode::kOmit);
}

TEST_F(DialURLFetcherTest, FetchFailsOnMissingAppInfo) {
  auto head = network::mojom::URLResponseHead::New();
  head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 404 Not Found");
  ASSERT_EQ(404, head->headers->response_code());

  EXPECT_CALL(*this, OnError(HasSubstr(base::NumberToString(
                                 net::ERR_HTTP_RESPONSE_CODE_FAILURE)),
                             std::optional<int>(404)));
  loader_factory_.AddResponse(
      url_, std::move(head), "",
      network::URLLoaderCompletionStatus(net::ERR_HTTP_RESPONSE_CODE_FAILURE),
      {}, network::TestURLLoaderFactory::kSendHeadersOnNetworkError);
  StartGetRequest();
}

TEST_F(DialURLFetcherTest, FetchFailsOnEmptyAppInfo) {
  EXPECT_CALL(*this, OnError(HasSubstr("Missing or empty response"), _));
  loader_factory_.AddResponse(url_, network::mojom::URLResponseHead::New(), "",
                              network::URLLoaderCompletionStatus());
  StartGetRequest();
}

TEST_F(DialURLFetcherTest, FetchFailsOnBadAppInfo) {
  EXPECT_CALL(*this, OnError("Invalid response encoding", _));
  std::string body("\xfc\x9c\xbf\x80\xbf\x80");
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = body.size();
  loader_factory_.AddResponse(url_, network::mojom::URLResponseHead::New(),
                              body, status);
  StartGetRequest();
}

}  // namespace media_router
