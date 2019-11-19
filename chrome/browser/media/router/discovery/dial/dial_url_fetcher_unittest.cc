// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/media/router/discovery/dial/dial_url_fetcher.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::HasSubstr;

namespace media_router {

class DialURLFetcherTest : public testing::Test {
 public:
  DialURLFetcherTest() : url_("http://127.0.0.1/app/Youtube") {}

  void StartGetRequest() {
    fetcher_ = std::make_unique<TestDialURLFetcher>(
        base::BindOnce(&DialURLFetcherTest::OnSuccess, base::Unretained(this)),
        base::BindOnce(&DialURLFetcherTest::OnError, base::Unretained(this)),
        &loader_factory_);
    fetcher_->Get(url_);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  MOCK_METHOD1(OnSuccess, void(const std::string&));
  MOCK_METHOD2(OnError, void(int, const std::string&));

  base::test::TaskEnvironment environment_;
  network::TestURLLoaderFactory loader_factory_;
  const GURL url_;
  std::unique_ptr<TestDialURLFetcher> fetcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DialURLFetcherTest);
};

TEST_F(DialURLFetcherTest, FetchSuccessful) {
  std::string body("<xml>appInfo</xml>");
  EXPECT_CALL(*this, OnSuccess(body));
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = body.size();
  loader_factory_.AddResponse(url_, network::mojom::URLResponseHead::New(),
                              body, status);
  StartGetRequest();
}

TEST_F(DialURLFetcherTest, FetchFailsOnMissingAppInfo) {
  EXPECT_CALL(*this, OnError(404, HasSubstr("404")));
  loader_factory_.AddResponse(
      url_, network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
  StartGetRequest();
}

TEST_F(DialURLFetcherTest, FetchFailsOnEmptyAppInfo) {
  EXPECT_CALL(*this, OnError(_, HasSubstr("Missing or empty response")));
  loader_factory_.AddResponse(url_, network::mojom::URLResponseHead::New(), "",
                              network::URLLoaderCompletionStatus());
  StartGetRequest();
}

TEST_F(DialURLFetcherTest, FetchFailsOnBadAppInfo) {
  EXPECT_CALL(*this, OnError(_, "Invalid response encoding"));
  std::string body("\xfc\x9c\xbf\x80\xbf\x80");
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = body.size();
  loader_factory_.AddResponse(url_, network::mojom::URLResponseHead::New(),
                              body, status);
  StartGetRequest();
}

}  // namespace media_router
