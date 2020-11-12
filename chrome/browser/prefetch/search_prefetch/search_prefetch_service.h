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
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

class Profile;
class GURL;
class PrefetchedResponseContainer;

class AutocompleteController;

enum class SearchPrefetchStatus {
  // The request is on the network and may move to any other state.
  kInFlight = 1,
  // The request received all the data and is ready to serve.
  kSuccessfullyCompleted = 2,
  // The request hit an error and cannot be served.
  kRequestFailed = 3,
  // The request was cancelled before completion.
  kRequestCancelled = 4,
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
  std::unique_ptr<PrefetchedResponseContainer> TakePrefetchResponse(
      const GURL& url);

  // Reports the status of a prefetch for a given search term.
  base::Optional<SearchPrefetchStatus> GetSearchPrefetchStatusForTesting(
      base::string16 search_terms);

 private:
  // Removes the prefetch and prefetch timers associated with |search_terms|.
  void DeletePrefetch(base::string16 search_terms);

  // Records the current time to prevent prefetches for a set duration.
  void ReportError();

  // Internal class to represent an ongoing or completed prefetch.
  class PrefetchRequest {
   public:
    // |service| must outlive this class and be able to manage this class's
    // lifetime.
    PrefetchRequest(const GURL& prefetch_url,
                    base::OnceClosure report_error_callback);
    ~PrefetchRequest();

    PrefetchRequest(const PrefetchRequest&) = delete;
    PrefetchRequest& operator=(const PrefetchRequest&) = delete;

    // Starts the network request to prefetch |prefetch_url_|.
    void StartPrefetchRequest(Profile* profile);

    // Cancels the on-going prefetch and marks the status appropriately.
    void CancelPrefetch();

    SearchPrefetchStatus current_status() const { return current_status_; }

    const GURL& prefetch_url() const { return prefetch_url_; }

    // Takes ownership of the prefetched data.
    std::unique_ptr<PrefetchedResponseContainer> TakePrefetchResponse();

   private:
    // Called as a callback when the prefetch request is complete. Stores the
    // response and other metadata in |prefetch_response_container_|.
    void LoadDone(std::unique_ptr<std::string> response_body);

    SearchPrefetchStatus current_status_ = SearchPrefetchStatus::kInFlight;

    // The URL to prefetch the search terms from.
    const GURL prefetch_url_;

    // The ongoing prefetch request. Null before and after the fetch.
    std::unique_ptr<network::SimpleURLLoader> simple_loader_;

    // Once a prefetch is completed successfully, the associated prefetch data
    // and metadata about the request.
    std::unique_ptr<PrefetchedResponseContainer> prefetch_response_container_;

    // Called when there is a network/server error on the prefetch request.
    base::OnceClosure report_error_callback_;
  };

  // Prefetches that are started are stored using search terms as a key. Only
  // one prefetch should be started for a given search term until the old
  // prefetch expires.
  std::map<base::string16, std::unique_ptr<PrefetchRequest>> prefetches_;

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
