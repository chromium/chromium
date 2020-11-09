// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_service.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time_to_iso8601.h"
#include "chrome/browser/media/feeds/media_feeds_service_factory.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-shared.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_test_utils.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_search_api/stub_url_checker.h"
#include "components/safe_search_api/url_checker.h"
#include "content/public/browser/storage_partition.h"
#include "media/base/media_switches.h"
#include "net/base/load_flags.h"
#include "net/cookies/cookie_access_result.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_feeds {

using SafeSearchCheckedType =
    media_history::MediaHistoryKeyedService::SafeSearchCheckedType;

namespace {

constexpr size_t kCacheSize = 2;

constexpr base::FilePath::CharType kMediaFeedsTestSpecDir[] =
    FILE_PATH_LITERAL("chrome/test/data/media/feeds/spec");

const char kTestData[] = R"END({
    "@context": "https://schema.org",
    "@type": "CompleteDataFeed",
    "dataFeedElement": [
      {
        "@context": "https://schema.org/",
        "@type": "VideoObject",
        "@id": "https://www.youtube.com/watch?v=lXm6jOQLe1Y",
        "author": {
          "@type": "Person",
          "name": "Google Chrome Developers",
          "url": "https://www.youtube.com/user/ChromeDevelopers"
        },
        "datePublished": "2019-05-09",
        "duration": "PT34M41S",
        "isFamilyFriendly": "https://schema.org/True",
        "name": "Anatomy of a Web Media Experience",
        "potentialAction": {
          "@type": "WatchAction",
          "target": "https://www.youtube.com/watch?v=lXm6jOQLe1Y"
        },
        "image": {
          "@type": "ImageObject",
          "width": 336,
          "height": 188,
          "url": "https://beccahughes.github.io/media/media-feeds/video1.webp"
        }
      },
      {
        "@context": "https://schema.org/",
        "@type": "TVSeries",
        "@id": "https://beccahughes.github.io/media/media-feeds/chrome-release",
        "datePublished": "2019-11-10",
        "isFamilyFriendly": "https://schema.org/True",
        "name": "Chrome Releases",
        "containsSeason": {
          "@type": "TVSeason",
          "numberOfEpisodes": 80,
          "episode": {
            "@type": "TVEpisode",
            "@id": "https://www.youtube.com/watch?v=L0OB0_bO5I0",
            "duration": "PT4M16S",
            "episodeNumber": 79,
            "potentialAction": {
                "@type": "WatchAction",
                "actionStatus": "https://schema.org/ActiveActionStatus",
                "startTime": "00:04:14",
                "target": "https://www.youtube.com/watch?v=L0OB0_bO5I0?t=254"
            },
            "image": {
                "@type": "ImageObject",
                "width": 1874,
                "height": 970,
                "url": "https://beccahughes.github.io/media/media-feeds/chrome79_current.png"
            },
            "name": "New in Chrome 79"
          },
          "seasonNumber": 1
        },
        "image": {
          "@type": "ImageObject",
          "width": 336,
          "height": 188,
          "url": "https://beccahughes.github.io/media/media-feeds/chromerel.webp"
        }
      },
      {
        "@context": "https://schema.org/",
        "@type": "Movie",
        "@id": "https://beccahughes.github.io/media/media-feeds/big-buck-bunny",
        "datePublished": "2008-01-01",
        "duration": "PT12M",
        "isFamilyFriendly": "https://schema.org/False",
        "name": "Big Buck Bunny",
        "potentialAction": {
          "@type": "WatchAction",
          "target": "https://mounirlamouri.github.io/sandbox/media/dynamic-controls.html"
        },
        "image": {
          "@type": "ImageObject",
          "width": 1392,
          "height": 749,
          "url": "https://beccahughes.github.io/media/media-feeds/big_buck_bunny.jpg"
        }
      }
    ],
    "provider": {
      "@type": "Organization",
      "name": "Chromium Developers",
      "logo": [{
        "@type": "ImageObject",
        "width": 1113,
        "height": 245,
        "url": "https://beccahughes.github.io/media/media-feeds/chromium_logo_white.png",
        "additionalProperty": {
          "@type": "PropertyValue",
          "name": "contentAttributes",
          "value": ["forDarkBackground", "hasTitle", "transparentBackground"]
        }
      }, {
        "@type": "ImageObject",
        "width": 600,
        "height": 315,
        "url": "https://beccahughes.github.io/media/media-feeds/chromium_card.png",
        "additionalProperty": {
          "@type": "PropertyValue",
          "name": "contentAttributes",
          "value": ["forLightBackground", "hasTitle", "centered"]
        }
      }]
    }
})END";

const char kFirstItemActionURL[] = "https://www.example.com/action";
const char kFirstItemPlayNextActionURL[] = "https://www.example.com/next";

}  // namespace

class MediaFeedsServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  MediaFeedsServiceTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    features_.InitWithFeatures(
        {media::kMediaFeeds, media::kMediaFeedsSafeSearch,
         media::kMediaFeedsBackgroundFetching},
        {});

    ChromeRenderViewHostTestHarness::SetUp();

    stub_url_checker_ = std::make_unique<safe_search_api::StubURLChecker>();

    test_clock_.SetNow(base::Time::Now());

    GetMediaFeedsService()->SetClockForTesting(&test_clock_);

    GetMediaFeedsService()->SetSafeSearchURLCheckerForTest(
        stub_url_checker_->BuildURLChecker(kCacheSize));

    GetMediaFeedsService()->test_url_loader_factory_for_fetcher_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
  }

  void AdvanceTime(base::TimeDelta time_delta) {
    test_clock_.SetNow(test_clock_.Now() + time_delta);
    task_environment()->FastForwardBy(time_delta);
  }

  base::Time Now() { return test_clock_.Now(); }

  void WaitForDB() {
    base::RunLoop run_loop;
    GetMediaHistoryService()->PostTaskToDBForTest(run_loop.QuitClosure());
    run_loop.Run();
  }

  void SimulateOnCheckURLDone(
      const media_history::MediaHistoryKeyedService::SafeSearchID id,
      const GURL& url,
      safe_search_api::Classification classification,
      bool uncertain) {
    GetMediaFeedsService()->OnCheckURLDone(id, url, url, classification,
                                           uncertain);
  }

  bool AddInflightSafeSearchCheck(
      const media_history::MediaHistoryKeyedService::SafeSearchID id,
      const std::set<GURL>& urls) {
    return GetMediaFeedsService()->AddInflightSafeSearchCheck(id, urls);
  }

  media_history::MediaHistoryKeyedService::MediaFeedFetchResult
  SuccessfulResultWithItems(
      std::vector<media_feeds::mojom::MediaFeedItemPtr> items,
      const int64_t feed_id) {
    media_history::MediaHistoryKeyedService::MediaFeedFetchResult result;
    result.feed_id = feed_id;
    result.items = std::move(items);
    result.status = media_feeds::mojom::FetchResult::kSuccess;
    result.display_name = "test";
    result.reset_token = media_history::test::GetResetTokenSync(
        GetMediaHistoryService(), feed_id);
    return result;
  }

  bool GetCurrentRequestHasBypassCacheFlag() {
    return GetCurrentRequest().load_flags & net::LOAD_BYPASS_CACHE;
  }

  media_history::MediaHistoryKeyedService::PendingSafeSearchCheckList
  GetPendingSafeSearchCheckMediaFeedItemsSync() {
    base::RunLoop run_loop;
    media_history::MediaHistoryKeyedService::PendingSafeSearchCheckList out;

    GetMediaHistoryService()->GetPendingSafeSearchCheckMediaFeedItems(
        base::BindLambdaForTesting([&](media_history::MediaHistoryKeyedService::
                                           PendingSafeSearchCheckList rows) {
          out = std::move(rows);
          run_loop.Quit();
        }));

    run_loop.Run();
    return out;
  }

  void DiscoverFeedAndPerformSafeSearchCheck(const GURL& feed_url) {
    SetSafeSearchEnabled(true);
    safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

    base::RunLoop run_loop;
    GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
        run_loop.QuitClosure());

    // Store a Media Feed.
    GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
    WaitForDB();

    // Wait for the service and DB to finish.
    run_loop.Run();
    WaitForDB();

    // Return to the default state.
    safe_search_checker()->ClearResponses();
    SetSafeSearchEnabled(false);
  }

  std::vector<media_feeds::mojom::MediaFeedItemPtr> GetItemsForMediaFeedSync(
      const int64_t feed_id) {
    base::RunLoop run_loop;
    std::vector<media_feeds::mojom::MediaFeedItemPtr> out;

    GetMediaHistoryService()->GetMediaFeedItems(
        media_history::MediaHistoryKeyedService::GetMediaFeedItemsRequest::
            CreateItemsForDebug(feed_id),
        base::BindLambdaForTesting(
            [&](std::vector<media_feeds::mojom::MediaFeedItemPtr> rows) {
              out = std::move(rows);
              run_loop.Quit();
            }));

    run_loop.Run();
    return out;
  }

  std::vector<media_feeds::mojom::MediaFeedPtr> GetMediaFeedsSync(
      const media_history::MediaHistoryKeyedService::GetMediaFeedsRequest&
          request =
              media_history::MediaHistoryKeyedService::GetMediaFeedsRequest()) {
    base::RunLoop run_loop;
    std::vector<media_feeds::mojom::MediaFeedPtr> out;

    GetMediaHistoryService()->GetMediaFeeds(
        request, base::BindLambdaForTesting(
                     [&](std::vector<media_feeds::mojom::MediaFeedPtr> rows) {
                       out = std::move(rows);
                       run_loop.Quit();
                     }));

    run_loop.Run();
    return out;
  }

  void SetSafeSearchEnabled(bool enabled) {
    profile()->GetPrefs()->SetBoolean(prefs::kMediaFeedsSafeSearchEnabled,
                                      enabled);
  }

  void SetBackgroundFetchingEnabled(bool enabled) {
    profile()->GetPrefs()->SetBoolean(prefs::kMediaFeedsBackgroundFetching,
                                      enabled);
  }

  bool RespondToPendingFeedFetch(const GURL& feed_url,
                                 bool from_cache = false) {
    return RespondToPendingFeedFetchWithData(feed_url, kTestData, from_cache);
  }

  bool RespondToPendingFeedFetchWithData(const GURL& feed_url,
                                         const std::string& data,
                                         bool from_cache = false) {
    auto response_head =
        ::network::CreateURLResponseHead(net::HttpStatusCode::HTTP_OK);
    response_head->was_fetched_via_cache = from_cache;

    return url_loader_factory_.SimulateResponseForPendingRequest(
        feed_url, network::URLLoaderCompletionStatus(net::OK),
        std::move(response_head), data);
  }

  bool RespondToPendingFeedFetchWithStatus(const GURL& feed_url,
                                           net::HttpStatusCode code) {
    bool rv = url_loader_factory_.SimulateResponseForPendingRequest(
        feed_url, network::URLLoaderCompletionStatus(net::OK),
        network::CreateURLResponseHead(code), std::string());
    return rv;
  }

  void SetAutomaticSelectionEnabled() {
    profile()->GetPrefs()->SetBoolean(
        kaleidoscope::prefs::kKaleidoscopeAutoSelectMediaFeeds, true);
  }

  safe_search_api::StubURLChecker* safe_search_checker() {
    return stub_url_checker_.get();
  }

  MediaFeedsService* GetMediaFeedsService() {
    return MediaFeedsServiceFactory::GetInstance()->GetForProfile(profile());
  }

  media_history::MediaHistoryKeyedService* GetMediaHistoryService() {
    return media_history::MediaHistoryKeyedService::Get(profile());
  }

  static media_feeds::mojom::MediaFeedItemPtr GetSingleExpectedItem(
      int id_start = 0) {
    auto item = media_feeds::mojom::MediaFeedItem::New();
    item->id = ++id_start;
    item->name = base::ASCIIToUTF16("The Movie");
    item->type = media_feeds::mojom::MediaFeedItemType::kMovie;
    item->date_published = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMinutes(10));
    item->action_status =
        media_feeds::mojom::MediaFeedItemActionStatus::kPotential;
    item->action = media_feeds::mojom::Action::New();
    item->action->url = GURL(kFirstItemActionURL);
    item->play_next_candidate = media_feeds::mojom::PlayNextCandidate::New();
    item->play_next_candidate->action = media_feeds::mojom::Action::New();
    item->play_next_candidate->action->url = GURL(kFirstItemPlayNextActionURL);

    return item;
  }

  static std::vector<media_feeds::mojom::MediaFeedItemPtr> GetExpectedItems(
      int id_start = 0) {
    std::vector<media_feeds::mojom::MediaFeedItemPtr> items;

    items.push_back(GetSingleExpectedItem(id_start));
    id_start++;

    {
      auto item = media_feeds::mojom::MediaFeedItem::New();
      item->id = ++id_start;
      item->type = media_feeds::mojom::MediaFeedItemType::kTVSeries;
      item->name = base::ASCIIToUTF16("The TV Series");
      item->action_status =
          media_feeds::mojom::MediaFeedItemActionStatus::kActive;
      item->action = media_feeds::mojom::Action::New();
      item->action->url = GURL("https://www.example.com/action2");
      item->author = media_feeds::mojom::Author::New();
      item->author->name = "Media Site";
      items.push_back(std::move(item));
    }

    {
      auto item = media_feeds::mojom::MediaFeedItem::New();
      item->id = ++id_start;
      item->type = media_feeds::mojom::MediaFeedItemType::kTVSeries;
      item->name = base::ASCIIToUTF16("The Live TV Series");
      item->action_status =
          media_feeds::mojom::MediaFeedItemActionStatus::kPotential;
      item->action = media_feeds::mojom::Action::New();
      item->action->url = GURL("https://www.example.com/action3");
      item->live = media_feeds::mojom::LiveDetails::New();
      items.push_back(std::move(item));
    }

    return items;
  }

  void CreateCookies(const std::vector<GURL>& urls,
                     bool domain_cookies = false,
                     bool expired = false) {
    const base::Time creation_time = base::Time::Now();

    base::RunLoop run_loop;
    int tasks = urls.size();

    for (auto& url : urls) {
      std::vector<std::string> cookie_line;
      cookie_line.push_back("A=1");

      if (url.SchemeIsCryptographic())
        cookie_line.push_back("Secure");

      if (domain_cookies)
        cookie_line.push_back("Domain=" + url.host());

      if (expired)
        cookie_line.push_back("Expires=Wed, 31 Dec 1969 07:28:00 GMT");

      std::unique_ptr<net::CanonicalCookie> cookie =
          net::CanonicalCookie::Create(url, base::JoinString(cookie_line, ";"),
                                       creation_time, creation_time);

      EXPECT_EQ(domain_cookies, cookie->IsDomainCookie());
      EXPECT_EQ(!domain_cookies, cookie->IsHostCookie());

      GetCookieManager()->SetCanonicalCookie(
          *cookie, url, net::CookieOptions::MakeAllInclusive(),
          base::BindLambdaForTesting(
              [&](net::CookieAccessResult access_result) {
                if (--tasks == 0)
                  run_loop.Quit();
              }));
    }

    run_loop.Run();
  }

  uint32_t DeleteCookies(network::mojom::CookieDeletionFilterPtr filter) {
    base::RunLoop run_loop;
    uint32_t result_out = 0u;

    GetCookieManager()->DeleteCookies(
        std::move(filter),
        base::BindLambdaForTesting([&run_loop, &result_out](uint32_t result) {
          result_out = result;
          run_loop.Quit();
        }));

    run_loop.Run();
    return result_out;
  }

  network::mojom::CookieManager* GetCookieManager() {
    auto* partition =
        content::BrowserContext::GetDefaultStoragePartition(profile());
    return partition->GetCookieManagerForBrowserProcess();
  }

  bool IsCookieObserverEnabled() const { return true; }

 private:
  const ::network::ResourceRequest& GetCurrentRequest() {
    return url_loader_factory_.pending_requests()->front().request;
  }

  base::test::ScopedFeatureList features_;

  network::TestURLLoaderFactory url_loader_factory_;

  std::unique_ptr<safe_search_api::StubURLChecker> stub_url_checker_;

  data_decoder::test::InProcessDataDecoder data_decoder_;

  base::SimpleTestClock test_clock_;
};

