// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/from_gws_page_load_metrics_observer.h"
#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

using page_load_metrics::PageAbortReason;

namespace internal {

const char kHistogramFromGWSDomContentLoaded[] =
    "PageLoad.Clients.FromGoogleSearch.DocumentTiming."
    "NavigationToDOMContentLoadedEventFired";
const char kHistogramFromGWSLoad[] =
    "PageLoad.Clients.FromGoogleSearch.DocumentTiming."
    "NavigationToLoadEventFired";
const char kHistogramFromGWSFirstPaint[] =
    "PageLoad.Clients.FromGoogleSearch.PaintTiming.NavigationToFirstPaint";
const char kHistogramFromGWSFirstImagePaint[] =
    "PageLoad.Clients.FromGoogleSearch.PaintTiming.NavigationToFirstImagePaint";
const char kHistogramFromGWSFirstContentfulPaint[] =
    "PageLoad.Clients.FromGoogleSearch.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramFromGWSLargestContentfulPaint[] =
    "PageLoad.Clients.FromGoogleSearch.PaintTiming."
    "NavigationToLargestContentfulPaint";
const char kHistogramFromGWSParseStartToFirstContentfulPaint[] =
    "PageLoad.Clients.FromGoogleSearch.PaintTiming."
    "ParseStartToFirstContentfulPaint";
const char kHistogramFromGWSParseDuration[] =
    "PageLoad.Clients.FromGoogleSearch.ParseTiming.ParseDuration";
const char kHistogramFromGWSParseStart[] =
    "PageLoad.Clients.FromGoogleSearch.ParseTiming.NavigationToParseStart";
const char kHistogramFromGWSFirstInputDelay[] =
    "PageLoad.Clients.FromGoogleSearch.InteractiveTiming.FirstInputDelay3";

const char kHistogramFromGWSAbortNewNavigationBeforeCommit[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.NewNavigation."
    "BeforeCommit";
const char kHistogramFromGWSAbortNewNavigationBeforePaint[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.NewNavigation."
    "AfterCommit.BeforePaint";
const char kHistogramFromGWSAbortNewNavigationBeforeInteraction[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.NewNavigation."
    "AfterPaint.BeforeInteraction";
const char kHistogramFromGWSAbortStopBeforeCommit[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Stop."
    "BeforeCommit";
const char kHistogramFromGWSAbortStopBeforePaint[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Stop."
    "AfterCommit.BeforePaint";
const char kHistogramFromGWSAbortStopBeforeInteraction[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Stop."
    "AfterPaint.BeforeInteraction";
const char kHistogramFromGWSAbortCloseBeforeCommit[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Close."
    "BeforeCommit";
const char kHistogramFromGWSAbortCloseBeforePaint[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Close."
    "AfterCommit.BeforePaint";
const char kHistogramFromGWSAbortCloseBeforeInteraction[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Close."
    "AfterPaint.BeforeInteraction";
const char kHistogramFromGWSAbortOtherBeforeCommit[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Other."
    "BeforeCommit";
const char kHistogramFromGWSAbortReloadBeforeCommit[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Reload."
    "BeforeCommit";
const char kHistogramFromGWSAbortReloadBeforePaint[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Reload."
    "AfterCommit.BeforePaint";
const char kHistogramFromGWSAbortReloadBeforeInteraction[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Reload."
    "AfterPaint.Before1sDelayedInteraction";
const char kHistogramFromGWSAbortForwardBackBeforeCommit[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming."
    "ForwardBackNavigation.BeforeCommit";
const char kHistogramFromGWSAbortForwardBackBeforePaint[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming."
    "ForwardBackNavigation.AfterCommit.BeforePaint";
const char kHistogramFromGWSAbortForwardBackBeforeInteraction[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming."
    "ForwardBackNavigation.AfterPaint.Before1sDelayedInteraction";
const char kHistogramFromGWSAbortBackgroundBeforeCommit[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Background."
    "BeforeCommit";
const char kHistogramFromGWSAbortBackgroundBeforePaint[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Background."
    "AfterCommit.BeforePaint";
const char kHistogramFromGWSAbortBackgroundBeforeInteraction[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Background."
    "AfterPaint.BeforeInteraction";

const char kHistogramFromGWSForegroundDuration[] =
    "PageLoad.Clients.FromGoogleSearch.PageTiming.ForegroundDuration";
const char kHistogramFromGWSForegroundDurationAfterPaint[] =
    "PageLoad.Clients.FromGoogleSearch.PageTiming.ForegroundDuration."
    "AfterPaint";
const char kHistogramFromGWSForegroundDurationWithPaint[] =
    "PageLoad.Clients.FromGoogleSearch.PageTiming.ForegroundDuration."
    "WithPaint";
const char kHistogramFromGWSForegroundDurationWithoutPaint[] =
    "PageLoad.Clients.FromGoogleSearch.PageTiming.ForegroundDuration."
    "WithoutPaint";
const char kHistogramFromGWSForegroundDurationNoCommit[] =
    "PageLoad.Clients.FromGoogleSearch.PageTiming.ForegroundDuration.NoCommit";

}  // namespace internal

namespace {

void LogCommittedAbortsBeforePaint(PageAbortReason abort_reason,
                                   base::TimeDelta page_end_time) {
  switch (abort_reason) {
    case PageAbortReason::ABORT_STOP:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortStopBeforePaint,
                          page_end_time);
      break;
    case PageAbortReason::ABORT_CLOSE:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortCloseBeforePaint,
                          page_end_time);
      break;
    case PageAbortReason::ABORT_NEW_NAVIGATION:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortNewNavigationBeforePaint,
          page_end_time);
      break;
    case PageAbortReason::ABORT_RELOAD:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortReloadBeforePaint,
                          page_end_time);
      break;
    case PageAbortReason::ABORT_FORWARD_BACK:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortForwardBackBeforePaint,
          page_end_time);
      break;
    case PageAbortReason::ABORT_BACKGROUND:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortBackgroundBeforePaint,
                          page_end_time);
      break;
    default:
      // These should only be logged for provisional aborts.
      DCHECK_NE(abort_reason, PageAbortReason::ABORT_OTHER);
      break;
  }
}

void LogAbortsAfterPaintBeforeInteraction(
    const page_load_metrics::PageAbortInfo& abort_info) {
  switch (abort_info.reason) {
    case PageAbortReason::ABORT_STOP:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortStopBeforeInteraction,
                          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_CLOSE:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortCloseBeforeInteraction,
          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_NEW_NAVIGATION:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortNewNavigationBeforeInteraction,
          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_RELOAD:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortReloadBeforeInteraction,
          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_FORWARD_BACK:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortForwardBackBeforeInteraction,
          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_BACKGROUND:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortBackgroundBeforeInteraction,
          abort_info.time_to_abort);
      break;
    default:
      // These should only be logged for provisional aborts.
      DCHECK_NE(abort_info.reason, PageAbortReason::ABORT_OTHER);
      break;
  }
}

void LogProvisionalAborts(const page_load_metrics::PageAbortInfo& abort_info) {
  switch (abort_info.reason) {
    case PageAbortReason::ABORT_STOP:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortStopBeforeCommit,
                          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_CLOSE:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortCloseBeforeCommit,
                          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_OTHER:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortOtherBeforeCommit,
                          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_NEW_NAVIGATION:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortNewNavigationBeforeCommit,
          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_RELOAD:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortReloadBeforeCommit,
                          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_FORWARD_BACK:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortForwardBackBeforeCommit,
          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_BACKGROUND:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortBackgroundBeforeCommit,
          abort_info.time_to_abort);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void LogForegroundDurations(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate,
    base::TimeTicks app_background_time) {
  base::Optional<base::TimeDelta> foreground_duration =
      page_load_metrics::GetInitialForegroundDuration(delegate,
                                                      app_background_time);
  if (!foreground_duration)
    return;

  if (delegate.DidCommit()) {
    PAGE_LOAD_LONG_HISTOGRAM(internal::kHistogramFromGWSForegroundDuration,
                             foreground_duration.value());
    if (timing.paint_timing->first_paint &&
        timing.paint_timing->first_paint < foreground_duration) {
      PAGE_LOAD_LONG_HISTOGRAM(
          internal::kHistogramFromGWSForegroundDurationAfterPaint,
          foreground_duration.value() -
              timing.paint_timing->first_paint.value());
      PAGE_LOAD_LONG_HISTOGRAM(
          internal::kHistogramFromGWSForegroundDurationWithPaint,
          foreground_duration.value());
    } else {
      PAGE_LOAD_LONG_HISTOGRAM(
          internal::kHistogramFromGWSForegroundDurationWithoutPaint,
          foreground_duration.value());
    }
  } else {
    PAGE_LOAD_LONG_HISTOGRAM(
        internal::kHistogramFromGWSForegroundDurationNoCommit,
        foreground_duration.value());
  }
}

bool WasAbortedInForeground(
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate,
    const page_load_metrics::PageAbortInfo& abort_info) {
  if (!delegate.StartedInForeground() ||
      abort_info.reason == PageAbortReason::ABORT_NONE)
    return false;

  base::Optional<base::TimeDelta> time_to_abort(abort_info.time_to_abort);
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          time_to_abort, delegate))
    return true;

  const base::TimeDelta time_to_first_background =
      delegate.GetFirstBackgroundTime().value();
  DCHECK_GT(abort_info.time_to_abort, time_to_first_background);
  base::TimeDelta background_abort_delta =
      abort_info.time_to_abort - time_to_first_background;
  // Consider this a foregrounded abort if it occurred within 100ms of a
  // background. This is needed for closing some tabs, where the signal for
  // background is often slightly ahead of the signal for close.
  if (background_abort_delta.InMilliseconds() < 100)
    return true;
  return false;
}

bool WasAbortedBeforeInteraction(
    const page_load_metrics::PageAbortInfo& abort_info,
    const base::Optional<base::TimeDelta>& time_to_interaction) {
  // These conditions should be guaranteed by the call to
  // WasAbortedInForeground, which is called before WasAbortedBeforeInteraction
  // gets invoked.
  DCHECK(abort_info.reason != PageAbortReason::ABORT_NONE);

  if (!time_to_interaction)
    return true;
  // For the case the abort is a reload or forward_back. Since pull to
  // reload / forward_back is the most common user case such aborts being
  // triggered, add a sanitization threshold here: if the first user
  // interaction are received before a reload / forward_back in a very
  // short time, treat the interaction as a gesture to perform the abort.

  // Why 1000ms?
  // 1000ms is enough to perform a pull to reload / forward_back gesture.
  // It's also too short a time for a user to consume any content
  // revealed by the interaction.
  if (abort_info.reason == PageAbortReason::ABORT_RELOAD ||
      abort_info.reason == PageAbortReason::ABORT_FORWARD_BACK) {
    return time_to_interaction.value() +
               base::TimeDelta::FromMilliseconds(1000) >
           abort_info.time_to_abort;
  } else {
    return time_to_interaction > abort_info.time_to_abort;
  }
}

}  // namespace

FromGWSPageLoadMetricsLogger::FromGWSPageLoadMetricsLogger() = default;
FromGWSPageLoadMetricsLogger::~FromGWSPageLoadMetricsLogger() = default;

void FromGWSPageLoadMetricsLogger::SetPreviouslyCommittedUrl(const GURL& url) {
  previously_committed_url_is_search_results_ =
      page_load_metrics::IsGoogleSearchResultUrl(url);
  previously_committed_url_is_search_redirector_ =
      page_load_metrics::IsGoogleSearchRedirectorUrl(url);
}

void FromGWSPageLoadMetricsLogger::SetProvisionalUrl(const GURL& url) {
  provisional_url_has_search_hostname_ =
      page_load_metrics::IsGoogleSearchHostname(url);
}

FromGWSPageLoadMetricsObserver::FromGWSPageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FromGWSPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  logger_.SetPreviouslyCommittedUrl(currently_committed_url);
  logger_.SetProvisionalUrl(navigation_handle->GetURL());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FromGWSPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  // We'd like to also check navigation_handle->HasUserGesture() here, however
  // this signal is not carried forward for navigations that open links in new
  // tabs, so we look only at PAGE_TRANSITION_LINK. Back/forward navigations
  // that were originally navigated from a link will continue to report a core
  // type of link, so to filter out back/forward navs, we also check that the
  // page transition is a new navigation.
  logger_.set_navigation_initiated_via_link(
      ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                   ui::PAGE_TRANSITION_LINK) &&
      ui::PageTransitionIsNewNavigation(
          navigation_handle->GetPageTransition()));

  logger_.SetNavigationStart(navigation_handle->NavigationStart());
  logger_.OnCommit(navigation_handle, source_id);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FromGWSPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  logger_.FlushMetricsOnAppEnterBackground(timing, GetDelegate());
  return STOP_OBSERVING;
}

void FromGWSPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  logger_.OnDomContentLoadedEventStart(timing, GetDelegate());
}

void FromGWSPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  logger_.OnLoadEventStart(timing, GetDelegate());
}

void FromGWSPageLoadMetricsObserver::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  logger_.OnFirstPaintInPage(timing, GetDelegate());
}

void FromGWSPageLoadMetricsObserver::OnFirstImagePaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  logger_.OnFirstImagePaintInPage(timing, GetDelegate());
}

void FromGWSPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  logger_.OnFirstContentfulPaintInPage(timing, GetDelegate());
}

void FromGWSPageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  logger_.OnFirstInputInPage(timing, GetDelegate());
}

void FromGWSPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  logger_.OnParseStart(timing, GetDelegate());
}

void FromGWSPageLoadMetricsObserver::OnParseStop(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  logger_.OnParseStop(timing, GetDelegate());
}

void FromGWSPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  logger_.OnComplete(timing, GetDelegate());
}

void FromGWSPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info) {
  logger_.OnFailedProvisionalLoad(failed_load_info, GetDelegate());
}

