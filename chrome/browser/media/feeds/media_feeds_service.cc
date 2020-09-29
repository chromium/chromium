// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_service.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_checker.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time_to_iso8601.h"
#include "chrome/browser/media/feeds/media_feeds_converter.h"
#include "chrome/browser/media/feeds/media_feeds_fetcher.h"
#include "chrome/browser/media/feeds/media_feeds_service_factory.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-forward.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-shared.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/safe_search_api/safe_search/safe_search_url_checker_client.h"
#include "components/safe_search_api/url_checker.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

namespace media_feeds {

namespace {

GURL Normalize(const GURL& url) {
  GURL normalized_url = url;
  GURL::Replacements replacements;
  // Strip username, password, query, and ref.
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

media_history::MediaHistoryKeyedService::MediaFeedFetchDetails
FetchDetailsFromFeed(const mojom::MediaFeedPtr& media_feed) {
  media_history::MediaHistoryKeyedService::MediaFeedFetchDetails details;
  details.url = media_feed->url;
  details.last_fetch_result = media_feed->last_fetch_result;
  details.reset_token = media_feed->reset_token;
  return details;
}

class CookieChangeListener : public network::mojom::CookieChangeListener {
 public:
  using CookieCallback =
      base::RepeatingCallback<void(const url::Origin&,
                                   const bool /* include_subdomains */,
                                   const std::string& /* name */,
                                   const net::CookieChangeCause& /* cause */)>;

  CookieChangeListener(Profile* profile, CookieCallback callback)
      : profile_(profile), callback_(std::move(callback)) {
    DCHECK(profile);
    DCHECK(!profile->IsOffTheRecord());
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    MaybeStartListening();
  }

  ~CookieChangeListener() override = default;
  CookieChangeListener(const CookieChangeListener& t) = delete;
  CookieChangeListener& operator=(const CookieChangeListener&) = delete;

  // network::mojom::CookieChangeListener:
  void OnCookieChange(const net::CookieChangeInfo& change) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (change.cookie.Domain().empty())
      return;

    auto url =
        net::cookie_util::CookieOriginToURL(change.cookie.Domain(), true);
    DCHECK(url.SchemeIsCryptographic());

    callback_.Run(url::Origin::Create(url), change.cookie.IsDomainCookie(),
                  change.cookie.Name(), change.cause);
  }

 private:
  void MaybeStartListening() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(profile_);

    auto* storage =
        content::BrowserContext::GetDefaultStoragePartition(profile_);
    if (!storage)
      return;

    auto* cookie_manager = storage->GetCookieManagerForBrowserProcess();
    if (!cookie_manager)
      return;

    cookie_manager->AddGlobalChangeListener(
        receiver_.BindNewPipeAndPassRemote());
    receiver_.set_disconnect_handler(base::BindOnce(
        &CookieChangeListener::OnConnectionError, base::Unretained(this)));
  }

  void OnConnectionError() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    receiver_.reset();
    MaybeStartListening();
  }

  Profile* const profile_;
  CookieCallback const callback_;

  THREAD_CHECKER(thread_checker_);

  mojo::Receiver<network::mojom::CookieChangeListener> receiver_{this};
};

}  // namespace

const char MediaFeedsService::kAggregateWatchtimeHistogramName[] =
    "Media.Feeds.AggregateWatchtime";

const char MediaFeedsService::kSafeSearchResultHistogramName[] =
    "Media.Feeds.SafeSearch.Result";

// static
constexpr base::TimeDelta MediaFeedsService::kTimeBetweenBackgroundFetches;

// static
constexpr base::TimeDelta
    MediaFeedsService::kTimeBetweenNonCachedBackgroundFetches;

// The maximum number of feeds to fetch when getting the top feeds.
const int kMaxTopFeedsToFetch = 5;

// The minimum watchtime required on the feed's origin before a feed can be
// considered a top feed.
constexpr base::TimeDelta kTopFeedsMinWatchTime =
    base::TimeDelta::FromMinutes(30);

