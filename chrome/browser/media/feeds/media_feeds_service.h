// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_SERVICE_H_
#define CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_SERVICE_H_

#include <memory>
#include <set>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/media/feeds/media_feeds_converter.h"
#include "chrome/browser/media/feeds/media_feeds_fetcher.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;
class GURL;

namespace base {
class Clock;
}

namespace safe_search_api {
enum class Classification;
class URLChecker;
}  // namespace safe_search_api

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace media_feeds {

namespace {
class CookieChangeListener;
}  // namespace

class MediaFeedsService : public KeyedService {
 public:
  static const char kAggregateWatchtimeHistogramName[];
  static const char kSafeSearchResultHistogramName[];

  // Time to wait between background fetch delayed tasks.
  static constexpr base::TimeDelta kTimeBetweenBackgroundFetches =
      base::TimeDelta::FromMinutes(15);

  // If this much time has passed since the last time we got a non-cached, fresh
  // version of the feed, we should bypass the cache on the next background
  // fetch of the feed.
  static constexpr base::TimeDelta kTimeBetweenNonCachedBackgroundFetches =
      base::TimeDelta::FromHours(24);

  using FetchMediaFeedCallback =
      base::OnceCallback<void(const std::string& logs)>;

  explicit MediaFeedsService(Profile* profile);
  ~MediaFeedsService() override;
  MediaFeedsService(const MediaFeedsService& t) = delete;
  MediaFeedsService& operator=(const MediaFeedsService&) = delete;

  static bool IsEnabled();

  // Returns the instance attached to the given |profile|.
  static MediaFeedsService* Get(Profile* profile);

  // Register profile prefs in the pref registry.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Checks the list of pending items against the Safe Search API and stores
  // the result.
  void CheckItemsAgainstSafeSearch(
      media_history::MediaHistoryKeyedService::PendingSafeSearchCheckList list);

  // Creates a SafeSearch URLChecker using a given URLLoaderFactory for testing.
  void SetSafeSearchURLCheckerForTest(
      std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker);

  // Stores a callback to be called once we have completed all inflight checks.
  void SetSafeSearchCompletionCallbackForTest(base::RepeatingClosure callback);

  // Fetches a media feed with the given ID and then store it in the
  // feeds table in media history. Runs the given callback after storing. The
  // fetch will be skipped if another fetch is currently ongoing.
  // If the feed is not supplied, it will be looked up in media history store in
  // order to get details related to fetching.
  void FetchMediaFeed(const int64_t feed_id,
                      const bool bypass_cache,
                      media_feeds::mojom::MediaFeedPtr feed,
                      FetchMediaFeedCallback callback);
  void FetchMediaFeed(int64_t feed_id, FetchMediaFeedCallback callback);

  // Stores a callback to be called once we have completed all inflight checks.
  void SetCookieChangeCallbackForTest(base::OnceClosure callback);

  // Saves a newly discovered media feed.
  void DiscoverMediaFeed(const GURL& url);

  // Resets a Media Feed by deleting any items and resetting it to defaults. If
  // |include_subdomains| is true then this will reset any feeds on any
  // subdomain of |origin|.
  void ResetMediaFeed(const url::Origin& origin,
                      media_feeds::mojom::ResetReason reason);

  // Check the list of discovered feeds and fetch a collection of those with the
  // highest watchtime. This should be called periodically in the background.
  void FetchTopMediaFeeds(base::OnceClosure callback);

  bool HasCookieObserverForTest() const;

  void EnsureCookieObserver();

  void RecordFeedWatchtimes();

 private:
  friend class MediaFeedsServiceTest;

  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }

  bool AddInflightSafeSearchCheck(
      const media_history::MediaHistoryKeyedService::SafeSearchID id,
      const std::set<GURL>& urls);

  void CheckForSafeSearch(
      const media_history::MediaHistoryKeyedService::SafeSearchID id,
      const GURL& url);

  void OnCheckURLDone(
      const media_history::MediaHistoryKeyedService::SafeSearchID id,
      const GURL& original_url,
      const GURL& url,
      safe_search_api::Classification classification,
      bool uncertain);

  void MaybeCallCompletionCallback();

  bool IsBackgroundFetchingEnabled() const;

  bool IsSafeSearchCheckingEnabled() const;

  void OnGotTopFeeds(base::OnceClosure callback,
                     std::vector<media_feeds::mojom::MediaFeedPtr> feeds);

  void OnGotFetchDetails(
      const int64_t feed_id,
      bool bypass_cache,
      base::Optional<
          media_history::MediaHistoryKeyedService::MediaFeedFetchDetails>
          details);

  void OnFetchResponse(
      int64_t feed_id,
      base::Optional<base::UnguessableToken> reset_token,
      media_history::MediaHistoryKeyedService::MediaFeedFetchResult result);

  void OnCompleteFetch(const int64_t feed_id,
                       const bool has_items,
                       const std::string& error_logs);

  void OnSafeSearchPrefChanged();

  void OnBackgroundFetchingPrefChanged();

  void OnResetOriginFromCookie(const url::Origin& origin,
                               const bool include_subdomains,
                               const std::string& name,
                               const net::CookieChangeCause& cause);

  void OnDiscoveredFeed();

  void OnGotFeedsForMetrics(
      std::vector<media_feeds::mojom::MediaFeedPtr> feeds);

  // Settings related to fetching a feed in the background.
  struct BackgroundFetchFeedSettings {
    // Whether this feed should be fetched now.
    bool should_fetch;
    // Whether to use cached data (false) or bypass and get fresh data (true).
    bool bypass_cache;
  };

  // Returns whether the feed should be fetched in the background as a top feed.
  BackgroundFetchFeedSettings GetBackgroundFetchFeedSettings(
      const media_feeds::mojom::MediaFeedPtr& feed);

  media_history::MediaHistoryKeyedService* GetMediaHistoryService();

  scoped_refptr<::network::SharedURLLoaderFactory>
  GetURLLoaderFactoryForFetcher();

  PrefChangeRegistrar pref_change_registrar_;

  struct InflightFeedFetch {
    InflightFeedFetch(std::unique_ptr<MediaFeedsFetcher> fetcher,
                      FetchMediaFeedCallback callback);
    ~InflightFeedFetch();
    InflightFeedFetch(InflightFeedFetch&& t);
    InflightFeedFetch(const InflightFeedFetch&) = delete;
    InflightFeedFetch& operator=(const InflightFeedFetch&) = delete;

    std::vector<FetchMediaFeedCallback> callbacks;

    std::unique_ptr<MediaFeedsFetcher> fetcher;
  };

  std::map<int64_t, InflightFeedFetch> fetches_;

  struct InflightSafeSearchCheck {
    explicit InflightSafeSearchCheck(const std::set<GURL>& urls);
    ~InflightSafeSearchCheck();
    InflightSafeSearchCheck(const InflightSafeSearchCheck&) = delete;
    InflightSafeSearchCheck& operator=(const InflightSafeSearchCheck&) = delete;

    std::set<GURL> pending;

    bool is_safe = false;
    bool is_unsafe = false;
    bool is_uncertain = false;
  };

  std::map<media_history::MediaHistoryKeyedService::SafeSearchID,
           std::unique_ptr<InflightSafeSearchCheck>>
      inflight_safe_search_checks_;

  base::RepeatingClosure safe_search_completion_callback_;

  scoped_refptr<::network::SharedURLLoaderFactory>
      test_url_loader_factory_for_fetcher_;

  std::unique_ptr<CookieChangeListener> cookie_change_listener_;

  base::OnceClosure cookie_change_callback_;

  std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker_;
  Profile* const profile_;

  // An internal clock for testing.
  base::Clock* clock_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<MediaFeedsService> weak_factory_{this};
};

}  // namespace media_feeds

#endif  // CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_SERVICE_H_
