// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_fetcher.h"

#include <memory>

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/schema_org/common/metadata.mojom.h"
#include "components/schema_org/schema_org_entity_names.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/cookie_access_result.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace media_feeds {

using testing::_;

namespace {

const char kTestUrl[] = "https://www.google.com";

const char kTestData[] = R"END({
    "@context": "https://schema.org",
    "@type": "CompleteDataFeed",
    "dataFeedElement": [],
    "provider": {
      "@type": "Organization",
      "name": "Media Feeds Developers",
      "logo": [{
        "@type": "ImageObject",
        "width": 1113,
        "height": 245,
        "url": "https://wicg.github.io/media-feeds/data/logo_white.png",
        "additionalProperty": {
          "@type": "PropertyValue",
          "name": "contentAttributes",
          "value": ["forDarkBackground", "hasTitle", "transparentBackground"]
        }
      }]
    }
})END";

const char kTestDataWithAssociatedOrigins[] = R"END({
    "@context": "https://schema.org",
    "@type": "CompleteDataFeed",
    "dataFeedElement": [],
    "additionalProperty": {
      "@type": "PropertyValue",
      "name": "associatedOrigin",
      "value": ["https://login.example.org", "https://login.example.com"]
    },
    "provider": {
      "@type": "Organization",
      "name": "Media Feeds Developers",
      "logo": [{
        "@type": "ImageObject",
        "width": 1113,
        "height": 245,
        "url": "https://wicg.github.io/media-feeds/data/logo_white.png",
        "additionalProperty": {
          "@type": "PropertyValue",
          "name": "contentAttributes",
          "value": ["forDarkBackground", "hasTitle", "transparentBackground"]
        }
      }]
    }
})END";

const char kTestFeedName[] = "Media Feeds Developers";

}  // namespace

class MediaFeedsFetcherTest : public ChromeRenderViewHostTestHarness {
 public:
  MediaFeedsFetcherTest() = default;
  ~MediaFeedsFetcherTest() override = default;
  MediaFeedsFetcherTest(const MediaFeedsFetcherTest& t) = delete;
  MediaFeedsFetcherTest& operator=(const MediaFeedsFetcherTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    fetcher_ = std::make_unique<MediaFeedsFetcher>(
        base::MakeRefCounted<::network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory()));
  }

  MediaFeedsFetcher* fetcher() { return fetcher_.get(); }
  ::network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  bool RespondToFetch(
      const std::string& response_body,
      net::HttpStatusCode response_code = net::HttpStatusCode::HTTP_OK,
      int net_error = net::OK,
      bool was_fetched_via_cache = false) {
    auto response_head = ::network::CreateURLResponseHead(response_code);
    response_head->was_fetched_via_cache = was_fetched_via_cache;

    bool rv = url_loader_factory()->SimulateResponseForPendingRequest(
        GURL(kTestUrl), ::network::URLLoaderCompletionStatus(net_error),
        std::move(response_head), response_body);
    task_environment()->RunUntilIdle();
    return rv;
  }

  void WaitForRequest() {
    task_environment()->RunUntilIdle();

    ASSERT_TRUE(GetCurrentRequest().url.is_valid());
    EXPECT_TRUE(GetCurrentRequest().site_for_cookies.IsEquivalent(
        net::SiteForCookies::FromUrl(GURL(kTestUrl))));
    EXPECT_EQ(GetCurrentlyQueriedHeaderValue(net::HttpRequestHeaders::kAccept),
              "application/ld+json");
    EXPECT_EQ(GetCurrentRequest().redirect_mode,
              ::network::mojom::RedirectMode::kError);
    EXPECT_EQ(net::HttpRequestHeaders::kGetMethod, GetCurrentRequest().method);
  }

  bool SetCookie(content::BrowserContext* browser_context,
                 const GURL& url,
                 const std::string& value) {
    bool result = false;
    base::RunLoop run_loop;
    mojo::Remote<network::mojom::CookieManager> cookie_manager;
    content::BrowserContext::GetDefaultStoragePartition(browser_context)
        ->GetNetworkContext()
        ->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());
    std::unique_ptr<net::CanonicalCookie> cc(net::CanonicalCookie::Create(
        url, value, base::Time::Now(), base::nullopt /* server_time */));
    EXPECT_TRUE(cc.get());

    cookie_manager->SetCanonicalCookie(
        *cc.get(), url, net::CookieOptions::MakeAllInclusive(),
        base::BindOnce(
            [](bool* result, base::RunLoop* run_loop,
               net::CookieAccessResult set_cookie_access_result) {
              *result = set_cookie_access_result.status.IsInclude();
              run_loop->Quit();
            },
            &result, &run_loop));
    run_loop.Run();
    return result;
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
  std::unique_ptr<MediaFeedsFetcher> fetcher_;
  data_decoder::test::InProcessDataDecoder data_decoder_;
};

