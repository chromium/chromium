// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FROM_GWS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FROM_GWS_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "components/page_load_metrics/browser/observers/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "url/gurl.h"

namespace internal {
// Exposed for tests.
extern const char kHistogramFromGWSDomContentLoaded[];
extern const char kHistogramFromGWSLoad[];
extern const char kHistogramFromGWSFirstPaint[];
extern const char kHistogramFromGWSFirstImagePaint[];
extern const char kHistogramFromGWSFirstContentfulPaint[];
extern const char kHistogramFromGWSLargestContentfulPaint[];
extern const char kHistogramFromGWSParseStartToFirstContentfulPaint[];
extern const char kHistogramFromGWSParseDuration[];
extern const char kHistogramFromGWSParseStart[];
extern const char kHistogramFromGWSFirstInputDelay[];
extern const char kHistogramFromGWSAbortStopBeforePaint[];
extern const char kHistogramFromGWSAbortStopBeforeInteraction[];
extern const char kHistogramFromGWSAbortStopBeforeCommit[];
extern const char kHistogramFromGWSAbortCloseBeforePaint[];
extern const char kHistogramFromGWSAbortCloseBeforeInteraction[];
extern const char kHistogramFromGWSAbortCloseBeforeCommit[];
extern const char kHistogramFromGWSAbortNewNavigationBeforeCommit[];
extern const char kHistogramFromGWSAbortNewNavigationBeforePaint[];
extern const char kHistogramFromGWSAbortNewNavigationBeforeInteraction[];
extern const char kHistogramFromGWSAbortReloadBeforeInteraction[];
extern const char kHistogramFromGWSForegroundDuration[];
extern const char kHistogramFromGWSForegroundDurationAfterPaint[];
extern const char kHistogramFromGWSForegroundDurationNoCommit[];

}  // namespace internal

// FromGWSPageLoadMetricsLogger is a peer class to
// FromGWSPageLoadMetricsObserver. FromGWSPageLoadMetricsLogger is responsible
// for tracking state needed to decide if metrics should be logged, and to log
// metrics in cases where metrics should be logged. FromGWSPageLoadMetricsLogger
// exists to decouple the logging policy implementation from other Chromium
// classes such as NavigationHandle and related infrastructure, in order to make
// the code more unit testable.
class FromGWSPageLoadMetricsLogger {
 public:
  FromGWSPageLoadMetricsLogger();
  ~FromGWSPageLoadMetricsLogger();

  void SetPreviouslyCommittedUrl(const GURL& url);
  void SetProvisionalUrl(const GURL& url);

  void set_navigation_initiated_via_link(bool navigation_initiated_via_link) {
    navigation_initiated_via_link_ = navigation_initiated_via_link;
  }

  void SetNavigationStart(const base::TimeTicks navigation_start) {
    // Should be invoked at most once
    DCHECK(navigation_start_.is_null());
    navigation_start_ = navigation_start;
  }

  // Invoked when metrics for the given page are complete.
  void OnCommit(content::NavigationHandle* navigation_handle,
                ukm::SourceId source_id);
  // TODO(crbug/993377): Replace const& to PageLoadMetricsObserverDelegate with
  // a member variable.
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);
  void OnFailedProvisionalLoad(
      const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info,
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);

  void OnDomContentLoadedEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);
  void OnFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);
  void OnFirstImagePaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);
  void OnParseStart(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);
  void OnParseStop(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);
  void OnUserInput(
      const blink::WebInputEvent& event,
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);
  void OnFirstInputInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);
  void FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);

  void OnTimingUpdate(content::RenderFrameHost* subframe_rfh,
                      const page_load_metrics::mojom::PageLoadTiming& timing);

  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle,
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);

  // The methods below are public only for testing.
  bool ShouldLogFailedProvisionalLoadMetrics();
  bool ShouldLogPostCommitMetrics(const GURL& url);
  bool ShouldLogForegroundEventAfterCommit(
      const base::Optional<base::TimeDelta>& event,
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);

 private:
  void LogMetricsOnComplete(
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);

  bool previously_committed_url_is_search_results_ = false;
  bool previously_committed_url_is_search_redirector_ = false;
  bool navigation_initiated_via_link_ = false;
  bool provisional_url_has_search_hostname_ = false;

  // The state of if first paint is triggered.
  bool first_paint_triggered_ = false;

  base::TimeTicks navigation_start_;

  // The time of first user interaction after paint from navigation start.
  base::Optional<base::TimeDelta> first_user_interaction_after_paint_;

  page_load_metrics::LargestContentfulPaintHandler
      largest_contentful_paint_handler_;

  DISALLOW_COPY_AND_ASSIGN(FromGWSPageLoadMetricsLogger);
};

class FromGWSPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  FromGWSPageLoadMetricsObserver();

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                         const GURL& currently_committed_url,
                         bool started_in_foreground) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;

  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void OnDomContentLoadedEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstImagePaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstInputInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnParseStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnParseStop(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFailedProvisionalLoad(
      const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info)
      override;

  void OnUserInput(
      const blink::WebInputEvent& event,
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void OnTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  FromGWSPageLoadMetricsLogger logger_;

  DISALLOW_COPY_AND_ASSIGN(FromGWSPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FROM_GWS_PAGE_LOAD_METRICS_OBSERVER_H_
