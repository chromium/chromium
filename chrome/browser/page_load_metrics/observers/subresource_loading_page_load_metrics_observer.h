// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SUBRESOURCE_LOADING_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SUBRESOURCE_LOADING_PAGE_LOAD_METRICS_OBSERVER_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "net/cookies/canonical_cookie.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class NavigationHandle;
}  // namespace content

// Records metrics related to loading of subresources on a page.
class SubresourceLoadingPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  SubresourceLoadingPageLoadMetricsObserver();
  ~SubresourceLoadingPageLoadMetricsObserver() override;

 protected:
  // Used as a callback for history service query results. Protected for
  // testing.
  void OnOriginLastVisitResult(base::Time query_start_time,
                               history::HistoryLastVisitToHostResult result);

 private:
  void RecordMetrics();

  // Starts an async call to the cookie manager to determine if there are likely
  // to be cookies set on a mainframe request. This is called on navigation
  // start and redirects but should not be called on commit because it'll get
  // cookies from the mainframe response, if any.
  void CheckForCookiesOnURL(content::BrowserContext* browser_context,
                            const GURL& url);

  // Used as a callback for the cookie manager query.
  void OnCookieResult(base::Time query_start_time,
                      const net::CookieStatusList& cookies,
                      const net::CookieStatusList& excluded_cookies);

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources) override;

  // Whether data saver was enabled for this page load when it committed.
  bool data_saver_enabled_at_commit_ = false;

  // The time that the navigation started. Used to timebox the history service
  // query on commit.
  base::Time navigation_start_;

  size_t loaded_css_js_from_cache_before_fcp_ = 0;
  size_t loaded_css_js_from_network_before_fcp_ = 0;

  // These vectors hold the durations that queries to the cookie manager and
  // history service took, respectively. Since we only want to record these when
  // we also record the query results, the query times are stashed here until
  // |RecordMetrics()| is called.
  std::vector<base::TimeDelta> cookie_query_times_;
  std::vector<base::TimeDelta> history_query_times_;

  // The minimum number of days since the last visit, as reported by
  // HistoryService, to any origin in the redirect chain. Set to -1 if there is
  // a response from the history service but was no previous visit.
  base::Optional<int> min_days_since_last_visit_to_origin_;

  // Set to true if any main frame request in the redirect chain had cookies set
  // on the request. Set to false if there were no cookies set. Not set if we
  // didn't get a response from the CookieManager before recording metrics.
  base::Optional<bool> mainframe_had_cookies_;

  // Task tracker for calls for the history service.
  base::CancelableTaskTracker task_tracker_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SubresourceLoadingPageLoadMetricsObserver> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(SubresourceLoadingPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SUBRESOURCE_LOADING_PAGE_LOAD_METRICS_OBSERVER_H_