MediaFeedsService::MediaFeedsService(Profile* profile)
    : profile_(profile), clock_(base::DefaultClock::GetInstance()) {
  DCHECK(!profile->IsOffTheRecord());

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kMediaFeedsSafeSearchEnabled,
      base::BindRepeating(&MediaFeedsService::OnSafeSearchPrefChanged,
                          weak_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      prefs::kMediaFeedsBackgroundFetching,
      base::BindRepeating(&MediaFeedsService::OnBackgroundFetchingPrefChanged,
                          weak_factory_.GetWeakPtr()));

  if (IsBackgroundFetchingEnabled()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaFeedsService::FetchTopMediaFeeds,
                       weak_factory_.GetWeakPtr(), base::OnceClosure()));
  }

  // Wrapping in PostTask is needed to avoid a crash in the tests.
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&MediaFeedsService::RecordFeedWatchtimes,
                                weak_factory_.GetWeakPtr()));
}

void MediaFeedsService::RecordFeedWatchtimes() {
  GetMediaHistoryService()->GetMediaFeeds(
      media_history::MediaHistoryKeyedService::GetMediaFeedsRequest::
          CreateTopFeedsForFetch(std::numeric_limits<unsigned>::max(),
                                 base::TimeDelta()),
      base::BindOnce(&MediaFeedsService::OnGotFeedsForMetrics,
                     weak_factory_.GetWeakPtr()));
}

// static
MediaFeedsService* MediaFeedsService::Get(Profile* profile) {
  return MediaFeedsServiceFactory::GetForProfile(profile);
}

MediaFeedsService::~MediaFeedsService() = default;

bool MediaFeedsService::IsEnabled() {
  return base::FeatureList::IsEnabled(media::kMediaFeeds);
}

// static
void MediaFeedsService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kMediaFeedsBackgroundFetching, false);
  registry->RegisterBooleanPref(prefs::kMediaFeedsSafeSearchEnabled, false);
}

void MediaFeedsService::CheckItemsAgainstSafeSearch(
    media_history::MediaHistoryKeyedService::PendingSafeSearchCheckList list) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!IsSafeSearchCheckingEnabled()) {
    MaybeCallCompletionCallback();
    return;
  }

  for (auto& check : list) {
    if (!AddInflightSafeSearchCheck(check->id, check->urls))
      continue;

    for (auto& url : check->urls)
      CheckForSafeSearch(check->id, url);
  }
}

void MediaFeedsService::SetSafeSearchURLCheckerForTest(
    std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker) {
  safe_search_url_checker_ = std::move(safe_search_url_checker);
}

void MediaFeedsService::SetSafeSearchCompletionCallbackForTest(
    base::RepeatingClosure callback) {
  safe_search_completion_callback_ = std::move(callback);
}

void MediaFeedsService::FetchMediaFeed(const int64_t feed_id,
                                       const bool bypass_cache,
                                       media_feeds::mojom::MediaFeedPtr feed,
                                       FetchMediaFeedCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Skip the fetch if there is already an ongoing fetch for this feed. However,
  // add the callback so it will resolve when the ongoing fetch is complete.
  if (base::Contains(fetches_, feed_id)) {
    fetches_.at(feed_id).callbacks.push_back(std::move(callback));
    return;
  }

  fetches_.emplace(feed_id,
                   InflightFeedFetch(std::make_unique<MediaFeedsFetcher>(
                                         GetURLLoaderFactoryForFetcher()),
                                     std::move(callback)));

  if (feed) {
    OnGotFetchDetails(feed_id, bypass_cache, FetchDetailsFromFeed(feed));
  } else {
    GetMediaHistoryService()->GetMediaFeedFetchDetails(
        feed_id,
        base::BindOnce(&MediaFeedsService::OnGotFetchDetails,
                       weak_factory_.GetWeakPtr(), feed_id, bypass_cache));
  }
}

