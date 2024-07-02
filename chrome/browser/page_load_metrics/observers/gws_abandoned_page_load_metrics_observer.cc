// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/gws_abandoned_page_load_metrics_observer.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"

namespace {
std::string DiscardReasonToString(
    content::NavigationDiscardReason discard_reason) {
  switch (discard_reason) {
    case content::NavigationDiscardReason::kNewNavigation:
      // TODO(https://crbug.com/347706997): Get the type of the new navigation
      // and expand the discard reason.
      return internal::kAbandonReasonNewNavigation;
    case content::NavigationDiscardReason::kWillRemoveFrame:
      return internal::kAbandonReasonFrameRemoved;
    case content::NavigationDiscardReason::kExplicitCancellation:
      return internal::kAbandonReasonExplicitCancellation;
    case content::NavigationDiscardReason::kInternalCancellation:
      return internal::kAbandonReasonInternalCancellation;
    case content::NavigationDiscardReason::kRenderProcessGone:
      return internal::kAbandonReasonRenderProcessGone;
    case content::NavigationDiscardReason::kNeverStarted:
      return internal::kAbandonReasonNeverStarted;
    case content::NavigationDiscardReason::kFailedSecurityCheck:
      return internal::kAbandonReasonFailedSecurityCheck;
      // Other cases like kCommittedNavigation and kRenderFrameHostDestruction
      // should be obsolete, so just use "other" as the reason.
    case content::NavigationDiscardReason::kCommittedNavigation:
      return internal::kAbandonReasonOther;
    case content::NavigationDiscardReason::kRenderFrameHostDestruction:
      return internal::kAbandonReasonOther;
  }
}
}  // namespace

namespace internal {

#define HISTOGRAM_PREFIX "PageLoad.Clients.GoogleSearch.Leakage."

const char kAbandonReasonNewNavigation[] = "NewNavigation";
const char kAbandonReasonFrameRemoved[] = "FrameRemoved";
const char kAbandonReasonExplicitCancellation[] = "ExplicitCancellation";
const char kAbandonReasonInternalCancellation[] = "InternalCancellation";
const char kAbandonReasonRenderProcessGone[] = "RenderProcessGone";
const char kAbandonReasonNeverStarted[] = "NeverStarted";
const char kAbandonReasonFailedSecurityCheck[] = "FailedSecurityCheck";
const char kAbandonReasonOther[] = "Other";
const char kAbandonReasonHidden[] = "Hidden";
const char kAbandonReasonErrorPage[] = "ErrorPage";
const char kAbandonReasonAppBackgrounded[] = "AppBackgrounded";

const char kHistogramGWSLeakageNavigationStart[] =
    HISTOGRAM_PREFIX "NavigationStart";
const char kHistogramGWSLeakageNavigationStartToLoaderStart[] =
    HISTOGRAM_PREFIX "NavigationStartToLoaderStart";
const char kHistogramGWSLeakageNavigationStartToFirstRedirectedRequestStart[] =
    HISTOGRAM_PREFIX "NavigationStartToFirstRedirectedRequestStart";
const char kHistogramGWSLeakageNavigationStartToFirstRedirectResponseStart[] =
    HISTOGRAM_PREFIX "NavigationStartToFirstRedirectResponseStart";
const char kHistogramGWSLeakageNavigationStartToNonRedirectedRequestStart[] =
    HISTOGRAM_PREFIX "NavigationStartToNonRedirectedRequestStart";
const char kHistogramGWSLeakageNavigationStartToNonRedirectResponseStart[] =
    HISTOGRAM_PREFIX "NavigationStartToNonRedirectResponseStart";
const char kHistogramGWSLeakageNavigationStartToCommitSent[] =
    HISTOGRAM_PREFIX "NavigationStartToCommitSent";
const char kHistogramGWSLeakageNavigationStartToDidCommit[] =
    HISTOGRAM_PREFIX "NavigationStartToDidCommit";
// TODO(https://crbug.com/347706997): Record more milestones related to loading,
// loader callbacks, and process creation timing.

const char kHistogramGWSLeakageNavigationStartToAbandon[] =
    HISTOGRAM_PREFIX "NavigationStartToAbandon.";
const char kHistogramGWSLeakageLoaderStartToAbandon[] =
    HISTOGRAM_PREFIX "LoaderStartToAbandon.";
const char kHistogramGWSLeakageFirstRedirectResponseStartToAbandon[] =
    HISTOGRAM_PREFIX "FirstRedirectResponseStartToAbandon.";
const char kHistogramGWSLeakageNonRedirectResponseStartToAbandon[] =
    HISTOGRAM_PREFIX "NonRedirectResponseStartToAbandon.";
const char kHistogramGWSLeakageCommitSentToAbandon[] =
    HISTOGRAM_PREFIX "CommitSentToAbandon.";

}  // namespace internal

