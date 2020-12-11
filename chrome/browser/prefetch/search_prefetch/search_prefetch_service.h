// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_H_
#define CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_H_

#include <map>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/optional.h"
#include "base/scoped_observation.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "chrome/browser/prefetch/search_prefetch/base_search_prefetch_request.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "url/gurl.h"

class AutocompleteController;
struct OmniboxLog;
class Profile;
class SearchPrefetchURLLoader;

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
  kMaxValue = kNotServedOtherReason,
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

  // Takes the response from this object if |url| matches a prefetched URL.
  std::unique_ptr<SearchPrefetchURLLoader> TakePrefetchResponse(
      const GURL& url);

  // Reports the status of a prefetch for a given search term.
  base::Optional<SearchPrefetchStatus> GetSearchPrefetchStatusForTesting(
      base::string16 search_terms);

 private:
  // Removes the prefetch and prefetch timers associated with |search_terms|.
  void DeletePrefetch(base::string16 search_terms);

  // Records the current time to prevent prefetches for a set duration.
  void ReportError();

  // If the navigation URL matches with a prefetch that can be served, this
  // function marks that prefetch as clicked to prevent deletion when omnibox
  // closes.
  void OnURLOpenedFromOmnibox(OmniboxLog* log);

  // Prefetches that are started are stored using search terms as a key. Only
  // one prefetch should be started for a given search term until the old
  // prefetch expires.
  std::map<base::string16, std::unique_ptr<BaseSearchPrefetchRequest>>
      prefetches_;

  // A group of timers to expire |prefetches_| based on the same key.
  std::map<base::string16, std::unique_ptr<base::OneShotTimer>>
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
};

#endif  // CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_H_