TEST_F(MediaFeedsServiceTest, GetForProfile) {
  EXPECT_NE(nullptr, MediaFeedsServiceFactory::GetForProfile(profile()));

  Profile* otr_profile = profile()->GetPrimaryOTRProfile();
  EXPECT_EQ(nullptr, MediaFeedsServiceFactory::GetForProfile(otr_profile));
}

TEST_F(MediaFeedsServiceTest, FetchFeed_Success) {
  base::HistogramTester histogram_tester;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Fetch the Media Feed.
  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(
      1, base::BindLambdaForTesting(
             [&](const std::string& ignored) { run_loop.Quit(); }));
  WaitForDB();
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));
  run_loop.Run();

  auto feeds = GetMediaFeedsSync();

  EXPECT_EQ(1u, feeds.size());
  EXPECT_TRUE(feeds[0]->last_fetch_time_not_cache_hit);
  EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
            feeds[0]->last_fetch_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsFetcher::kFetchSizeKbHistogramName, 15, 1);
}

TEST_F(MediaFeedsServiceTest, FetchFeed_SuccessFromCache) {
  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Fetch the Media Feed.
  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(
      1, base::BindLambdaForTesting(
             [&](const std::string& ignored) { run_loop.Quit(); }));
  WaitForDB();
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url, true));
  run_loop.Run();

  auto feeds = GetMediaFeedsSync();

  EXPECT_EQ(1u, feeds.size());
  EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
            feeds[0]->last_fetch_result);

  // This should not be set yet because the only fetch for this feed was from
  // the cache.
  EXPECT_FALSE(feeds[0]->last_fetch_time_not_cache_hit);
}

TEST_F(MediaFeedsServiceTest, FetchFeed_BackendError) {
  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Fetch the Media Feed.
  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(
      1, base::BindLambdaForTesting(
             [&](const std::string& ignored) { run_loop.Quit(); }));
  WaitForDB();
  ASSERT_TRUE(RespondToPendingFeedFetchWithStatus(
      feed_url, net::HTTP_INTERNAL_SERVER_ERROR));
  run_loop.Run();

  auto feeds = GetMediaFeedsSync();

  EXPECT_EQ(1u, feeds.size());
  EXPECT_EQ(media_feeds::mojom::FetchResult::kFailedBackendError,
            feeds[0]->last_fetch_result);
}

