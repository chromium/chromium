// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_service.h"

#include <memory>

#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace kaleidoscope {

namespace {

const char kTestUrl[] =
    "https://chromemediarecommendations-pa.googleapis.com/v1/collections";

const char kTestData[] = "zzzz";

const char kTestAPIKey[] = "apikey";

const char kTestAccessToken[] = "accesstoken";

}  // namespace

class KaleidoscopeServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  KaleidoscopeServiceTest() = default;
  ~KaleidoscopeServiceTest() override = default;
  KaleidoscopeServiceTest(const KaleidoscopeServiceTest& t) = delete;
  KaleidoscopeServiceTest& operator=(const KaleidoscopeServiceTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    GetService()->test_url_loader_factory_for_fetcher_ =
        base::MakeRefCounted<::network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
  }

  ::network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  bool RespondToFetch(
      const std::string& response_body,
      net::HttpStatusCode response_code = net::HttpStatusCode::HTTP_OK,
      int net_error = net::OK) {
    auto response_head = ::network::CreateURLResponseHead(response_code);

    bool rv = url_loader_factory()->SimulateResponseForPendingRequest(
        GURL(kTestUrl), ::network::URLLoaderCompletionStatus(net_error),
        std::move(response_head), response_body);
    task_environment()->RunUntilIdle();
    return rv;
  }

  void WaitForRequest() {
    task_environment()->RunUntilIdle();

    ASSERT_TRUE(GetCurrentRequest().url.is_valid());
    EXPECT_EQ(net::HttpRequestHeaders::kPostMethod, GetCurrentRequest().method);
    EXPECT_EQ(GetCurrentlyQueriedHeaderValue("X-Goog-Api-Key"), kTestAPIKey);
    EXPECT_EQ(
        GetCurrentlyQueriedHeaderValue(net::HttpRequestHeaders::kAuthorization),
        base::StrCat({"Bearer ", kTestAccessToken}));
  }

  media::mojom::CredentialsPtr CreateCredentials() {
    auto creds = media::mojom::Credentials::New();
    creds->api_key = kTestAPIKey;
    creds->access_token = kTestAccessToken;
    return creds;
  }

  KaleidoscopeService* GetService() {
    return KaleidoscopeService::Get(profile());
  }

 private:
  std::string GetCurrentlyQueriedHeaderValue(const base::StringPiece& key) {
    std::string out;
    GetCurrentRequest().headers.GetHeader(key, &out);
    return out;
  }

  const ::network::ResourceRequest& GetCurrentRequest() {
    return url_loader_factory()->pending_requests()->front().request;
  }

  ::network::TestURLLoaderFactory url_loader_factory_;
};

TEST_F(KaleidoscopeServiceTest, Success) {
  GetService()->GetCollections(
      CreateCredentials(), "123", "abcd",
      base::BindLambdaForTesting(
          [&](const std::string& result) { EXPECT_EQ(kTestData, result); }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch(kTestData));
}

TEST_F(KaleidoscopeServiceTest, ServerFail) {
  GetService()->GetCollections(
      CreateCredentials(), "123", "abcd",
      base::BindLambdaForTesting(
          [&](const std::string& result) { EXPECT_TRUE(result.empty()); }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch("", net::HTTP_BAD_REQUEST));
}

TEST_F(KaleidoscopeServiceTest, NetworkFail) {
  GetService()->GetCollections(
      CreateCredentials(), "123", "abcd",
      base::BindLambdaForTesting(
          [&](const std::string& result) { EXPECT_TRUE(result.empty()); }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch("", net::HTTP_OK, net::ERR_UNEXPECTED));
}

}  // namespace kaleidoscope
