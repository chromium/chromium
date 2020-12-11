// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_H_
#define CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_H_

#include <map>

#include "base/callback.h"
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

class Profile;
class SearchPrefetchURLLoader;

class AutocompleteController;

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

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      observer_{this};

  Profile* profile_;
};

#endif  // CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_H_