TEST_F(MediaFeedsServiceTest, FetchFeed_NotFoundError) {
  const GURL feed_url("https://www.google.com/feed");

  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Fetch the Media Feed.
  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(
      1, base::BindLambdaForTesting(
             [&](const std::string& ignored) { run_loop.Quit(); }));
  WaitForDB();
  ASSERT_TRUE(RespondToPendingFeedFetchWithStatus(feed_url, net::HTTP_OK));
  run_loop.Run();

  auto feeds = GetMediaFeedsSync();

  EXPECT_EQ(1u, feeds.size());
  EXPECT_EQ(media_feeds::mojom::FetchResult::kFailedNetworkError,
            feeds[0]->last_fetch_result);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_AllSafe) {
  DiscoverFeedAndPerformSafeSearchCheck(GURL("https://www.google.com/feed"));

  base::HistogramTester histogram_tester;
  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should be empty.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_TRUE(pending_items.empty());
  }

  // Check the items were updated.
  auto items = GetItemsForMediaFeedSync(1);
  EXPECT_EQ(3u, items.size());
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kSafe,
            items[0]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kSafe,
            items[1]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kSafe,
            items[2]->safe_search_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kSafe, 3);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_AllUnsafe) {
  DiscoverFeedAndPerformSafeSearchCheck(GURL("https://www.google.com/feed"));

  base::HistogramTester histogram_tester;
  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ true);

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should be empty.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_TRUE(pending_items.empty());
  }

  // Check the items were updated.
  auto items = GetItemsForMediaFeedSync(1);
  EXPECT_EQ(3u, items.size());
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnsafe,
            items[0]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnsafe,
            items[1]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnsafe,
            items[2]->safe_search_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kUnsafe, 3);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_Failed_Request) {
  DiscoverFeedAndPerformSafeSearchCheck(GURL("https://www.google.com/feed"));

  base::HistogramTester histogram_tester;
  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpFailedResponse();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should still be 3.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
  }

  // Check the items were updated.
  auto items = GetItemsForMediaFeedSync(1);
  EXPECT_EQ(3u, items.size());
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[0]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[1]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[2]->safe_search_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kUnknown, 3);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_Failed_Pref) {
  DiscoverFeedAndPerformSafeSearchCheck(GURL("https://www.google.com/feed"));
  base::HistogramTester histogram_tester;

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should still be 3.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
  }

  // Check the items were updated.
  auto items = GetItemsForMediaFeedSync(1);
  EXPECT_EQ(3u, items.size());
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[0]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[1]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[2]->safe_search_result);

  histogram_tester.ExpectTotalCount(
      MediaFeedsService::kSafeSearchResultHistogramName, 0);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_CheckTwice_Inflight) {
  DiscoverFeedAndPerformSafeSearchCheck(GURL("https://www.google.com/feed"));

  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  {
    // This checks we ignore the duplicate items for inflight checks.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should be empty.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_TRUE(pending_items.empty());
  }
}

TEST_F(MediaFeedsServiceTest, SafeSearch_CheckTwice_Committed) {
  DiscoverFeedAndPerformSafeSearchCheck(GURL("https://www.google.com/feed"));

  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  auto pending_items_a = GetPendingSafeSearchCheckMediaFeedItemsSync();
  EXPECT_EQ(3u, pending_items_a.size());

  auto pending_items_b = GetPendingSafeSearchCheckMediaFeedItemsSync();
  EXPECT_EQ(3u, pending_items_b.size());

  {
    base::RunLoop run_loop;
    GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
        run_loop.QuitClosure());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items_a));

    // Wait for the service and DB to finish.
    run_loop.Run();
    WaitForDB();
  }

  {
    base::RunLoop run_loop;
    GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
        run_loop.QuitClosure());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items_b));

    // Wait for the service and DB to finish.
    run_loop.Run();
    WaitForDB();
  }

  {
    // The pending items should be empty.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_TRUE(pending_items.empty());
  }
}

TEST_F(MediaFeedsServiceTest, SafeSearch_Mixed_SafeUnsafe) {
  DiscoverFeedAndPerformSafeSearchCheck(GURL("https://www.google.com/feed"));

  SetSafeSearchEnabled(true);
  base::HistogramTester histogram_tester;

  // Store some media feed items.
  std::vector<media_feeds::mojom::MediaFeedItemPtr> items;
  items.push_back(GetSingleExpectedItem());
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(std::move(items), 1), base::DoNothing());
  WaitForDB();

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(1u, pending_items.size());
    EXPECT_TRUE(AddInflightSafeSearchCheck(pending_items[0]->id,
                                           pending_items[0]->urls));
  }

  SimulateOnCheckURLDone(std::make_pair(SafeSearchCheckedType::kFeedItem, 1),
                         GURL(kFirstItemActionURL),
                         safe_search_api::Classification::SAFE,
                         /*uncertain=*/false);
  SimulateOnCheckURLDone(std::make_pair(SafeSearchCheckedType::kFeedItem, 1),
                         GURL(kFirstItemPlayNextActionURL),
                         safe_search_api::Classification::UNSAFE,
                         /*uncertain=*/false);

  WaitForDB();

  {
    // Check the result of the item we stored.
    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_EQ(1u, items.size());
    EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnsafe,
              items[0]->safe_search_result);
  }

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kUnsafe, 1);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_Mixed_SafeUncertain) {
  DiscoverFeedAndPerformSafeSearchCheck(GURL("https://www.google.com/feed"));

  SetSafeSearchEnabled(true);
  base::HistogramTester histogram_tester;

  // Store some media feed items.
  std::vector<media_feeds::mojom::MediaFeedItemPtr> items;
  items.push_back(GetSingleExpectedItem());
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(std::move(items), 1), base::DoNothing());
  WaitForDB();

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(1u, pending_items.size());
    EXPECT_TRUE(AddInflightSafeSearchCheck(pending_items[0]->id,
                                           pending_items[0]->urls));
  }

  SimulateOnCheckURLDone(std::make_pair(SafeSearchCheckedType::kFeedItem, 1),
                         GURL(kFirstItemActionURL),
                         safe_search_api::Classification::SAFE,
                         /*uncertain=*/false);
  SimulateOnCheckURLDone(std::make_pair(SafeSearchCheckedType::kFeedItem, 1),
                         GURL(kFirstItemPlayNextActionURL),
                         safe_search_api::Classification::SAFE,
                         /*uncertain=*/true);

  WaitForDB();

  {
    // Check the result of the item we stored.
    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_EQ(1u, items.size());
    EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
              items[0]->safe_search_result);
  }

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kUnknown, 1);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_Mixed_UnsafeUncertain) {
  DiscoverFeedAndPerformSafeSearchCheck(GURL("https://www.google.com/feed"));

  SetSafeSearchEnabled(true);
  base::HistogramTester histogram_tester;

  // Store some media feed items.
  std::vector<media_feeds::mojom::MediaFeedItemPtr> items;
  items.push_back(GetSingleExpectedItem());
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(std::move(items), 1), base::DoNothing());
  WaitForDB();

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(1u, pending_items.size());
    EXPECT_TRUE(AddInflightSafeSearchCheck(pending_items[0]->id,
                                           pending_items[0]->urls));
  }

  SimulateOnCheckURLDone(std::make_pair(SafeSearchCheckedType::kFeedItem, 1),
                         GURL(kFirstItemActionURL),
                         safe_search_api::Classification::UNSAFE,
                         /*uncertain=*/false);
  SimulateOnCheckURLDone(std::make_pair(SafeSearchCheckedType::kFeedItem, 1),
                         GURL(kFirstItemPlayNextActionURL),
                         safe_search_api::Classification::SAFE,
                         /*uncertain=*/true);

  WaitForDB();

  {
    // Check the result of the item we stored.
    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_EQ(1u, items.size());
    EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnsafe,
              items[0]->safe_search_result);
  }

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kUnsafe, 1);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_Failed_Feature) {
  DiscoverFeedAndPerformSafeSearchCheck(GURL("https://www.google.com/feed"));

  SetSafeSearchEnabled(true);

  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(media::kMediaFeedsSafeSearch);

  base::HistogramTester histogram_tester;

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should still be 3.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
  }

  // Check the items were updated.
  auto items = GetItemsForMediaFeedSync(1);
  EXPECT_EQ(3u, items.size());
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[0]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[1]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[2]->safe_search_result);

  histogram_tester.ExpectTotalCount(
      MediaFeedsService::kSafeSearchResultHistogramName, 0);
}

