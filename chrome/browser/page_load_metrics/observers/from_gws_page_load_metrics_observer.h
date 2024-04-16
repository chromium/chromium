// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FROM_GWS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FROM_GWS_PAGE_LOAD_METRICS_OBSERVER_H_

#include <optional>

#include "base/time/time.h"
#include "components/google/core/common/google_util.h"
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
extern const char kHistogramFromGWSCumulativeLayoutShiftMainFrame[];
extern const char kHistogramFromGWSMaxCumulativeShiftScoreSessionWindow[];

extern const char kHistogramFromGWSFromSidePanelFirstInputDelay[];
extern const char
    kHistogramFromGWSFromSidePanelMaxCumulativeShiftScoreSessionWindow[];
extern const char kHistogramFromGWSFromSidePanelFirstContentfulPaint[];
extern const char kHistogramFromGWSFromSidePanelFirstImagePaint[];
extern const char kHistogramFromGWSFromSidePanelLargestContentfulPaint[];

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

  FromGWSPageLoadMetricsLogger(const FromGWSPageLoadMetricsLogger&) = delete;
  FromGWSPageLoadMetricsLogger& operator=(const FromGWSPageLoadMetricsLogger&) =
      delete;

  ~FromGWSPageLoadMetricsLogger();

  void SetPreviouslyCommittedUrl(const GURL& url);
  void SetProvisionalUrl(const GURL& url);

  // Configures the logger with relevant side panel state so that logs are
  // emitted correctly.
  void SetNavigationStateForSidePanel(const GURL& initiating_side_panel_url,
                                      bool navigation_initiated_via_link);

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
  // TODO(crbug.com/40640180): Replace const& to PageLoadMetricsObserverDelegate
  // with a member variable.
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

  bool IsSidePanelInitiatedNavigation() const;
  bool ShouldLogSidePanelMetrics() const;

  // The methods below are public only for testing.
  bool ShouldLogFailedProvisionalLoadMetrics();
  bool ShouldLogPostCommitMetrics(const GURL& url);
  bool ShouldLogForegroundEventAfterCommit(
      const std::optional<base::TimeDelta>& event,
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);

 private:
  void LogMetricsOnComplete(
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);

  bool previously_committed_url_is_search_results_ = false;
  google_util::GoogleSearchMode navigation_initiated_search_mode_ =
      google_util::GoogleSearchMode::kUnspecified;
  bool previously_committed_url_is_search_redirector_ = false;
  bool navigation_initiated_via_link_ = false;
  bool provisional_url_has_search_hostname_ = false;

  // The state of if first paint is triggered.
  bool first_paint_triggered_ = false;

  // The committed URL in the side panel that initiated this navigation. (i.e.
  // first entry in the current redirection chain). This is only set if this
  // navigation was initiated from the side panel
  std::optional<GURL> initiating_side_panel_url_;

  base::TimeTicks navigation_start_;

  // The time of first user interaction after paint from navigation start.
  std::optional<base::TimeDelta> first_user_interaction_after_paint_;
};

class FromGWSPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  FromGWSPageLoadMetricsObserver();

  FromGWSPageLoadMetricsObserver(const FromGWSPageLoadMetricsObserver&) =
      delete;
  FromGWSPageLoadMetricsObserver& operator=(
      const FromGWSPageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                         const GURL& currently_committed_url,
                         bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;

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

  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFailedProvisionalLoad(
      const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info)
      override;

  void OnUserInput(
      const blink::WebInputEvent& event,
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void SetNavigationStateForSidePanelForTesting(
      const GURL& initiating_side_panel_url,
      bool navigation_initiated_via_link);

 private:
  FromGWSPageLoadMetricsLogger logger_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FROM_GWS_PAGE_LOAD_METRICS_OBSERVER_H_