void FromGWSPageLoadMetricsObserver::OnUserInput(
    const blink::WebInputEvent& event,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  logger_.OnUserInput(event, timing, GetDelegate());
}

void FromGWSPageLoadMetricsObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  logger_.OnTimingUpdate(subframe_rfh, timing);
}

void FromGWSPageLoadMetricsObserver::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  logger_.OnDidFinishSubFrameNavigation(navigation_handle, GetDelegate());
}

void FromGWSPageLoadMetricsLogger::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  if (!ShouldLogPostCommitMetrics(navigation_handle->GetURL()))
    return;
  ukm::builders::PageLoad_FromGoogleSearch(source_id).Record(
      ukm::UkmRecorder::Get());
}

void FromGWSPageLoadMetricsLogger::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  if (!ShouldLogPostCommitMetrics(delegate.GetUrl()))
    return;

  LogMetricsOnComplete(delegate);

  page_load_metrics::PageAbortInfo abort_info = GetPageAbortInfo(delegate);
  if (!WasAbortedInForeground(delegate, abort_info))
    return;

  // If we did not receive any timing IPCs from the render process, we can't
  // know for certain if the page was truly aborted before paint, or if the
  // abort happened before we received the IPC from the render process. Thus, we
  // do not log aborts for these page loads. Tracked page loads that receive no
  // timing IPCs are tracked via the ERR_NO_IPCS_RECEIVED error code in the
  // PageLoad.Events.InternalError histogram, so we can keep track of how often
  // this happens.
  if (page_load_metrics::IsEmpty(timing))
    return;

  if (!timing.paint_timing->first_paint ||
      timing.paint_timing->first_paint >= abort_info.time_to_abort) {
    LogCommittedAbortsBeforePaint(abort_info.reason, abort_info.time_to_abort);
  } else if (WasAbortedBeforeInteraction(abort_info,
                                         first_user_interaction_after_paint_)) {
    LogAbortsAfterPaintBeforeInteraction(abort_info);
  }

  LogForegroundDurations(timing, delegate, base::TimeTicks());
}