void MediaFeedsService::FetchMediaFeed(int64_t feed_id,
                                       FetchMediaFeedCallback callback) {
  FetchMediaFeed(feed_id, false, nullptr, std::move(callback));
}

media_history::MediaHistoryKeyedService*
MediaFeedsService::GetMediaHistoryService() {
  DCHECK(profile_);
  media_history::MediaHistoryKeyedService* service =
      media_history::MediaHistoryKeyedServiceFactory::GetForProfile(profile_);
  DCHECK(service);
  return service;
}

bool MediaFeedsService::AddInflightSafeSearchCheck(
    const media_history::MediaHistoryKeyedService::SafeSearchID id,
    const std::set<GURL>& urls) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (base::Contains(inflight_safe_search_checks_, id))
    return false;

  inflight_safe_search_checks_.emplace(
      id, std::make_unique<InflightSafeSearchCheck>(urls));

  return true;
}

void MediaFeedsService::CheckForSafeSearch(
    const media_history::MediaHistoryKeyedService::SafeSearchID id,
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsSafeSearchCheckingEnabled());

  if (!safe_search_url_checker_) {
    // TODO(https://crbug.com/1066643): Add a UI toggle to turn this feature on.
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("media_feeds_checker", R"(
          semantics {
            sender: "Media Feeds Safe Search Checker"
            description:
              "Media Feeds are feeds of personalized media recommendations "
              "that are fetched from media websites and displayed to the user. "
              "These feeds are discovered automatically on websites that embed "
              "them. Chrome will then periodically fetch the feeds in the "
              "background. This checker will check the media recommendations "
              "against the Google SafeSearch API to ensure the recommendations "
              "are safe and do not contain any inappropriate content."
            trigger:
              "Having a discovered feed that has not been fetched recently. "
              "Feeds are discovered when the browser visits any webpage with a "
              "feed link element in the header. Chrome will only fetch feeds "
              "from a website that meets certain media heuristics. This is to "
              "limit Media Feeds to only sites the user watches videos on."
            data: "URL to be checked."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "This feature is off by default and cannot be controlled in "
              "settings."
            chrome_policy {
              SavingBrowserHistoryDisabled {
                policy_options {mode: MANDATORY}
                SavingBrowserHistoryDisabled: false
              }
            }
          })");

    safe_search_url_checker_ = std::make_unique<safe_search_api::URLChecker>(
        std::make_unique<safe_search_api::SafeSearchURLCheckerClient>(
            content::BrowserContext::GetDefaultStoragePartition(profile_)
                ->GetURLLoaderFactoryForBrowserProcess(),
            traffic_annotation));
  }

  safe_search_url_checker_->CheckURL(
      Normalize(url), base::BindOnce(&MediaFeedsService::OnCheckURLDone,
                                     base::Unretained(this), id, url));
}

void MediaFeedsService::SetCookieChangeCallbackForTest(
    base::OnceClosure callback) {
  cookie_change_callback_ = std::move(callback);
}

void MediaFeedsService::DiscoverMediaFeed(const GURL& url) {
  GetMediaHistoryService()->DiscoverMediaFeed(
      url, base::BindOnce(&MediaFeedsService::OnDiscoveredFeed,
                          weak_factory_.GetWeakPtr()));
}

void MediaFeedsService::ResetMediaFeed(const url::Origin& origin,
                                       media_feeds::mojom::ResetReason reason) {
  GetMediaHistoryService()->ResetMediaFeed(origin, reason);
}

