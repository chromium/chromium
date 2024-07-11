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

extern const char kSuffixWasBackgrounded[];
extern const char kSuffixWasHidden[];
extern const char kSuffixWasNonSRP[];

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
  void LogMetricsOnAbandon(const std::string& abandon_reason,
                           base::TimeTicks navigation_abandon_time);
  void LogPageLoadHistogram(const std::string& name,
                            base::TimeTicks event_time,
                            base::TimeTicks relative_start_time);
  bool WasBackgrounded() const {
    return !first_backgrounded_timestamp_.is_null();
  }
  bool WasHidden() const { return !first_hidden_timestamp_.is_null(); }

  void LogPreviousBackgroundingIfNeeded();
  void LogPreviousHidingIfNeeded();

  // Set to true if we see the navigation involves non-SRP URL, which will be
  // specially marked in the logged metrics.
  bool did_request_non_srp_ = false;
  // Set to true if we see the navigation involves SRP URL, which means we need
  // to log metrics for this navigation.
  bool involved_srp_url_ = false;

  // Timestamp of the first time `FlushMetricsOnAppEnterBackground()` or
  // `OnHidden()` are called, respectively. This is tracked in case the
  // abandonments are not logged immediately, e.g. when we're not sure if the
  // navigation we're tracking will involve SRP (i.e. `involved_srp_url` is
  // false).
  base::TimeTicks first_backgrounded_timestamp_;
  base::TimeTicks first_hidden_timestamp_;
  // Whether we've previously logged backgrounding/hiding time. This is useful
  // because we will keep observing when backgrounding/hiding happens, unlike
  // other abandonment triggers. This ensures we will only log those events
  // once.
  bool did_log_backgrounding_ = false;
  bool did_log_hiding_ = false;
  // Whether the navigation has been abandoned before.
  bool did_abandon_navigation_ = false;

  // Whether the NavigationStart histogram, which should only be logged once per
  // navigation, has been logged before.
  bool did_log_navigation_start_ = false;

  // The most up-to-date NavigationHandleTiming for the navigation we're
  // tracking, updated from `OnNavigationHandleTimingUpdated()`.
  content::NavigationHandleTiming latest_navigation_handle_timing_;
  // The `latest_navigation_handle_timing_` value of the last time we called
  // `LogNavigationMilestoneMetrics()`. This is needed because that function can
  // be called multiple times, but we only want to log the milestones that we
  // haven't logged on a previous call before.
  content::NavigationHandleTiming last_logged_navigation_handle_timing_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_H_