void FromGWSPageLoadMetricsLogger::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  if (!ShouldLogFailedProvisionalLoadMetrics())
    return;

  page_load_metrics::PageAbortInfo abort_info = GetPageAbortInfo(delegate);
  if (!WasAbortedInForeground(delegate, abort_info))
    return;

  LogProvisionalAborts(abort_info);

  LogForegroundDurations(page_load_metrics::mojom::PageLoadTiming(), delegate,
                         base::TimeTicks());
}

bool FromGWSPageLoadMetricsLogger::ShouldLogFailedProvisionalLoadMetrics() {
  // See comment in ShouldLogPostCommitMetrics above the call to
  // page_load_metrics::IsGoogleSearchHostname for more info on this if test.
  if (provisional_url_has_search_hostname_)
    return false;

  return previously_committed_url_is_search_results_ ||
         previously_committed_url_is_search_redirector_;
}

bool FromGWSPageLoadMetricsLogger::ShouldLogPostCommitMetrics(const GURL& url) {
  DCHECK(!url.is_empty());

  // If this page has a URL on a known google search hostname, then it may be a
  // page associated with search (either a search results page, or a search
  // redirector url), so we should not log stats. We could try to detect only
  // the specific known search URLs here, and log navigations to other pages on
  // the google search hostname (for example, a search for 'about google'
  // includes a result for https://www.google.com/about/), however, we assume
  // these cases are relatively uncommon, and we run the risk of logging metrics
  // for some search redirector URLs. Thus we choose the more conservative
  // approach of ignoring all urls on known search hostnames.
  if (page_load_metrics::IsGoogleSearchHostname(url))
    return false;

  // We're only interested in tracking navigations (e.g. clicks) initiated via
  // links. Note that the redirector will mask these, so don't enforce this if
  // the navigation came from a redirect url. TODO(csharrison): Use this signal
  // for provisional loads when the content APIs allow for it.
  if (previously_committed_url_is_search_results_ &&
      navigation_initiated_via_link_) {
    return true;
  }

  // If the navigation was via the search redirector, then the information about
  // whether the navigation was from a link would have been associated with the
  // navigation to the redirector, and not included in the redirected
  // navigation. Therefore, do not require link navigation this case.
  return previously_committed_url_is_search_redirector_;
}