TEST_F(MediaFeedsServiceTest, FetcherShouldTriggerSafeSearch) {
  const GURL feed_url("https://www.google.com/feed");
  DiscoverFeedAndPerformSafeSearchCheck(feed_url);

  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Fetch the Media Feed.
    base::RunLoop run_loop;
    GetMediaFeedsService()->FetchMediaFeed(
        1, base::BindLambdaForTesting(
               [&](const std::string& ignored) { run_loop.Quit(); }));
    WaitForDB();
    ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));
    run_loop.Run();
  }

  // Wait for the items to be checked against safe search
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should be empty.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_TRUE(pending_items.empty());
  }

  // Check the items were updated.
  auto items = GetItemsForMediaFeedSync(1);
  EXPECT_EQ(3u, items.size());

  for (auto& item : items) {
    EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kSafe,
              item->safe_search_result);
  }
}

TEST_F(MediaFeedsServiceTest, FetcherShouldDeleteFeedIfGone) {
  const GURL feed_url("https://www.google.com/feed");
  DiscoverFeedAndPerformSafeSearchCheck(feed_url);

  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Check there are pending items.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
  }

  // Enable the safe search pref. This should trigger a refetch.
  SetSafeSearchEnabled(true);

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should be empty.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_TRUE(pending_items.empty());
  }

  {
    // Check the items were updated.
    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_EQ(3u, items.size());

    for (auto& item : items) {
      EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kSafe,
                item->safe_search_result);
    }
  }

  // Store some new media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    // Check there are pending items.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
  }
}

TEST_F(MediaFeedsServiceTest, FetcherShouldSupportMultipleFetchesForSameFeed) {
  const GURL feed_url("https://www.google.com/feed");

  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Fetch the same feed twice.
  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(
      1, base::BindLambdaForTesting(
             [&](const std::string& ignored) { run_loop.Quit(); }));
  WaitForDB();

  base::RunLoop run_loop_alt;
  GetMediaFeedsService()->FetchMediaFeed(
      1, base::BindLambdaForTesting(
             [&](const std::string& ignored) { run_loop_alt.Quit(); }));
  WaitForDB();

  // Respond and make sure both run loop quit closures were called.
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));
  run_loop.Run();
  run_loop_alt.Run();
}

TEST_F(MediaFeedsServiceTest, FetcherShouldHandleReset) {
  const GURL feed_url("https://www.google.com/feed");

  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    // Check the feed and items are stored correctly.
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
    EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
              feeds[0]->last_fetch_result);

    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_EQ(GetExpectedItems(), items);
  }

  // Start fetching the feed but do not resolve the request.
  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(
      1, base::BindLambdaForTesting(
             [&](const std::string& ignored) { run_loop.Quit(); }));
  WaitForDB();

  // The last request was successful so we can hit the cache.
  EXPECT_FALSE(GetCurrentRequestHasBypassCacheFlag());

  // Reset the feed.
  GetMediaFeedsService()->ResetMediaFeed(
      url::Origin::Create(feed_url), media_feeds::mojom::ResetReason::kVisit);
  WaitForDB();

  {
    // Check the feed was reset.
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kVisit, feeds[0]->reset_reason);
    EXPECT_EQ(media_feeds::mojom::FetchResult::kNone,
              feeds[0]->last_fetch_result);

    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_TRUE(items.empty());
  }

  // Respond to the pending fetch.
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));
  run_loop.Run();
  WaitForDB();

  {
    // The feed should have still been reset since the fetch was started with
    // outdated information.
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kVisit, feeds[0]->reset_reason);
    EXPECT_EQ(media_feeds::mojom::FetchResult::kFailedDueToResetWhileInflight,
              feeds[0]->last_fetch_result);

    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_TRUE(items.empty());
  }

  // Start fetching the feed but do not resolve the request.
  base::RunLoop run_loop_alt;
  GetMediaFeedsService()->FetchMediaFeed(
      1, base::BindLambdaForTesting(
             [&](const std::string& ignored) { run_loop_alt.Quit(); }));
  WaitForDB();

  // The last request failed so we should not hit the cache.
  EXPECT_TRUE(GetCurrentRequestHasBypassCacheFlag());

  // Respond to the pending fetch.
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));
  run_loop_alt.Run();
  WaitForDB();

  {
    // The feed should have been successfully stored.
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
    EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
              feeds[0]->last_fetch_result);

    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_FALSE(items.empty());
  }
}

