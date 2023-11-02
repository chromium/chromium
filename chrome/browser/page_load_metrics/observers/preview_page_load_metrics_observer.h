// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PREVIEW_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PREVIEW_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

// Records Link-Preview project related metrics to evaluate the project impact.
// So, it collects data from all kind of navigations regardless of the
// Link-Preview feature existence to analyse users' activity trend.
// As a starting point, this class experimentally records page visit types and
// total foreground time duration for each visit type to analyze a.k.a unwanted
// navigations.
class PreviewPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // As we don't identify client redirect cases, kPassingVisit may be
  // overestimated a little.
  enum class PageVisitType {
    kObsoleteIndependentVisit = 0,
    kObsoleteOriginVisit = 1,
    kPassingVisit = 2,
    kTerminalVisit = 3,
    kHistoryVisit = 4,
    kIndependentLinkVisit = 5,
    kIndependentUIVisit = 6,
    kOriginLinkVisit = 7,
    kOriginUIVisit = 8,
    kMaxValue = kOriginUIVisit,
  };
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PreviewFinalStatus {
    // Preview started, but the page wasn't promoted in any way
    kPreviewed = 0,
    // Preview started, and the page was promoted into a new tab
    kPromoted = 1,
    // TODO(b:292184832): Will define another entry for the promotion to the
    // current tab
    kMaxValue = kPromoted,
  };

  PreviewPageLoadMetricsObserver() = default;
  PreviewPageLoadMetricsObserver(const PreviewPageLoadMetricsObserver&) =
      delete;
  PreviewPageLoadMetricsObserver& operator=(
      const PreviewPageLoadMetricsObserver&) = delete;
  ~PreviewPageLoadMetricsObserver() override = default;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnPreviewStart(content::NavigationHandle* navigation_handle,
                               const GURL& currently_committed_url) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnShown() override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void DidActivatePreviewedPage(base::TimeTicks activation_time) override;

 private:
  enum class Status {
    kNotPreviewed,
    kPreviewed,
    kPromoted,
  };
  PageVisitType RecordPageVisitType();
  void RecordMetrics();
  void CheckPageTransitionType(content::NavigationHandle* navigation_handle);
  PreviewFinalStatus ConvertStatusToPreviewFinalStatus(Status state);

  bool currently_in_foreground_ = false;
  bool is_history_navigation_ = false;
  bool is_first_navigation_ = false;
  base::TimeTicks last_time_shown_;
  base::TimeDelta total_foreground_duration_;
  Status status_ = Status::kNotPreviewed;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PREVIEW_PAGE_LOAD_METRICS_OBSERVER_H_