void MediaFeedsService::FetchTopMediaFeeds(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!IsBackgroundFetchingEnabled())
    return;

  // If the user has opted into auto selection of media feeds then we should get
  // the top media feeds based on heuristics. Otherwise, we should fallback to
  // feeds the user has opted into.
  if (profile_->GetPrefs()->GetBoolean(
          kaleidoscope::prefs::kKaleidoscopeAutoSelectMediaFeeds)) {
    GetMediaHistoryService()->GetMediaFeeds(
        media_history::MediaHistoryKeyedService::GetMediaFeedsRequest::
            CreateTopFeedsForFetch(kMaxTopFeedsToFetch, kTopFeedsMinWatchTime),
        base::BindOnce(&MediaFeedsService::OnGotTopFeeds,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    GetMediaHistoryService()->GetMediaFeeds(
        media_history::MediaHistoryKeyedService::GetMediaFeedsRequest::
            CreateSelectedFeedsForFetch(),
        base::BindOnce(&MediaFeedsService::OnGotTopFeeds,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void MediaFeedsService::OnGotTopFeeds(
    base::OnceClosure callback,
    std::vector<media_feeds::mojom::MediaFeedPtr> feeds) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  int feeds_fetched = 0;
  auto it = feeds.begin();

  while (feeds_fetched < kMaxTopFeedsToFetch && it != feeds.end()) {
    auto& feed = *it;

    auto background_fetch_feed_settings = GetBackgroundFetchFeedSettings(feed);
    if (background_fetch_feed_settings.should_fetch) {
      auto feed_id = feed->id;
      FetchMediaFeed(
          feed_id, /*bypass_cache=*/background_fetch_feed_settings.bypass_cache,
          std::move(feed), base::DoNothing());
      feeds_fetched++;
    }

    ++it;
  }

  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MediaFeedsService::FetchTopMediaFeeds,
                     weak_factory_.GetWeakPtr(), base::OnceClosure()),
      kTimeBetweenBackgroundFetches);

  if (callback)
    std::move(callback).Run();
}

void MediaFeedsService::OnCheckURLDone(
    const media_history::MediaHistoryKeyedService::SafeSearchID id,
    const GURL& original_url,
    const GURL& url,
    safe_search_api::Classification classification,
    bool uncertain) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsSafeSearchCheckingEnabled());

  // Get the inflight safe search check data.
  auto it = inflight_safe_search_checks_.find(id);
  if (it == inflight_safe_search_checks_.end())
    return;

  // Remove the url we just checked from the pending list.
  auto* check = it->second.get();
  check->pending.erase(original_url);

  if (uncertain) {
    check->is_uncertain = true;
  } else if (classification == safe_search_api::Classification::SAFE) {
    check->is_safe = true;
  } else if (classification == safe_search_api::Classification::UNSAFE) {
    check->is_unsafe = true;
  }

  // If we have no more URLs to check or the result is unsafe then we should
  // store now.
  if (!(check->pending.empty() || check->is_unsafe))
    return;

  auto result = media_feeds::mojom::SafeSearchResult::kUnknown;
  if (check->is_unsafe) {
    result = media_feeds::mojom::SafeSearchResult::kUnsafe;
  } else if (check->is_safe && !check->is_uncertain) {
    result = media_feeds::mojom::SafeSearchResult::kSafe;
  }

  std::map<media_history::MediaHistoryKeyedService::SafeSearchID,
           media_feeds::mojom::SafeSearchResult>
      results;
  results.emplace(id, result);
  inflight_safe_search_checks_.erase(id);

  auto* service =
      media_history::MediaHistoryKeyedServiceFactory::GetForProfile(profile_);
  DCHECK(service);
  service->StoreMediaFeedItemSafeSearchResults(results);

  base::UmaHistogramEnumeration(kSafeSearchResultHistogramName, result);

  MaybeCallCompletionCallback();
}

void MediaFeedsService::MaybeCallCompletionCallback() {
  if (inflight_safe_search_checks_.empty() &&
      !safe_search_completion_callback_.is_null()) {
    safe_search_completion_callback_.Run();
  }
}

bool MediaFeedsService::IsBackgroundFetchingEnabled() const {
  return base::FeatureList::IsEnabled(media::kMediaFeedsBackgroundFetching) &&
         profile_->GetPrefs()->GetBoolean(prefs::kMediaFeedsBackgroundFetching);
}