GWSAbandonedPageLoadMetricsObserver::GWSAbandonedPageLoadMetricsObserver() =
    default;

GWSAbandonedPageLoadMetricsObserver::~GWSAbandonedPageLoadMetricsObserver() =
    default;

const char* GWSAbandonedPageLoadMetricsObserver::GetObserverName() const {
  static const char kName[] = "GWSAbandonedPageLoadMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSAbandonedPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  if (!page_load_metrics::IsGoogleSearchResultUrl(
          navigation_handle->GetURL())) {
    // TODO(https://crbug.com/347706997): Keep observing for navigations that
    // didn't start out as going to SRP but later redirects to SRP, but mark
    // the logged milestones specifically as redirected from non-SRP.
    return STOP_OBSERVING;
  }

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSAbandonedPageLoadMetricsObserver::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  // TODO(https://crbug.com/347706997): Keep observing for navigations that
  // didn't start out as going to SRP but later redirects to SRP, but mark
  // the logged milestones specifically as redirected from non-SRP.
  if (!page_load_metrics::IsGoogleSearchResultUrl(
          navigation_handle->GetURL())) {
    return STOP_OBSERVING;
  }

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSAbandonedPageLoadMetricsObserver::OnNavigationHandleTimingUpdated(
    content::NavigationHandle* navigation_handle) {
  CHECK(
      page_load_metrics::IsGoogleSearchResultUrl(navigation_handle->GetURL()));

  latest_navigation_handle_timing_ =
      navigation_handle->GetNavigationHandleTiming();

  if (navigation_handle->GetNetErrorCode() != net::OK) {
    // The navigation will commit an error page instead of SRP. Record this as
    // an abandonment as soon as we notice.
    LogMetricsOnAbandon(internal::kAbandonReasonErrorPage,
                        latest_navigation_handle_timing_.request_failed_time);
    return STOP_OBSERVING;
  }

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSAbandonedPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  CHECK(
      page_load_metrics::IsGoogleSearchResultUrl(navigation_handle->GetURL()));
  CHECK(!did_abandon_navigation_);

  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramGWSLeakageNavigationStartToDidCommit,
      base::TimeTicks::Now() - GetDelegate().GetNavigationStart());
  LogNavigationMilestoneMetrics();

  // This function is called from the NavigationHandle destructor navigation
  // finished. This means that the navigation can't be abandoned anymore, so we
  // can stop observing.
  // TODO(https://crbug.com/347706997): Once we add the loading milestones we
  // need to continue observing, since the loading updates can arrive after
  // this.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSAbandonedPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Prerender navigations won't be tracked.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSAbandonedPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class is interested only in events that are preprocessed and
  // dispatched also to the outermost page at PageLoadTracker. So, this class
  // doesn't need to forward events for FencedFrames.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSAbandonedPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // TODO(https://crbug.com/347706997): Continue observing and logging
  // milestones after hiding, but mark those milestones specifically as
  // having been backgrounded.
  LogMetricsOnAbandon(internal::kAbandonReasonAppBackgrounded,
                      base::TimeTicks::Now());
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSAbandonedPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // TODO(https://crbug.com/347706997): Continue observing and logging
  // milestones after hiding, but mark those milestones specifically as
  // having been hidden.
  LogMetricsOnAbandon(internal::kAbandonReasonHidden, base::TimeTicks::Now());
  return STOP_OBSERVING;
}

void GWSAbandonedPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo&
        failed_provisional_load_info) {
  LogMetricsOnAbandon(
      DiscardReasonToString(failed_provisional_load_info.discard_reason),
      base::TimeTicks::Now());
}

void GWSAbandonedPageLoadMetricsObserver::OnDidInternalNavigationAbort(
    content::NavigationHandle* navigation_handle) {
  CHECK(navigation_handle->GetNavigationDiscardReason().has_value());
  LogMetricsOnAbandon(
      DiscardReasonToString(
          navigation_handle->GetNavigationDiscardReason().value()),
      base::TimeTicks::Now());
}