TEST_F(MediaFeedsServiceTest, ResetOnCookieChange_ExplicitDeletion_All) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some cookies on the feed URL and another URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(feed_url);
    cookie_urls.push_back(GURL("https://www.example.com"));
    CreateCookies(cookie_urls);
  }

  EXPECT_EQ(2u, DeleteCookies(network::mojom::CookieDeletionFilter::New()));
  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_SingleHostMatch) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some cookies on the feed URL and another URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(feed_url);
    cookie_urls.push_back(GURL("https://www.example.com"));
    CreateCookies(cookie_urls);
  }

  auto filter = network::mojom::CookieDeletionFilter::New();
  filter->url = feed_url;
  EXPECT_EQ(1u, DeleteCookies(std::move(filter)));
  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_SingleHostMatch_Insecure) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some cookies on the feed URL and another URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(GURL("http://www.google.com"));
    cookie_urls.push_back(GURL("https://www.example.com"));
    CreateCookies(cookie_urls);
  }

  auto filter = network::mojom::CookieDeletionFilter::New();
  filter->url = feed_url;
  EXPECT_EQ(1u, DeleteCookies(std::move(filter)));
  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest, ResetOnCookieChange_Expired) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some cookies on the feed URL and another URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(feed_url);
    cookie_urls.push_back(GURL("https://www.example.com"));
    CreateCookies(cookie_urls);
    CreateCookies(cookie_urls, false, true);
  }

  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_SingleHostNoMatch) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");
  const GURL alt_url("https://www.example.com");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  {
    // Store some cookies on the feed URL and another URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(feed_url);
    cookie_urls.push_back(alt_url);
    CreateCookies(cookie_urls);
  }

  auto filter = network::mojom::CookieDeletionFilter::New();
  filter->url = alt_url;
  EXPECT_EQ(1u, DeleteCookies(std::move(filter)));
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest, ResetOnCookieChange_Overwrite) {
  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  {
    // Store some cookies on the feed URL and another URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(feed_url);
    cookie_urls.push_back(GURL("https://www.example.com"));
    CreateCookies(cookie_urls);
    CreateCookies(cookie_urls);
    WaitForDB();
  }

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_DomainMatch) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some domain cookies.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(GURL("https://google.com"));
    CreateCookies(cookie_urls, true);
  }

  EXPECT_EQ(1u, DeleteCookies(network::mojom::CookieDeletionFilter::New()));
  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_DomainMatch_FullDomain) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some domain cookies.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(GURL("https://www.google.com"));
    CreateCookies(cookie_urls, true);
  }

  EXPECT_EQ(1u, DeleteCookies(network::mojom::CookieDeletionFilter::New()));
  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_DomainMatch_Insecure) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some domain cookies.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(GURL("http://google.com"));
    CreateCookies(cookie_urls, true);
  }

  EXPECT_EQ(1u, DeleteCookies(network::mojom::CookieDeletionFilter::New()));
  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_DomainMatch_NoMatch) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some domain cookies.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(GURL("https://example.com"));
    CreateCookies(cookie_urls, true);
  }

  EXPECT_EQ(1u, DeleteCookies(network::mojom::CookieDeletionFilter::New()));
  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_WithCookieFilter_Match) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  {
    // Store some cookies on the feed URL and another URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(feed_url);
    cookie_urls.push_back(GURL("https://www.example.com"));
    CreateCookies(cookie_urls);
  }

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  auto result = SuccessfulResultWithItems(GetExpectedItems(), 1);
  result.cookie_name_filter = "A";
  GetMediaHistoryService()->StoreMediaFeedFetchResult(std::move(result),
                                                      base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  EXPECT_EQ(2u, DeleteCookies(network::mojom::CookieDeletionFilter::New()));
  run_loop.Run();
  WaitForDB();

  {
    // The feed should have been reset because we have a cookie filter that
    // matches.
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_WithCookieFilter_NoMatch) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  {
    // Store some cookies on the feed URL and another URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(feed_url);
    cookie_urls.push_back(GURL("https://www.example.com"));
    CreateCookies(cookie_urls);
  }

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  auto result = SuccessfulResultWithItems(GetExpectedItems(), 1);
  result.cookie_name_filter = "B";
  GetMediaHistoryService()->StoreMediaFeedFetchResult(std::move(result),
                                                      base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  EXPECT_EQ(2u, DeleteCookies(network::mojom::CookieDeletionFilter::New()));
  run_loop.Run();
  WaitForDB();

  {
    // The feed should not have been reset because the cookie filter does not
    // match.
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_Creation_WithCookieFilter_Match) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  auto result = SuccessfulResultWithItems(GetExpectedItems(), 1);
  result.cookie_name_filter = "A";
  GetMediaHistoryService()->StoreMediaFeedFetchResult(std::move(result),
                                                      base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some cookies on the feed URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(feed_url);
    CreateCookies(cookie_urls);
  }

  run_loop.Run();
  WaitForDB();

  {
    // The feed should have been reset because we have a cookie filter that
    // matches.
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_Creation_WithCookieFilter_NoMatch) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  auto result = SuccessfulResultWithItems(GetExpectedItems(), 1);
  result.cookie_name_filter = "B";
  GetMediaHistoryService()->StoreMediaFeedFetchResult(std::move(result),
                                                      base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some cookies on the feed URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(feed_url);
    CreateCookies(cookie_urls);
  }

  run_loop.Run();
  WaitForDB();

  {
    // The feed should not have been reset because the cookie filter does not
    // match.
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_Creation_WithoutCookieFilter) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some cookies on the feed URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(feed_url);
    CreateCookies(cookie_urls);
  }

  run_loop.Run();
  WaitForDB();

  {
    // The feed should not have been reset because the cookie filter was not
    // present and therefore we only reset on deletion/expiration.
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest, DiscoverFeed_SafeSearch_Enabled) {
  const GURL feed_url("https://www.google.com/feed");

  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  // The feed should have been updated to be safe.
  auto feeds = GetMediaFeedsSync();
  ASSERT_EQ(1u, feeds.size());
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kSafe,
            feeds[0]->safe_search_result);
}

TEST_F(MediaFeedsServiceTest, DiscoverFeed_SafeSearch_Disabled) {
  const GURL feed_url("https://www.google.com/feed");

  SetSafeSearchEnabled(false);

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  auto feeds = GetMediaFeedsSync();
  ASSERT_EQ(1u, feeds.size());
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            feeds[0]->safe_search_result);
}

// Runs the Media Feeds tests from the spec. The file names start with success
// if the feed is valid and bad if they are not.
class MediaFeedsSpecTest : public MediaFeedsServiceTest,
                           public testing::WithParamInterface<std::string> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    MediaFeedsSpecTest,
    testing::Values("bad-member.json",
                    "bad-missing-logo-content-attributes.json",
                    "bad-provider-missing-name.json",
                    "success-allow-http-schema.org.json",
                    "success-empty.json",
                    "success-ignore-bad-content-attribute.json",
                    "success-member-opt-image-and-email.json",
                    "bad-image-as-url.json",
                    "bad-missing-logo.json",
                    "success-associated-origin-backwards-compat.json",
                    "success-full-feed.json",
                    "success-ignore-bad-element.json",
                    "success-minimal-feed.json"));

