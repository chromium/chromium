// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_H_
#define CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/optional.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/prefetch/search_prefetch/base_search_prefetch_request.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "url/gurl.h"

class AutocompleteController;
struct OmniboxLog;
class PrefRegistrySimple;
class Profile;
class SearchPrefetchURLLoader;

namespace network {
struct ResourceRequest;
}

// Any updates to this class need to be propagated to enums.xml.
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
  kMaxValue = kThrottled,
};

// Any updates to this class need to be propagated to enums.xml.
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
  // The request was canceled before completion.
  kRequestWasCancelled = 6,
  // The request failed due to some network/service error.
  kRequestFailed = 7,
  // The request wasn't served unexpectantly.
  kNotServedOtherReason = 8,
  // The navigation was a POST request, reload or link navigation.
  kPostReloadOrLink = 9,
  kMaxValue = kPostReloadOrLink,
};

class SearchPrefetchService : public KeyedService,
                              public TemplateURLServiceObserver {
 public:
  explicit SearchPrefetchService(Profile* profile);
  ~SearchPrefetchService() override;

  SearchPrefetchService(const SearchPrefetchService&) = delete;
  SearchPrefetchService& operator=(const SearchPrefetchService&) = delete;

  // KeyedService:
  void Shutdown() override;

  // TemplateURLServiceObserver:
  // Monitors changes to DSE. If a change occurs, clears prefetches.
  void OnTemplateURLServiceChanged() override;

  // Called when |controller| has updated information.
  void OnResultChanged(AutocompleteController* controller);

  // Returns whether the prefetch started or not.
  bool MaybePrefetchURL(const GURL& url);

  // Clear all prefetches from the service.
  void ClearPrefetches();

  // Clear the disk cache entry for |url|.
  void ClearCacheEntry(const GURL& navigation_url);

  // Update the last serving time of |url|, so it's eviction priority is
  // lowered.
  void UpdateServeTime(const GURL& navigation_url);

  // Takes the response from this object if |url| matches a prefetched URL.
  std::unique_ptr<SearchPrefetchURLLoader> TakePrefetchResponseFromMemoryCache(
      const network::ResourceRequest& tentative_resource_request);

  // Creates a cache loader to serve a cache only response with fallback to
  // network fetch.
  std::unique_ptr<SearchPrefetchURLLoader> TakePrefetchResponseFromDiskCache(
      const GURL& navigation_url);

  // Reports the status of a prefetch for a given search term.
  base::Optional<SearchPrefetchStatus> GetSearchPrefetchStatusForTesting(
      std::u16string search_terms);

  // Calls |LoadFromPrefs()|.
  bool LoadFromPrefsForTesting();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  // Records a cache entry for a navigation that is being served.
  void AddCacheEntry(const GURL& navigation_url, const GURL& prefetch_url);

  // Removes the prefetch and prefetch timers associated with |search_terms|.
  void DeletePrefetch(std::u16string search_terms);

  // Records the current time to prevent prefetches for a set duration.
  void ReportError();

  // If the navigation URL matches with a prefetch that can be served, this
  // function marks that prefetch as clicked to prevent deletion when omnibox
  // closes.
  void OnURLOpenedFromOmnibox(OmniboxLog* log);

  // These methods serialize and deserialize |prefetch_cache_| to
  // |profile_| pref service in a dictionary value.
  //
  // Returns true iff loading the prefs removed at least one entry, so the pref
  // should be saved.
  bool LoadFromPrefs();
  void SaveToPrefs() const;

  // Prefetches that are started are stored using search terms as a key. Only
  // one prefetch should be started for a given search term until the old
  // prefetch expires.
  std::map<std::u16string, std::unique_ptr<BaseSearchPrefetchRequest>>
      prefetches_;

  // A group of timers to expire |prefetches_| based on the same key.
  std::map<std::u16string, std::unique_ptr<base::OneShotTimer>>
      prefetch_expiry_timers_;

  // The time of the last prefetch network/server error.
  base::TimeTicks last_error_time_ticks_;

  // The current state of the DSE.
  base::Optional<TemplateURLData> template_url_service_data_;

  // A subscription to the omnibox log service to track when a navigation is
  // about to happen.
  base::CallbackListSubscription omnibox_subscription_;

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      observer_{this};

  Profile* profile_;

  // A map of previously handled URLs that allows certain navigations to be
  // served from cache. The value is the prefetch URL in cache and the latest
  // serving time of the response.
  std::map<GURL, std::pair<GURL, base::Time>> prefetch_cache_;
};

#endif  // CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_H_
