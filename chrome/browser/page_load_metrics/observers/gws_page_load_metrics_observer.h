// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/google/core/common/google_util.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/navigation_handle_timing.h"

namespace internal {
// Exposed for tests.

extern const char kHistogramGWSNavigationStartToFinalRequestStart[];
extern const char kHistogramGWSNavigationStartToFinalResponseStart[];
extern const char kHistogramGWSNavigationStartToFinalLoaderCallback[];
extern const char kHistogramGWSNavigationStartToFirstRequestStart[];
extern const char kHistogramGWSNavigationStartToFirstResponseStart[];
extern const char kHistogramGWSNavigationStartToFirstLoaderCallback[];
extern const char kHistogramGWSNavigationStartToOnComplete[];

extern const char kHistogramGWSConnectTimingFirstRequestDomainLookupDelay[];
extern const char kHistogramGWSConnectTimingFirstRequestConnectDelay[];
extern const char kHistogramGWSConnectTimingFirstRequestSslDelay[];
extern const char kHistogramGWSConnectTimingFinalRequestDomainLookupDelay[];
extern const char kHistogramGWSConnectTimingFinalRequestConnectDelay[];
extern const char kHistogramGWSConnectTimingFinalRequestSslDelay[];

extern const char kHistogramGWSAFTEnd[];
extern const char kHistogramGWSAFTStart[];

extern const char kHistogramGWSFirstContentfulPaint[];
extern const char kHistogramGWSLargestContentfulPaint[];
extern const char kHistogramGWSParseStart[];
extern const char kHistogramGWSConnectStart[];
extern const char kHistogramGWSDomainLookupStart[];
extern const char kHistogramGWSDomainLookupEnd[];

}  // namespace internal

class GWSPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // The source of the navigation. Specifically keeps track of special cases
  // such as the navigtation started from NewTabPage, or if the navigation
  // started in the background.
  //
  // These values are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  //
  // LINT.IfChange(NavigationSourceType)
  enum NavigationSourceType {
    kUnknown,
    kFromNewTabPage,
    kStartedInBackground,
    kStartedInBackgroundFromNewTabPage,
    kFromGWSPage,
    kStartedInBackgroundFromGWSPage,
    kMaxValue = kStartedInBackgroundFromGWSPage,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/page/enums.xml:NavigationSourceTypeEnum)

  GWSPageLoadMetricsObserver();

  GWSPageLoadMetricsObserver(const GWSPageLoadMetricsObserver&) = delete;
  GWSPageLoadMetricsObserver& operator=(const GWSPageLoadMetricsObserver&) =
      delete;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;

  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;

  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;

  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnParseStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnConnectStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnDomainLookupStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnDomainLookupEnd(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnCustomUserTimingMarkObserved(
      const std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr>&
          timings) override;

  // The methods below are only intended for use in testing.
  void SetIsFirstNavigationForTesting(bool is_first_navigation) {
    is_first_navigation_ = is_first_navigation;
  }

  void SetNewTabPageForTesting(bool is_new_tab_page) {
    source_type_ = kFromNewTabPage;
  }

 private:
  void LogMetricsOnComplete();
  void RecordNavigationTimingHistograms();
  void RecordLatencyHitograms(base::TimeTicks response_start_time);

  // Records the histograms required before commit. This is to ensure that we
  // are getting the metrics only for GWS navigations.
  void RecordPreCommitHistograms();

  bool IsFromNewTabPage(content::NavigationHandle* navigation_handle);
  std::string AddHistogramSuffix(const std::string& histogram_name);

  content::NavigationHandleTiming navigation_handle_timing_;

  bool is_first_navigation_ = false;

  NavigationSourceType source_type_ = kUnknown;

  std::optional<base::TimeDelta> aft_start_time_;
  std::optional<base::TimeDelta> aft_end_time_;
  std::optional<base::TimeDelta> body_chunk_start_time_;
  std::optional<base::TimeDelta> header_chunk_start_time_;
  std::optional<base::TimeDelta> header_chunk_end_time_;

  int64_t navigation_id_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_PAGE_LOAD_METRICS_OBSERVER_H_