TEST_P(MediaFeedsSpecTest, RunOpenSourceTest) {
  const GURL feed_url("https://www.google.com/feed");

  // Get the path of the test data.
  base::FilePath file;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &file));
  file = file.Append(kMediaFeedsTestSpecDir).AppendASCII(GetParam());

  // Read the test file to a string.
  std::string test_data;
  base::ReadFileToString(file, &test_data);
  ASSERT_TRUE(test_data.size());

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Fetch the Media Feed.
  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(
      1, base::BindLambdaForTesting(
             [&](const std::string& ignored) { run_loop.Quit(); }));
  WaitForDB();
  ASSERT_TRUE(RespondToPendingFeedFetchWithData(feed_url, test_data));
  run_loop.Run();

  auto feeds = GetMediaFeedsSync();
  ASSERT_EQ(1u, feeds.size());

  if (base::Contains(GetParam(), "success")) {
    EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
              feeds[0]->last_fetch_result);
  } else {
    EXPECT_EQ(media_feeds::mojom::FetchResult::kInvalidFeed,
              feeds[0]->last_fetch_result);
  }
}

// FetchTopMediaFeeds should fetch a feed with enough watchtime on that origin
// even if it hasn't been fetched before.
TEST_F(MediaFeedsServiceTest, FetchTopMediaFeeds_SuccessNewFetch) {
  SetAutomaticSelectionEnabled();
  base::HistogramTester histogram_tester;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  SetBackgroundFetchingEnabled(true);
  task_environment()->RunUntilIdle();

  // FetchTopMediaFeeds should ignore the feed, as the origin does not have
  // enough watchtime.
  ASSERT_FALSE(RespondToPendingFeedFetch(feed_url));

  // Set the watchtime higher than the minimum threshold for top feeds.
  auto watchtime = base::TimeDelta::FromMinutes(45);
  content::MediaPlayerWatchTime watch_time(
      feed_url, feed_url.GetOrigin(), watchtime, base::TimeDelta(), true, true);
  GetMediaHistoryService()->SavePlayback(watch_time);
  WaitForDB();

  // Now that there is high watchtime, the fetch should occur the next time
  // background fetching happens.
  AdvanceTime(MediaFeedsService::kTimeBetweenBackgroundFetches);
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));

  auto feeds = GetMediaFeedsSync();

  EXPECT_EQ(1u, feeds.size());
  EXPECT_TRUE(feeds[0]->last_fetch_time_not_cache_hit);
  EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
            feeds[0]->last_fetch_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsFetcher::kFetchSizeKbHistogramName, 15, 1);
}

// Fetch top feeds should periodically fetch the feed from cache if available.
TEST_F(MediaFeedsServiceTest, FetchTopMediaFeeds_SuccessFromCache) {
  SetAutomaticSelectionEnabled();
  base::HistogramTester histogram_tester;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Set the watchtime higher than the minimum threshold for top feeds.
  auto watchtime = base::TimeDelta::FromMinutes(45);
  content::MediaPlayerWatchTime watch_time(
      feed_url, feed_url.GetOrigin(), watchtime, base::TimeDelta(), true, true);
  GetMediaHistoryService()->SavePlayback(watch_time);
  WaitForDB();

  // Fetch the Media Feed an initial time.
  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(
      1, base::BindLambdaForTesting(
             [&](const std::string& ignored) { run_loop.Quit(); }));
  WaitForDB();
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));
  run_loop.Run();

  // After some time, background fetching should refresh the feed from the
  // cached initial fetch.
  AdvanceTime(MediaFeedsService::kTimeBetweenBackgroundFetches);
  SetBackgroundFetchingEnabled(true);
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(GetCurrentRequestHasBypassCacheFlag());
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));

  auto feeds = GetMediaFeedsSync();

  EXPECT_EQ(1u, feeds.size());
  EXPECT_TRUE(feeds[0]->last_fetch_time_not_cache_hit);
  EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
            feeds[0]->last_fetch_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsFetcher::kFetchSizeKbHistogramName, 15, 2);
}

// FetchTopMediaFeeds should back off if the feed fails to fetch. But after 24
// hours, it should fetch regardless of failures, bypassing the cache.
TEST_F(MediaFeedsServiceTest, FetchTopMediaFeeds_BacksOffFailedFetches) {
  SetAutomaticSelectionEnabled();
  base::HistogramTester histogram_tester;
  const int times_to_fail = 10;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Set the watchtime higher than the minimum threshold for top feeds.
  auto watchtime = base::TimeDelta::FromMinutes(45);
  content::MediaPlayerWatchTime watch_time(
      feed_url, feed_url.GetOrigin(), watchtime, base::TimeDelta(), true, true);
  GetMediaHistoryService()->SavePlayback(watch_time);
  WaitForDB();

  // Fetch the Media Feed unsuccessfully several times.
  for (int i = 0; i < times_to_fail; i++) {
    base::RunLoop run_loop;
    GetMediaFeedsService()->FetchMediaFeed(
        1, base::BindLambdaForTesting(
               [&](const std::string& ignored) { run_loop.Quit(); }));
    WaitForDB();
    ASSERT_TRUE(RespondToPendingFeedFetchWithStatus(
        feed_url, net::HTTP_INTERNAL_SERVER_ERROR));
    run_loop.Run();
  }

  AdvanceTime(base::TimeDelta::FromHours(1));
  SetBackgroundFetchingEnabled(true);
  task_environment()->RunUntilIdle();

  // No fetch should happen because of the backoff from failures.
  ASSERT_FALSE(RespondToPendingFeedFetch(feed_url));

  // After 24 hours, the feed should be fetched regardless of failure count.
  AdvanceTime(MediaFeedsService::kTimeBetweenNonCachedBackgroundFetches);
  task_environment()->RunUntilIdle();

  // If we bypass failure count, we should also bypass cache.
  EXPECT_TRUE(GetCurrentRequestHasBypassCacheFlag());
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));

  auto feeds = GetMediaFeedsSync();

  EXPECT_EQ(1u, feeds.size());
  EXPECT_TRUE(feeds[0]->last_fetch_time_not_cache_hit);
  EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
            feeds[0]->last_fetch_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsFetcher::kFetchSizeKbHistogramName, 15, 1);
}