bool MediaFeedsService::IsSafeSearchCheckingEnabled() const {
  return base::FeatureList::IsEnabled(media::kMediaFeedsSafeSearch) &&
         profile_->GetPrefs()->GetBoolean(prefs::kMediaFeedsSafeSearchEnabled);
}

MediaFeedsService::InflightFeedFetch::InflightFeedFetch(
    std::unique_ptr<MediaFeedsFetcher> fetcher,
    FetchMediaFeedCallback callback)
    : fetcher(std::move(fetcher)) {
  callbacks.push_back(std::move(callback));
}

MediaFeedsService::InflightFeedFetch::~InflightFeedFetch() = default;

MediaFeedsService::InflightFeedFetch::InflightFeedFetch(InflightFeedFetch&& t) =
    default;

MediaFeedsService::InflightSafeSearchCheck::InflightSafeSearchCheck(
    const std::set<GURL>& urls)
    : pending(urls) {}

MediaFeedsService::InflightSafeSearchCheck::~InflightSafeSearchCheck() =
    default;

void MediaFeedsService::OnGotFetchDetails(
    const int64_t feed_id,
    bool bypass_cache,
    base::Optional<
        media_history::MediaHistoryKeyedService::MediaFeedFetchDetails>
        details) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(base::Contains(fetches_, feed_id));

  if (!details.has_value()) {
    OnCompleteFetch(feed_id, false, "Missing feed fetch details");
    return;
  }

  bool should_bypass_cache =
      bypass_cache ||
      details->last_fetch_result != media_feeds::mojom::FetchResult::kSuccess;

  fetches_.at(feed_id).fetcher->FetchFeed(
      details->url, should_bypass_cache,
      // Use of unretained is safe because the callback is owned
      // by fetcher_, which will not outlive this.
      base::BindOnce(&MediaFeedsService::OnFetchResponse,
                     base::Unretained(this), feed_id, details->reset_token));
}

void MediaFeedsService::OnFetchResponse(
    int64_t feed_id,
    base::Optional<base::UnguessableToken> reset_token,
    media_history::MediaHistoryKeyedService::MediaFeedFetchResult result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (result.gone) {
    GetMediaHistoryService()->DeleteMediaFeed(
        feed_id, base::BindOnce(&MediaFeedsService::OnCompleteFetch,
                                weak_factory_.GetWeakPtr(), feed_id, false,
                                "The server returned 410 Gone which resulted "
                                "in the feed being deleted."));
    return;
  }

  result.feed_id = feed_id;
  result.reset_token = reset_token;

  const bool has_items = !result.items.empty();
  std::string error_logs;
  error_logs.swap(result.error_logs);

  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      std::move(result), base::BindOnce(&MediaFeedsService::OnCompleteFetch,
                                        weak_factory_.GetWeakPtr(), feed_id,
                                        has_items, error_logs));
}

void MediaFeedsService::OnCompleteFetch(const int64_t feed_id,
                                        const bool has_items,
                                        const std::string& error_logs) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(base::Contains(fetches_, feed_id));

  for (auto& callback : fetches_.at(feed_id).callbacks) {
    std::move(callback).Run(error_logs);
  }

  fetches_.erase(feed_id);

  // If safe search checking is enabled then we should check the new feed items
  // against the Safe Search API.
  if (has_items && IsSafeSearchCheckingEnabled()) {
    GetMediaHistoryService()->GetPendingSafeSearchCheckMediaFeedItems(
        base::BindOnce(&MediaFeedsService::CheckItemsAgainstSafeSearch,
                       weak_factory_.GetWeakPtr()));
  }
}

void MediaFeedsService::OnSafeSearchPrefChanged() {
  if (!IsSafeSearchCheckingEnabled())
    return;

  GetMediaHistoryService()->GetPendingSafeSearchCheckMediaFeedItems(
      base::BindOnce(&MediaFeedsService::CheckItemsAgainstSafeSearch,
                     weak_factory_.GetWeakPtr()));
}

