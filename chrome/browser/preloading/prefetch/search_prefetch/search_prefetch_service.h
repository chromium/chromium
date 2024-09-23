// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_H_

#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_request.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_url_loader.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox.mojom-shared.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "content/public/browser/preloading.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

struct AutocompleteMatch;
struct OmniboxLog;
class PrefRegistrySimple;
class Profile;
class AutocompleteResult;

namespace content {
class WebContents;
}

namespace network {
struct ResourceRequest;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Any updates to this class need to be propagated to enums.xml.
// This enum is used for UMA only. For recording a UKM PreloadingAttempt record,
// use or introduce a corresponding enum to content::PreloadingEligibility or
// ChromePreloadingEligibility.
//
// LINT.IfChange
enum class SearchPrefetchEligibilityReason {
  // The prefetch was started.
  kPrefetchStarted = 0,
  // The user has disabled prefetching and preconnecting in their client.
  kPrefetchDisabled = 1,
  // The user has disabled javascript overall or on the DSE.
  kJavascriptDisabled = 2,
  // The default search engine is not set.
  kSearchEngineNotValid = 3,
  // The entry has no search terms in the suggestion.
  kNotDefaultSearchWithTerms = 4,
  // We have seen an error in a network request recently.
  kErrorBackoff = 5,
  // This query was issued recently as a prefetch and was not served (can be
  // failed, cancelled, or complete).
  kAttemptedQueryRecently = 6,
  // Too many prefetches have been cancelled, failed, or not served recently.
  kMaxAttemptsReached = 7,
  // A URLLoaderThrottle decided this request should not be issued.
  kThrottled = 8,
  // The prefetch was suppressed because the network is too slow.
  kSlowNetwork = 9,
  kMaxValue = kSlowNetwork,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:SearchPrefetchEligibilityReason)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Any updates to this class need to be propagated to enums.xml.
//
// If you change this, please follow the process in
// go/preloading-dashboard-updates to update the mapping reflected in dashboard,
// or if you are not a Googler, please file an FYI bug on https://crbug.new with
// component Internals>Preload.
//
// LINT.IfChange
enum class SearchPrefetchServingReason {
  // The prefetch was started.
  kServed = 0,
  // The default search engine is not set.
  kSearchEngineNotValid = 1,
  // The user has disabled javascript overall or on the DSE.
  kJavascriptDisabled = 2,
  // The entry has no search terms in the suggestion.
  kNotDefaultSearchWithTerms = 3,
  // There wasn't a prefetch issued for the search terms.
  kNoPrefetch = 4,
  // The prefetch for the search terms was for a different origin than the DSE.
  kPrefetchWasForDifferentOrigin = 5,
  // kRequestWasCancelled = 6,  // No longer used.
  // The request failed due to some network/service error.
  kRequestFailed = 7,
  // The request wasn't served unexpectantly.
  kNotServedOtherReason = 8,
  // The navigation was a POST request, reload or link navigation.
  kPostReloadFormOrLink = 9,
  // kPrerendered = 10,  // No longer used.
  // The prefetch is not ready as it was in-flight.
  kRequestInFlightNotReady = 11,
  kMaxValue = kRequestInFlightNotReady,
};
// LINT.ThenChange()

class SearchPrefetchService : public KeyedService,
                              public TemplateURLServiceObserver {
 public:

  struct SearchPrefetchServingReasonRecorder;
  explicit SearchPrefetchService(Profile* profile);
  ~SearchPrefetchService() override;

  SearchPrefetchService(const SearchPrefetchService&) = delete;
  SearchPrefetchService& operator=(const SearchPrefetchService&) = delete;

  // KeyedService:
  void Shutdown() override;

  // TemplateURLServiceObserver:
  // Monitors changes to DSE. If a change occurs, clears prefetches.
  void OnTemplateURLServiceChanged() override;

  // Called when `AutocompleteController` receives updates on `result`.
  void OnResultChanged(content::WebContents* web_contents,
                       const AutocompleteResult& result);

  // Returns whether the prefetch started or not.
  bool MaybePrefetchURL(const GURL& url, content::WebContents* web_contents);

  // Clear all prefetches from the service.
  void ClearPrefetches();

  // Clear the disk cache entry for |url|.
  void ClearCacheEntry(const GURL& navigation_url);

  // Update the last serving time of |url|, so it's eviction priority is
  // lowered.
  void UpdateServeTime(const GURL& navigation_url);

  // Takes the response from this object if |url| matches a prefetched URL.
  SearchPrefetchURLLoader::RequestHandler TakePrefetchResponseFromMemoryCache(
      const network::ResourceRequest& tentative_resource_request);

  // Creates a cache loader to serve a cache only response with fallback to
  // network fetch.
  SearchPrefetchURLLoader::RequestHandler TakePrefetchResponseFromDiskCache(
      const GURL& navigation_url);

  // Called by `SearchPrerenderTask` upon prerender activation.
  void OnPrerenderedRequestUsed(const GURL& canonical_search_url,
                                const GURL& navigation_url);

  // Creates a response reader if this instance has prefetched a response for
  // the given `tentative_resource_request`, and the caller can read the
  // response with the returned value. Returns an empty callback if the response
  // is not found.
  SearchPrefetchURLLoader::RequestHandler MaybeCreateResponseReader(
      const network::ResourceRequest& tentative_resource_request);

  // Reports the status of a prefetch for a given search suggestion URL.
  std::optional<SearchPrefetchStatus> GetSearchPrefetchStatusForTesting(
      const GURL& canonical_search_url);

  // Given the canonical_search_url, returns the corresponding url that is sent
  // to the network.
  // TODO(crbug.com/345275145): Prerender should not rely on this to get the
  // real url. Refactor the test code and then remove this method.
  GURL GetRealPrefetchUrlForTesting(const GURL& canonical_search_url);

  // Calls |LoadFromPrefs()|.
  bool LoadFromPrefsForTesting();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  base::WeakPtr<SearchPrefetchService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Considers if this prefetch is worth starting, and if so, starts a prefetch
  // for |match|. |index| is the location within the omnibox drop down.
  // |web_contents| represents the active WebContents this prefetch is started
  // which can be nullptr in case no active WebContents is present.
  // |navigation_predictor| indicates the omnibox event type that
  // indicated a likely navigation. Returns whether or not a prefetch was
  // started.
  bool OnNavigationLikely(
      size_t index,
      const AutocompleteMatch& match,
      omnibox::mojom::NavigationPredictor navigation_predictor,
      content::WebContents* web_contents);

  // If the navigation URL matches with a prefetch that can be served, this
  // function marks that prefetch as clicked to prevent deletion when omnibox
  // closes.
  void OnURLOpenedFromOmnibox(OmniboxLog* log);

  // Fires all timers.
  void FireAllExpiryTimerForTesting();

  // For a given `canonical_search_url`, tells its corresponding
  // StreamingSearchPrefetchURLLoader to run the callback upon destruction.
  void SetLoaderDestructionCallbackForTesting(
      const GURL& canonical_search_url,
      base::OnceClosure streaming_url_loader_destruction_callback);

 private:
  friend class PrerenderOmniboxSearchSuggestionBrowserTest;
  friend class SearchPrefetchServiceEnabledBrowserTest;

  // Returns whether the prefetch started or not.
  bool MaybePrefetchURL(const GURL& url,
                        bool navigation_prefetch,
                        content::WebContents* web_contents,
                        content::PreloadingPredictor predictor);

  // Adds |this| as an observer of |template_url_service| if not added already.
  void ObserveTemplateURLService(TemplateURLService* template_url_service);

  // Records a cache entry for a navigation that is being served.
  void AddCacheEntry(const GURL& navigation_url, const GURL& prefetch_url);

  // Removes the prefetch and prefetch timers associated with |search_terms|.
  // Note: Always call this method to remove prefetch requests from memory
  // cache; Do not delete it from `prefetches_` directly.
  void DeletePrefetch(GURL canonical_search_url);

  // Records metrics around the error rate of prefetches. When |error| is true,
  // records the current time to prevent prefetches for a set duration.
  void ReportFetchResult(bool error);

  // These methods serialize and deserialize |prefetch_cache_| to
  // |profile_| pref service in a dictionary value.
  //
  // Returns true iff loading the prefs removed at least one entry, so the pref
  // should be saved.
  bool LoadFromPrefs();
  void SaveToPrefs() const;

  // Retrieved the started prefetches by search_terms.
  std::map<GURL, std::unique_ptr<SearchPrefetchRequest>>::iterator
  RetrieveSearchTermsInMemoryCache(
      const network::ResourceRequest& tentative_resource_request,
      SearchPrefetchServingReasonRecorder& recorder);

  // Called when this receives preloadable hints, and iff the
  // SearchPrefetchUpgradeToPrerender feature is enabled. The feature is running
  // on the assumption that Prerender is triggered after Prefetch receives
  // servable response, so some specific logic is required and implemented by
  // this method, e.g., it prefetches a prerender hint regardless of whether it
  // is a prefetch hint, since a prerenderable result should be prefetchable.
  void CoordinatePrefetchWithPrerender(const AutocompleteMatch& match,
                                       content::WebContents* web_contents,
                                       TemplateURLService* template_url_service,
                                       const GURL& canonical_search_url);

  // Preloads the compression dictionaries in the network service.
  void MaybePreloadDictionary(const AutocompleteResult& result);
  void DeletePreloadedDictionaries();

  // Prefetches that are started are stored using search terms as a key. Only
  // one prefetch should be started for a given search term until the old
  // prefetch expires.
  std::map<GURL, std::unique_ptr<SearchPrefetchRequest>> prefetches_;

  // A group of timers to expire |prefetches_| based on the same key.
  std::map<GURL, std::unique_ptr<base::OneShotTimer>> prefetch_expiry_timers_;

  // The time of the last prefetch network/server error.
  base::TimeTicks last_error_time_ticks_;

  // The current state of the DSE.
  std::optional<TemplateURLData> template_url_service_data_;

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      observer_{this};

  raw_ptr<Profile> profile_;

  // A map of previously handled URLs that allows certain navigations to be
  // served from cache. The value is the prefetch URL in cache and the latest
  // serving time of the response.
  std::map<GURL, std::pair<GURL, base::Time>> prefetch_cache_;

  mojo::PendingRemote<network::mojom::PreloadedSharedDictionaryInfoHandle>
      preloaded_shared_dictionaries_handle_;
  base::OneShotTimer preloaded_shared_dictionaries_expiry_timer_;

  base::WeakPtrFactory<SearchPrefetchService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_H_
