// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/ntp_json_fetcher.h"

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace explore_sites {

using testing::_;

class NTPJsonFetcherTest : public testing::Test {
 public:
  NTPJsonFetcherTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUp() {
    controllable_http_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &https_server_, "/ntp.json");
    ASSERT_TRUE(https_server_.Start());

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        chrome::android::kExploreSites,
        {std::make_pair("base_url", https_server_.base_url().spec())});
  }

  MOCK_METHOD0(OnGotCatalog, void());
  MOCK_METHOD0(OnError, void());

 protected:
  void SetValidResponse() {
    std::string json =
        R"({"categories":[{"icon_url":"https://www.google.com/favicon.ico",)"
        R"("title":"Sports","id":"Sports"}]})";

    controllable_http_response_->WaitForRequest();
    controllable_http_response_->Send(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "\r\n");
    controllable_http_response_->Send(json);
    controllable_http_response_->Done();
    base::RunLoop().RunUntilIdle();
  }

  void SetUnparseableResponse() {
    std::string json = R"({
      "esp_url": "https:\/\/example.com",
      "categories": [
        "abc"
      ]]
    })";

    controllable_http_response_->WaitForRequest();
    controllable_http_response_->Send(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "\r\n");
    controllable_http_response_->Send(json);
    controllable_http_response_->Done();
    base::RunLoop().RunUntilIdle();
  }

  void SetFailedResponse() {
    controllable_http_response_->WaitForRequest();
    controllable_http_response_->Send(
        "HTTP/1.1 400 BAD REQUEST\r\n"
        "Content-Type: application/json\r\n"
        "\r\n");
    controllable_http_response_->Done();
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<NTPJsonFetcher> StartFetcher() {
    auto fetcher = std::make_unique<NTPJsonFetcher>(browser_context());
    fetcher->Start(base::BindOnce(&NTPJsonFetcherTest::OnJsonFetched,
                                  base::Unretained(this)));
    return fetcher;
  }

  NTPCatalog* catalog() { return catalog_.get(); }
  content::BrowserContext* browser_context() { return &browser_context_; }

 private:
  void OnJsonFetched(std::unique_ptr<NTPCatalog> catalog) {
    if (catalog.get()) {
      catalog_ = std::move(catalog);
      OnGotCatalog();
    } else {
      OnError();
    }
  }

  std::unique_ptr<NTPCatalog> catalog_;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile browser_context_;
  net::EmbeddedTestServer https_server_;

  // This is how we configure the JSON responses.
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      controllable_http_response_;

  // This allows us to override the URL via finch params.
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(https://crbug.com/854250): Fix the tests. They are disabled because
// they're failing on trybots. Probably they have to be browser tests instead.
TEST_F(NTPJsonFetcherTest, DISABLED_Success) {
  EXPECT_CALL(*this, OnGotCatalog());
  auto fetcher = StartFetcher();
  SetValidResponse();
  testing::Mock::VerifyAndClearExpectations(this);
  std::vector<NTPCatalog::Category> category_list = {
      {"Sports", "Sports", GURL("https://www.google.com/favicon.ico")}};
  NTPCatalog expected(category_list);
  ASSERT_NE(nullptr, catalog());
  EXPECT_EQ(*catalog(), expected);
}

TEST_F(NTPJsonFetcherTest, DISABLED_Failure) {
  EXPECT_CALL(*this, OnError());
  auto fetcher = StartFetcher();
  SetFailedResponse();
}

TEST_F(NTPJsonFetcherTest, DISABLED_ParseFailure) {
  EXPECT_CALL(*this, OnError());
  auto fetcher = StartFetcher();
  SetUnparseableResponse();
}

}  // namespace explore_sites