void GWSAbandonedPageLoadMetricsObserver::LogMetricsOnAbandon(
    std::string abandon_reason,
    base::TimeTicks navigation_abandon_time) {
  // The navigation got abandoned before it finished committing. Log the
  // milestones now.
  LogNavigationMilestoneMetrics();

  CHECK(!did_abandon_navigation_);
  did_abandon_navigation_ = true;

  // Log the time from the latest navigation milestone received. This helps us
  // know at what point of the navigation the abandonment happened. Note that
  // for redirects and non-redirects we only check "response" milestones and not
  // the "request" counterparts, since we're only notified of
  // NavigationHandleTiming update when we get the response. Thus, the response
  // timing must be more recent than the request counterpart.
  if (!latest_navigation_handle_timing_.navigation_commit_sent_time.is_null()) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramGWSLeakageCommitSentToAbandon + abandon_reason,
        navigation_abandon_time -
            latest_navigation_handle_timing_.navigation_commit_sent_time);
  } else if (!latest_navigation_handle_timing_.non_redirect_response_start_time
                  .is_null()) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramGWSLeakageNonRedirectResponseStartToAbandon +
            abandon_reason,
        navigation_abandon_time -
            latest_navigation_handle_timing_.final_response_start_time);
  } else if (!latest_navigation_handle_timing_.first_response_start_time
                  .is_null()) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramGWSLeakageFirstRedirectResponseStartToAbandon +
            abandon_reason,
        navigation_abandon_time -
            latest_navigation_handle_timing_.first_response_start_time);
  } else if (!latest_navigation_handle_timing_.loader_start_time.is_null()) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramGWSLeakageLoaderStartToAbandon + abandon_reason,
        navigation_abandon_time -
            latest_navigation_handle_timing_.loader_start_time);
  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramGWSLeakageNavigationStartToAbandon + abandon_reason,
        navigation_abandon_time - GetDelegate().GetNavigationStart());
  }
}

void GWSAbandonedPageLoadMetricsObserver::LogNavigationMilestoneMetrics() {
  CHECK(!did_abandon_navigation_);

  const base::TimeTicks navigation_start_time =
      GetDelegate().GetNavigationStart();

  if (!latest_navigation_handle_timing_.navigation_commit_sent_time.is_null()) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramGWSLeakageNavigationStartToCommitSent,
        latest_navigation_handle_timing_.navigation_commit_sent_time -
            navigation_start_time);
  }

  if (!latest_navigation_handle_timing_.non_redirect_response_start_time
           .is_null()) {
    // The navigation had received its final non-redirect response.
    CHECK(!latest_navigation_handle_timing_.non_redirected_request_start_time
               .is_null());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramGWSLeakageNavigationStartToNonRedirectResponseStart,
        latest_navigation_handle_timing_.non_redirect_response_start_time -
            navigation_start_time);
    PAGE_LOAD_HISTOGRAM(
        internal::
            kHistogramGWSLeakageNavigationStartToNonRedirectedRequestStart,
        latest_navigation_handle_timing_.non_redirected_request_start_time -
            navigation_start_time);
  }

  if (!latest_navigation_handle_timing_.first_response_start_time.is_null() &&
      latest_navigation_handle_timing_.first_response_start_time !=
          latest_navigation_handle_timing_.non_redirect_response_start_time) {
    // If we got a response that is not the final response, it must be a
    // redirect response.
    if (!latest_navigation_handle_timing_.first_response_start_time.is_null()) {
      PAGE_LOAD_HISTOGRAM(
          internal::
              kHistogramGWSLeakageNavigationStartToFirstRedirectResponseStart,
          latest_navigation_handle_timing_.first_response_start_time -
              navigation_start_time);
    }

    if (!latest_navigation_handle_timing_.first_request_start_time.is_null()) {
      PAGE_LOAD_HISTOGRAM(
          internal::
              kHistogramGWSLeakageNavigationStartToFirstRedirectedRequestStart,
          latest_navigation_handle_timing_.first_request_start_time -
              navigation_start_time);
    }
  }

  if (!latest_navigation_handle_timing_.loader_start_time.is_null()) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramGWSLeakageNavigationStartToLoaderStart,
        latest_navigation_handle_timing_.loader_start_time -
            navigation_start_time);
  }

  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSLeakageNavigationStart,
                      base::TimeTicks::Now() - navigation_start_time);
}