bool FromGWSPageLoadMetricsLogger::ShouldLogForegroundEventAfterCommit(
    const base::Optional<base::TimeDelta>& event,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  DCHECK(delegate.DidCommit())
      << "ShouldLogForegroundEventAfterCommit called without committed URL.";
  return ShouldLogPostCommitMetrics(delegate.GetUrl()) &&
         page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
             event, delegate);
}

void FromGWSPageLoadMetricsLogger::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  if (ShouldLogForegroundEventAfterCommit(
          timing.document_timing->dom_content_loaded_event_start, delegate)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramFromGWSDomContentLoaded,
        timing.document_timing->dom_content_loaded_event_start.value());
  }
}

void FromGWSPageLoadMetricsLogger::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  if (ShouldLogForegroundEventAfterCommit(
          timing.document_timing->load_event_start, delegate)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSLoad,
                        timing.document_timing->load_event_start.value());
  }
}

void FromGWSPageLoadMetricsLogger::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  if (ShouldLogForegroundEventAfterCommit(timing.paint_timing->first_paint,
                                          delegate)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSFirstPaint,
                        timing.paint_timing->first_paint.value());
  }
  first_paint_triggered_ = true;
}

void FromGWSPageLoadMetricsLogger::OnFirstImagePaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  if (ShouldLogForegroundEventAfterCommit(
          timing.paint_timing->first_image_paint, delegate)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSFirstImagePaint,
                        timing.paint_timing->first_image_paint.value());
  }
}