void MediaFeedsService::OnBackgroundFetchingPrefChanged() {
  if (!IsBackgroundFetchingEnabled())
    return;

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaFeedsService::FetchTopMediaFeeds,
                     weak_factory_.GetWeakPtr(), base::OnceClosure()));
}

void MediaFeedsService::OnResetOriginFromCookie(
    const url::Origin& origin,
    const bool include_subdomains,
    const std::string& name,
    const net::CookieChangeCause& cause) {
  GetMediaHistoryService()->ResetMediaFeedDueToCookies(
      origin, include_subdomains, name, cause);

  if (!cookie_change_callback_.is_null())
    std::move(cookie_change_callback_).Run();
}

MediaFeedsService::BackgroundFetchFeedSettings
MediaFeedsService::GetBackgroundFetchFeedSettings(
    const media_feeds::mojom::MediaFeedPtr& feed) {
  BackgroundFetchFeedSettings settings;
  settings.should_fetch = false;
  settings.bypass_cache = false;

  // Fetches should be spaced with exponential backoff based on how many
  // sequential times the fetch has failed.
  if (feed->last_fetch_time.has_value()) {
    // TODO(crbug.com/1064751): Consider using net::BackoffEntry for this.
    base::Time next_fetch_time =
        feed->last_fetch_time.value() +
        base::TimeDelta::FromMinutes(kTimeBetweenBackgroundFetches.InMinutes() *
                                     pow(2, feed->fetch_failed_count));
    settings.should_fetch = next_fetch_time < clock_->Now();
  }

  // If we haven't gotten a non-cached version of the feed in a while, we should
  // fetch.
  if (feed->last_fetch_time_not_cache_hit.has_value()) {
    base::Time next_fetch_time = feed->last_fetch_time_not_cache_hit.value() +
                                 kTimeBetweenNonCachedBackgroundFetches;
    if (next_fetch_time < clock_->Now()) {
      settings.should_fetch = true;
      settings.bypass_cache = true;
    }
  }

  // If the feed has never been fetched, we should fetch it.
  if (!feed->last_fetch_time.has_value()) {
    settings.should_fetch = true;
  }

  // If the feed has been reset, we should fetch and ignore the cache.
  if (feed->reset_reason != mojom::ResetReason::kNone) {
    settings.should_fetch = true;
    settings.bypass_cache = true;
  }

  return settings;
}

scoped_refptr<::network::SharedURLLoaderFactory>
MediaFeedsService::GetURLLoaderFactoryForFetcher() {
  if (test_url_loader_factory_for_fetcher_)
    return test_url_loader_factory_for_fetcher_;

  return content::BrowserContext::GetDefaultStoragePartition(profile_)
      ->GetURLLoaderFactoryForBrowserProcess();
}

bool MediaFeedsService::HasCookieObserverForTest() const {
  return cookie_change_listener_ != nullptr;
}

void MediaFeedsService::OnDiscoveredFeed() {
  if (!IsSafeSearchCheckingEnabled())
    return;

  GetMediaHistoryService()->GetPendingSafeSearchCheckMediaFeedItems(
      base::BindOnce(&MediaFeedsService::CheckItemsAgainstSafeSearch,
                     weak_factory_.GetWeakPtr()));
}

void MediaFeedsService::EnsureCookieObserver() {
  if (cookie_change_listener_)
    return;

  cookie_change_listener_ = std::make_unique<CookieChangeListener>(
      profile_, base::BindRepeating(&MediaFeedsService::OnResetOriginFromCookie,
                                    base::Unretained(this)));
}

void MediaFeedsService::OnGotFeedsForMetrics(
    std::vector<media_feeds::mojom::MediaFeedPtr> feeds) {
  for (const auto& feed : feeds) {
    base::UmaHistogramCustomTimes(kAggregateWatchtimeHistogramName,
                                  *feed->aggregate_watchtime, base::TimeDelta(),
                                  base::TimeDelta::FromHours(1), 60);
  }
}

}  // namespace media_feeds
