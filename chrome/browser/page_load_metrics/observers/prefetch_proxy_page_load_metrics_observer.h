// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PREFETCH_PROXY_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PREFETCH_PROXY_PAGE_LOAD_METRICS_OBSERVER_H_

#include <stdint.h>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_tab_helper.h"
#include "components/history/core/browser/history_types.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/prefetch_metrics.h"
#include "net/cookies/canonical_cookie.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}  // namespace content

// Records metrics related to loading of Prefetch Proxy. See
// //chrome/browser/prefetch/prefetch_proxy/.
class PrefetchProxyPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  PrefetchProxyPageLoadMetricsObserver();

  PrefetchProxyPageLoadMetricsObserver(
      const PrefetchProxyPageLoadMetricsObserver&) = delete;
  PrefetchProxyPageLoadMetricsObserver& operator=(
      const PrefetchProxyPageLoadMetricsObserver&) = delete;

  ~PrefetchProxyPageLoadMetricsObserver() override;

 protected:
  // Used as a callback for history service query results. Protected for
  // testing.
  void OnOriginLastVisitResult(base::Time query_start_time,
                               history::HistoryLastVisitResult result);

 private:
  void RecordMetrics();

  // Sets |prefetch_metrics_| for this page load. Done in a separate method so
  // that this can be done in an event notification.
  void GetPrefetchMetrics();

  // Records the corresponding UKM events.
  void RecordPrefetchProxyEvent();
  void RecordAfterSRPEvent();

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnDidInternalNavigationAbort(
      content::NavigationHandle* navigation_handle) override;
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
  void OnPrefetchLikely() override;

  // The time that the navigation started. Used to timebox the history service
  // query on commit.
  base::Time navigation_start_;

  // The minimum number of days since the last visit, as reported by
  // HistoryService, to any origin in the redirect chain. Set to -1 if there is
  // a response from the history service but was no previous visit.
  absl::optional<int> min_days_since_last_visit_to_origin_;

  // Metrics related to Prefetch Proxy prefetching on a SRP, for plumbing
  // into UKM.
  scoped_refptr<PrefetchProxyTabHelper::PrefetchMetrics> srp_metrics_;

  // Metrics related to prefetches requested by a page via the Speculation Rules
  // API.
  absl::optional<content::PrefetchReferringPageMetrics> referring_page_metrics_;

  // Metrics for the page load after a Google SRP where NavigationPredictor
  // passed parsed SRP links to the TabHelper. Not set if that isn't true.
  absl::optional<PrefetchProxyTabHelper::AfterSRPMetrics> after_srp_metrics_;

  // Metrics for page loads where prefetches were requested via the Speculation
  // Rules API by the previous page load.
  absl::optional<content::PrefetchServingPageMetrics> serving_page_metrics_;

  // Task tracker for calls for the history service.
  base::CancelableTaskTracker task_tracker_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PrefetchProxyPageLoadMetricsObserver> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PREFETCH_PROXY_PAGE_LOAD_METRICS_OBSERVER_H_