void FromGWSPageLoadMetricsLogger::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  if (ShouldLogForegroundEventAfterCommit(
          timing.paint_timing->first_contentful_paint, delegate)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value());

    // If we have a foreground paint, we should have a foreground parse start,
    // since paints can't happen until after parsing starts.
    DCHECK(page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
        timing.parse_timing->parse_start, delegate));
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramFromGWSParseStartToFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value() -
            timing.parse_timing->parse_start.value());
  }
}

void FromGWSPageLoadMetricsLogger::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  if (ShouldLogForegroundEventAfterCommit(
          timing.interactive_timing->first_input_delay, delegate)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSFirstInputDelay,
                        timing.interactive_timing->first_input_delay.value());
  }
}

void FromGWSPageLoadMetricsLogger::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  if (ShouldLogForegroundEventAfterCommit(timing.parse_timing->parse_start,
                                          delegate)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSParseStart,
                        timing.parse_timing->parse_start.value());
  }
}

void FromGWSPageLoadMetricsLogger::OnParseStop(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  if (ShouldLogForegroundEventAfterCommit(timing.parse_timing->parse_stop,
                                          delegate)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSParseDuration,
                        timing.parse_timing->parse_stop.value() -
                            timing.parse_timing->parse_start.value());
  }
}

void FromGWSPageLoadMetricsLogger::OnUserInput(
    const blink::WebInputEvent& event,
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  if (first_paint_triggered_ && !first_user_interaction_after_paint_) {
    DCHECK(!navigation_start_.is_null());
    first_user_interaction_after_paint_ =
        base::TimeTicks::Now() - navigation_start_;
  }
}

void FromGWSPageLoadMetricsLogger::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  LogMetricsOnComplete(delegate);
  LogForegroundDurations(timing, delegate, base::TimeTicks::Now());
}

void FromGWSPageLoadMetricsLogger::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  largest_contentful_paint_handler_.RecordTiming(timing.paint_timing,
                                                 subframe_rfh);
}

void FromGWSPageLoadMetricsLogger::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  largest_contentful_paint_handler_.OnDidFinishSubFrameNavigation(
      navigation_handle, delegate);
}

void FromGWSPageLoadMetricsLogger::LogMetricsOnComplete(
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  if (!delegate.DidCommit() || !ShouldLogPostCommitMetrics(delegate.GetUrl()))
    return;

  const page_load_metrics::ContentfulPaintTimingInfo&
      all_frames_largest_contentful_paint =
          largest_contentful_paint_handler_.MergeMainFrameAndSubframes();
  if (!all_frames_largest_contentful_paint.IsEmpty() &&
      WasStartedInForegroundOptionalEventInForeground(
          all_frames_largest_contentful_paint.Time(), delegate)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSLargestContentfulPaint,
                        all_frames_largest_contentful_paint.Time().value());
  }
}