TEST_F(MediaFeedsFetcherTest, SucceedsOnBasicFetch) {
  GURL site_with_cookies(kTestUrl);
  ASSERT_TRUE(SetCookie(profile(), site_with_cookies, "testing"));

  fetcher()->FetchFeed(
      GURL("https://www.google.com"), false,
      base::BindLambdaForTesting(
          [&](media_history::MediaHistoryKeyedService::MediaFeedFetchResult
                  result) {
            EXPECT_EQ(result.status, mojom::FetchResult::kSuccess);
            EXPECT_FALSE(result.was_fetched_from_cache);
            EXPECT_FALSE(result.gone);
            EXPECT_EQ(kTestFeedName, result.display_name);
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch(kTestData));
}

TEST_F(MediaFeedsFetcherTest, SucceedsOnBasicFetch_ForceCache) {
  GURL site_with_cookies(kTestUrl);
  ASSERT_TRUE(SetCookie(profile(), site_with_cookies, "testing"));

  fetcher()->FetchFeed(
      GURL("https://www.google.com"), true,
      base::BindLambdaForTesting(
          [&](media_history::MediaHistoryKeyedService::MediaFeedFetchResult
                  result) {
            EXPECT_EQ(result.status, mojom::FetchResult::kSuccess);
            EXPECT_FALSE(result.was_fetched_from_cache);
            EXPECT_FALSE(result.gone);
            EXPECT_EQ(kTestFeedName, result.display_name);
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch(kTestData));
}

TEST_F(MediaFeedsFetcherTest, SucceedsFetchFromCache) {
  fetcher()->FetchFeed(
      GURL("https://www.google.com"), false,
      base::BindLambdaForTesting(
          [&](media_history::MediaHistoryKeyedService::MediaFeedFetchResult
                  result) {
            EXPECT_EQ(result.status, mojom::FetchResult::kSuccess);
            EXPECT_TRUE(result.was_fetched_from_cache);
            EXPECT_FALSE(result.gone);
            EXPECT_EQ(kTestFeedName, result.display_name);
          }));

  WaitForRequest();
  ASSERT_TRUE(
      RespondToFetch(kTestData, net::HttpStatusCode::HTTP_OK, net::OK, true));
}

TEST_F(MediaFeedsFetcherTest, ReturnsFailedResponseCode) {
  fetcher()->FetchFeed(
      GURL("https://www.google.com"), false,
      base::BindLambdaForTesting(
          [&](media_history::MediaHistoryKeyedService::MediaFeedFetchResult
                  result) {
            EXPECT_EQ(result.status, mojom::FetchResult::kFailedBackendError);
            EXPECT_FALSE(result.was_fetched_from_cache);
            EXPECT_FALSE(result.gone);
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch("", net::HTTP_BAD_REQUEST));
}

TEST_F(MediaFeedsFetcherTest, ReturnsGone) {
  base::MockCallback<MediaFeedsFetcher::MediaFeedCallback> callback;

  fetcher()->FetchFeed(
      GURL("https://www.google.com"), false,
      base::BindLambdaForTesting(
          [&](media_history::MediaHistoryKeyedService::MediaFeedFetchResult
                  result) {
            EXPECT_EQ(result.status, mojom::FetchResult::kNone);
            EXPECT_FALSE(result.was_fetched_from_cache);
            EXPECT_TRUE(result.gone);
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch("", net::HTTP_GONE));
}

TEST_F(MediaFeedsFetcherTest, ReturnsNetError) {
  fetcher()->FetchFeed(
      GURL("https://www.google.com"), false,
      base::BindLambdaForTesting(
          [&](media_history::MediaHistoryKeyedService::MediaFeedFetchResult
                  result) {
            EXPECT_EQ(result.status, mojom::FetchResult::kFailedBackendError);
            EXPECT_FALSE(result.was_fetched_from_cache);
            EXPECT_FALSE(result.gone);
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch("", net::HTTP_OK, net::ERR_UNEXPECTED));
}

TEST_F(MediaFeedsFetcherTest, ReturnsErrFileNotFoundForEmptyFeedData) {
  fetcher()->FetchFeed(
      GURL("https://www.google.com"), false,
      base::BindLambdaForTesting(
          [&](media_history::MediaHistoryKeyedService::MediaFeedFetchResult
                  result) {
            EXPECT_EQ(result.status, mojom::FetchResult::kFailedNetworkError);
            EXPECT_FALSE(result.was_fetched_from_cache);
            EXPECT_FALSE(result.gone);
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch(""));
}

TEST_F(MediaFeedsFetcherTest, ReturnsErrFailedForBadEntityData) {
  fetcher()->FetchFeed(
      GURL("https://www.google.com"), false,
      base::BindLambdaForTesting(
          [&](media_history::MediaHistoryKeyedService::MediaFeedFetchResult
                  result) {
            EXPECT_EQ(result.status, mojom::FetchResult::kFailedBackendError);
            EXPECT_FALSE(result.was_fetched_from_cache);
            EXPECT_FALSE(result.gone);
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch(
      "{\"@type\":\"CompleteDataFeed\"\"name\":\"Bad json missing a comma\"}"));
}

TEST_F(MediaFeedsFetcherTest, Success_AssociatedOrigins_BothWork) {
  GURL site_with_cookies(kTestUrl);
  ASSERT_TRUE(SetCookie(profile(), site_with_cookies, "testing"));

  fetcher()->FetchFeed(
      GURL("https://www.google.com"), false,
      base::BindLambdaForTesting(
          [&](media_history::MediaHistoryKeyedService::MediaFeedFetchResult
                  result) {
            EXPECT_EQ(result.status, mojom::FetchResult::kSuccess);
            EXPECT_FALSE(result.was_fetched_from_cache);
            EXPECT_FALSE(result.gone);
            EXPECT_EQ(kTestFeedName, result.display_name);
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch(kTestDataWithAssociatedOrigins));
}

}  // namespace media_feeds