// After 24 hours, FetchTopMediaFeeds should fetch the feed and bypass the
// cache.
TEST_F(MediaFeedsServiceTest, FetchTopMediaFeeds_SuccessBypassCache) {
  SetAutomaticSelectionEnabled();
  base::HistogramTester histogram_tester;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Set the watchtime higher than the minimum threshold for top feeds.
  auto watchtime = base::TimeDelta::FromMinutes(45);
  content::MediaPlayerWatchTime watch_time(
      feed_url, feed_url.GetOrigin(), watchtime, base::TimeDelta(), true, true);
  GetMediaHistoryService()->SavePlayback(watch_time);
  WaitForDB();

  // Fetch the Media Feed an initial time.
  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(
      1, base::BindLambdaForTesting(
             [&](const std::string& ignored) { run_loop.Quit(); }));
  WaitForDB();
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));
  run_loop.Run();

  AdvanceTime(base::TimeDelta::FromHours(24));
  SetBackgroundFetchingEnabled(true);
  task_environment()->RunUntilIdle();

  // After a long time between fetches, we should bypass the cache.
  EXPECT_TRUE(GetCurrentRequestHasBypassCacheFlag());
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));

  auto feeds = GetMediaFeedsSync();

  EXPECT_EQ(1u, feeds.size());
  EXPECT_TRUE(feeds[0]->last_fetch_time_not_cache_hit);
  EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
            feeds[0]->last_fetch_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsFetcher::kFetchSizeKbHistogramName, 15, 2);
}

// After a feed reset, FetchTopMediaFeeds should fetch anyway.
TEST_F(MediaFeedsServiceTest, FetchTopMediaFeeds_SuccessResetFeed) {
  SetAutomaticSelectionEnabled();
  base::HistogramTester histogram_tester;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  auto watchtime = base::TimeDelta::FromMinutes(45);
  content::MediaPlayerWatchTime watch_time(
      feed_url, feed_url.GetOrigin(), watchtime, base::TimeDelta(), true, true);
  GetMediaHistoryService()->SavePlayback(watch_time);
  WaitForDB();

  // Fetch the Media Feed.
  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(
      1, base::BindLambdaForTesting(
             [&](const std::string& ignored) { run_loop.Quit(); }));
  WaitForDB();
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));
  run_loop.Run();

  GetMediaFeedsService()->ResetMediaFeed(url::Origin::Create(feed_url),
                                         mojom::ResetReason::kVisit);
  WaitForDB();

  SetBackgroundFetchingEnabled(true);
  task_environment()->RunUntilIdle();
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));

  auto feeds = GetMediaFeedsSync();

  EXPECT_EQ(1u, feeds.size());
  EXPECT_TRUE(feeds[0]->last_fetch_time_not_cache_hit);
  EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
            feeds[0]->last_fetch_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsFetcher::kFetchSizeKbHistogramName, 15, 2);
}

// After enabling the pref, top feeds should fetch immediately and then again
// after 15 minutes.
TEST_F(MediaFeedsServiceTest, FetchTopMediaFeeds_SuccessRepeatsPeriodically) {
  SetAutomaticSelectionEnabled();
  base::HistogramTester histogram_tester;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Set the watchtime higher than the minimum threshold for top feeds.
  auto watchtime = base::TimeDelta::FromMinutes(45);
  content::MediaPlayerWatchTime watch_time(
      feed_url, feed_url.GetOrigin(), watchtime, base::TimeDelta(), true, true);
  GetMediaHistoryService()->SavePlayback(watch_time);
  WaitForDB();

  // Once we set this, background fetching should start automatically.
  SetBackgroundFetchingEnabled(true);
  task_environment()->RunUntilIdle();

  // There should be only one fetch ready.
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));
  ASSERT_FALSE(RespondToPendingFeedFetch(feed_url));

  // Wait 15 minutes and the next fetch should be queued up.
  AdvanceTime(MediaFeedsService::kTimeBetweenBackgroundFetches);
  task_environment()->RunUntilIdle();

  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));

  auto feeds = GetMediaFeedsSync();

  EXPECT_EQ(1u, feeds.size());
  EXPECT_TRUE(feeds[0]->last_fetch_time_not_cache_hit);
  EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
            feeds[0]->last_fetch_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsFetcher::kFetchSizeKbHistogramName, 15, 2);
}

// FetchTopMediaFeeds should fetch a feed with enough watchtime on that origin
// even if it hasn't been fetched before.
TEST_F(MediaFeedsServiceTest, FetchTopMediaFeeds_DisableAutoSelection) {
  base::HistogramTester histogram_tester;

  const GURL feed_url_a("https://www.google.com/feed");
  const GURL feed_url_b("https://www.google.co.uk/feed");

  // Store a couple of Media Feeds.
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url_a);
  GetMediaFeedsService()->DiscoverMediaFeed(feed_url_b);
  WaitForDB();

  // The first feed we should opt into.
  GetMediaHistoryService()->UpdateFeedUserStatus(
      1, media_feeds::mojom::FeedUserStatus::kEnabled);
  WaitForDB();

  SetBackgroundFetchingEnabled(true);
  task_environment()->RunUntilIdle();

  // The first feed should be fetched and the second one should be ignored since
  // the user has not enabled it.
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url_a));
  ASSERT_FALSE(RespondToPendingFeedFetch(feed_url_b));

  auto feeds = GetMediaFeedsSync();
  ASSERT_EQ(2u, feeds.size());
  EXPECT_TRUE(feeds[0]->last_fetch_time_not_cache_hit);
  EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
            feeds[0]->last_fetch_result);
  EXPECT_EQ(media_feeds::mojom::FetchResult::kNone,
            feeds[1]->last_fetch_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsFetcher::kFetchSizeKbHistogramName, 15, 1);
}

TEST_F(MediaFeedsServiceTest, AggregateWatchtimeHistogram) {
  base::HistogramTester histogram_tester;

  task_environment()->RunUntilIdle();

  const GURL feed_url("https://www.google.com/feed");

  GetMediaFeedsService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  content::MediaPlayerWatchTime watch_time(feed_url, feed_url.GetOrigin(),
                                           base::TimeDelta::FromMinutes(30),
                                           base::TimeDelta(), true, true);
  GetMediaHistoryService()->SavePlayback(watch_time);
  WaitForDB();

  GetMediaFeedsService()->RecordFeedWatchtimes();
  WaitForDB();

  histogram_tester.ExpectUniqueTimeSample(
      MediaFeedsService::kAggregateWatchtimeHistogramName,
      base::TimeDelta::FromMinutes(30), 1);
}

}  // namespace media_feeds
