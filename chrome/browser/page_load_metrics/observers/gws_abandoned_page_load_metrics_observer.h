// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/google/core/common/google_util.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/navigation_handle_timing.h"

namespace internal {
// Exposed for tests.

extern const char kAbandonReasonNewNavigation[];
extern const char kAbandonReasonFrameRemoved[];
extern const char kAbandonReasonExplicitCancellation[];
extern const char kAbandonReasonInternalCancellation[];
extern const char kAbandonReasonRenderProcessGone[];
extern const char kAbandonReasonNeverStarted[];
extern const char kAbandonReasonFailedSecurityCheck[];
extern const char kAbandonReasonOther[];
extern const char kAbandonReasonHidden[];
extern const char kAbandonReasonErrorPage[];
extern const char kAbandonReasonAppBackgrounded[];

extern const char kHistogramGWSLeakageNavigationStart[];
extern const char kHistogramGWSLeakageNavigationStartToLoaderStart[];
extern const char
    kHistogramGWSLeakageNavigationStartToFirstRedirectedRequestStart[];
extern const char
    kHistogramGWSLeakageNavigationStartToFirstRedirectResponseStart[];
extern const char
    kHistogramGWSLeakageNavigationStartToNonRedirectedRequestStart[];
extern const char
    kHistogramGWSLeakageNavigationStartToNonRedirectResponseStart[];
extern const char kHistogramGWSLeakageNavigationStartToCommitSent[];
extern const char kHistogramGWSLeakageNavigationStartToDidCommit[];

extern const char kHistogramGWSLeakageNavigationStartToAbandon[];
extern const char kHistogramGWSLeakageLoaderStartToAbandon[];
extern const char kHistogramGWSLeakageFirstRedirectResponseStartToAbandon[];
extern const char kHistogramGWSLeakageNonRedirectResponseStartToAbandon[];
extern const char kHistogramGWSLeakageCommitSentToAbandon[];

}  // namespace internal

// Observes and records UMA for navigations to GWS which might or might get
// "abandoned" at some point during the navigation / loading. Different from
// GWSPageLoadMetricsObserver, this observer will log the navigation milestones
// even if the navigation didn't end up reaching all the milestones. This allows
// us to look at the amount of navigations that reached each milestone, to see
// where the navigation gets abandoned. In addition to that, this observer will
// also log the abandonment reason and the last navigation milestone the
// navigation reached before getting abandoned.
class GWSAbandonedPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  GWSAbandonedPageLoadMetricsObserver();
  ~GWSAbandonedPageLoadMetricsObserver() override;

  GWSAbandonedPageLoadMetricsObserver(
      const GWSAbandonedPageLoadMetricsObserver&) = delete;
  GWSAbandonedPageLoadMetricsObserver& operator=(
      const GWSAbandonedPageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  const char* GetObserverName() const override;
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnNavigationHandleTimingUpdated(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;

  // Signals that the navigation is abandoned: backgrounded, hidden, or failed.
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFailedProvisionalLoad(
      const page_load_metrics::FailedProvisionalLoadInfo&
          failed_provisional_load_info) override;
  void OnDidInternalNavigationAbort(
      content::NavigationHandle* navigation_handle) override;

  // Prerender, fenced-frame, bfcache cases are excluded.
  // TODO(https://crbug.com/347706997): Consider logging for these cases, but
  // mark them specifically to avoid skewing the timings.
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;

 private:
  void LogNavigationMilestoneMetrics();
  void LogMetricsOnAbandon(std::string abandon_reason,
                           base::TimeTicks navigation_abandon_time);

  bool did_abandon_navigation_ = false;

  content::NavigationHandleTiming latest_navigation_handle_timing_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_H_
