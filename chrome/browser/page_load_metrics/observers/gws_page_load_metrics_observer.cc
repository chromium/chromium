// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/gws_page_load_metrics_observer.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/trace_event/named_trigger.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"
#include "chrome/common/webui_url_constants.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/google/browser/gws_abandoned_page_load_metrics_observer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

using page_load_metrics::PageAbortReason;

namespace internal {

#define HISTOGRAM_PREFIX "PageLoad.Clients.GoogleSearch."

const char kHistogramGWSNavigationStartToFinalRequestStart[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFinalRequestStart";
const char kHistogramGWSNavigationStartToFinalResponseStart[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFinalResponseStart";
const char kHistogramGWSNavigationStartToFinalLoaderCallback[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFinalLoaderCallback";
const char kHistogramGWSNavigationStartToFirstRequestStart[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFirstRequestStart";
const char kHistogramGWSNavigationStartToFirstResponseStart[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFirstResponseStart";
const char kHistogramGWSNavigationStartToFirstLoaderCallback[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFirstLoaderCallback";
const char kHistogramGWSNavigationStartToOnComplete[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToOnComplete";

const char kHistogramGWSConnectTimingFirstRequestDomainLookupDelay[] =
    HISTOGRAM_PREFIX "ConnectTiming.FirstRequestDomainLookupDelay";
const char kHistogramGWSConnectTimingFirstRequestConnectDelay[] =
    HISTOGRAM_PREFIX "ConnectTiming.FirstRequestConnectDelay";
const char kHistogramGWSConnectTimingFirstRequestSslDelay[] =
    HISTOGRAM_PREFIX "ConnectTiming.FirstRequestSslDelay";
const char kHistogramGWSConnectTimingFinalRequestDomainLookupDelay[] =
    HISTOGRAM_PREFIX "ConnectTiming.FinalRequestDomainLookupDelay";
const char kHistogramGWSConnectTimingFinalRequestConnectDelay[] =
    HISTOGRAM_PREFIX "ConnectTiming.FinalRequestConnectDelay";
const char kHistogramGWSConnectTimingFinalRequestSslDelay[] =
    HISTOGRAM_PREFIX "ConnectTiming.FinalRequestSslDelay";

const char kHistogramGWSAFTEnd[] = HISTOGRAM_PREFIX "PaintTiming.AFTEnd";
const char kHistogramGWSAFTStart[] = HISTOGRAM_PREFIX "PaintTiming.AFTStart";
const char kHistogramGWSHeaderChunkStart[] =
    HISTOGRAM_PREFIX "PaintTiming.HeaderChunkStart";
const char kHistogramGWSHeaderChunkEnd[] =
    HISTOGRAM_PREFIX "PaintTiming.HeaderChunkEnd";
const char kHistogramGWSBodyChunkStart[] =
    HISTOGRAM_PREFIX "PaintTiming.BodyChunkStart";
const char kHistogramGWSBodyChunkEnd[] =
    HISTOGRAM_PREFIX "PaintTiming.BodyChunkEnd";
const char kHistogramGWSFirstContentfulPaint[] =
    HISTOGRAM_PREFIX "PaintTiming.NavigationToFirstContentfulPaint";
const char kHistogramGWSLargestContentfulPaint[] =
    HISTOGRAM_PREFIX "PaintTiming.NavigationToLargestContentfulPaint";
const char kHistogramGWSParseStart[] =
    HISTOGRAM_PREFIX "ParseTiming.NavigationToParseStart";
const char kHistogramGWSConnectStart[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationToConnectStart2";
const char kHistogramGWSDomainLookupStart[] =
    HISTOGRAM_PREFIX "DomainLookupTiming.NavigationToDomainLookupStart2";
const char kHistogramGWSDomainLookupEnd[] =
    HISTOGRAM_PREFIX "DomainLookupTiming.NavigationToDomainLookupEnd2";

const char kHistogramGWSHST[] = HISTOGRAM_PREFIX "CSI.HeadChunkStartTime";
const char kHistogramGWSHCT[] = HISTOGRAM_PREFIX "CSI.HeadChunkContentTime";
const char kHistogramGWSSCT[] = HISTOGRAM_PREFIX "CSI.SearchContentTime";
const char kHistogramGWSSRT[] = HISTOGRAM_PREFIX "CSI.ServerResponseTime";
const char kHistogramGWSTimeBetweenHCTAndSCT[] =
    HISTOGRAM_PREFIX "CSI.TimeBetweenHCTAndSCT";

const char kHistogramGWSNavigationSourceType[] =
    HISTOGRAM_PREFIX "NavigationSourceType";

const char kHistogramGWSIsFirstNavigationForGWS[] =
    HISTOGRAM_PREFIX "IsFirstNavigationForGWS";

}  // namespace internal

namespace {
bool IsNavigationFromNewTabPage(
    GWSPageLoadMetricsObserver::NavigationSourceType type) {
  switch (type) {
    case GWSPageLoadMetricsObserver::kFromNewTabPage:
    case GWSPageLoadMetricsObserver::kStartedInBackgroundFromNewTabPage:
      return true;
    case GWSPageLoadMetricsObserver::kFromGWSPage:
    case GWSPageLoadMetricsObserver::kUnknown:
    case GWSPageLoadMetricsObserver::kStartedInBackgroundFromGWSPage:
    case GWSPageLoadMetricsObserver::kStartedInBackground:
      return false;
  }
}

GWSPageLoadMetricsObserver::NavigationSourceType GetBackgroundedState(
    GWSPageLoadMetricsObserver::NavigationSourceType type) {
  switch (type) {
    case GWSPageLoadMetricsObserver::kFromNewTabPage:
      return GWSPageLoadMetricsObserver::kStartedInBackgroundFromNewTabPage;
    case GWSPageLoadMetricsObserver::kFromGWSPage:
      return GWSPageLoadMetricsObserver::kStartedInBackgroundFromGWSPage;
    case GWSPageLoadMetricsObserver::kUnknown:
      return GWSPageLoadMetricsObserver::kStartedInBackground;
    case GWSPageLoadMetricsObserver::kStartedInBackgroundFromGWSPage:
    case GWSPageLoadMetricsObserver::kStartedInBackgroundFromNewTabPage:
    case GWSPageLoadMetricsObserver::kStartedInBackground:
      // Types that already have backgrounded types
      return type;
  }
}
}  // namespace

GWSPageLoadMetricsObserver::GWSPageLoadMetricsObserver() {
  static bool is_first_navigation = true;
  is_first_navigation_ = is_first_navigation;
  is_first_navigation = false;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  navigation_id_ = navigation_handle->GetNavigationId();
  if (page_load_metrics::IsGoogleSearchResultUrl(navigation_handle->GetURL())) {
    // Emit a trigger to allow trace collection tied to gws navigations.
    base::trace_event::EmitNamedTrigger("gws-navigation-start");
  }

  // Determine the source of the navigation.
  if (IsFromNewTabPage(navigation_handle)) {
    source_type_ = kFromNewTabPage;
  } else if (page_load_metrics::IsGoogleSearchResultUrl(
                 currently_committed_url)) {
    source_type_ = kFromGWSPage;
  }

  // Since `kFromNewTabPage` / `kFromGWSPage` and `kStartedInBackground` may
  // not be mutual exclusive, we also consider the case where both cases may
  // be satisfied (i.e. check if the navigation comes from background and was
  // from NTP/ GWS).
  if (!started_in_foreground) {
    source_type_ = GetBackgroundedState(source_type_);
  }

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  const bool is_gws_url =
      page_load_metrics::IsGoogleSearchResultUrl(navigation_handle->GetURL());
  if (is_first_navigation_) {
    base::UmaHistogramBoolean(internal::kHistogramGWSIsFirstNavigationForGWS,
                              is_gws_url);
  }
  if (!is_gws_url) {
    return STOP_OBSERVING;
  }

  navigation_handle_timing_ = navigation_handle->GetNavigationHandleTiming();
  RecordPreCommitHistograms();
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // TODO(crbug.com/40222513): Handle Prerendering cases.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class is interested only in events that are preprocessed and
  // dispatched also to the outermost page at PageLoadTracker. So, this class
  // doesn't need to forward events for FencedFrames.
  return STOP_OBSERVING;
}

void GWSPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    return;
  }

  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSFirstContentfulPaint,
                      timing.paint_timing->first_contentful_paint.value());
}

void GWSPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_start, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSParseStart,
                      timing.parse_timing->parse_start.value());
}

void GWSPageLoadMetricsObserver::OnConnectStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.connect_start, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(AddHistogramSuffix(internal::kHistogramGWSConnectStart),
                      timing.connect_start.value());
}

void GWSPageLoadMetricsObserver::OnDomainLookupStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.domain_lookup_timing->domain_lookup_start, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupStart),
      timing.domain_lookup_timing->domain_lookup_start.value());
}

void GWSPageLoadMetricsObserver::OnDomainLookupEnd(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.domain_lookup_timing->domain_lookup_end, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupEnd),
      timing.domain_lookup_timing->domain_lookup_end.value());
}

void GWSPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  const base::TimeTicks navigation_start = GetDelegate().GetNavigationStart();
  if (!navigation_start.is_null()) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSNavigationStartToOnComplete,
                        base::TimeTicks::Now() - navigation_start);
  }
  LogMetricsOnComplete();
}

void GWSPageLoadMetricsObserver::OnCustomUserTimingMarkObserved(
    const std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr>&
        timings) {
  for (const auto& mark : timings) {
    if (mark->mark_name == internal::kGwsAFTStartMarkName) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSAFTStart, mark->start_time);
      aft_start_time_ = mark->start_time;
    } else if (mark->mark_name == internal::kGwsAFTEndMarkName) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSAFTEnd, mark->start_time);
      aft_end_time_ = mark->start_time;
    } else if (mark->mark_name == internal::kGwsHeaderChunkStartMarkName) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSHeaderChunkStart,
                          mark->start_time);
      header_chunk_start_time_ = mark->start_time;
    } else if (mark->mark_name == internal::kGwsHeaderChunkEndMarkName) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSHeaderChunkEnd,
                          mark->start_time);
      header_chunk_end_time_ = mark->start_time;
    } else if (mark->mark_name == internal::kGwsBodyChunkStartMarkName) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSBodyChunkStart,
                          mark->start_time);
      body_chunk_start_time_ = mark->start_time;
    } else if (mark->mark_name == internal::kGwsBodyChunkEndMarkName) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSBodyChunkEnd,
                          mark->start_time);
    }
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  LogMetricsOnComplete();
  return STOP_OBSERVING;
}

void GWSPageLoadMetricsObserver::LogMetricsOnComplete() {
  const page_load_metrics::ContentfulPaintTimingInfo&
      all_frames_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .MergeMainFrameAndSubframes();
  if (!all_frames_largest_contentful_paint.ContainsValidTime() ||
      !WasStartedInForegroundOptionalEventInForeground(
          all_frames_largest_contentful_paint.Time(), GetDelegate())) {
    return;
  }
  RecordNavigationTimingHistograms();
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSLargestContentfulPaint,
                      all_frames_largest_contentful_paint.Time().value());
}

void GWSPageLoadMetricsObserver::RecordNavigationTimingHistograms() {
  const base::TimeTicks navigation_start_time =
      GetDelegate().GetNavigationStart();
  const content::NavigationHandleTiming& timing = navigation_handle_timing_;

  // Record metrics for navigation only when all relevant milestones are
  // recorded and in the expected order. It is allowed that they have the same
  // value for some cases (e.g., internal redirection for HSTS).
  if (navigation_start_time.is_null() ||
      timing.first_request_start_time.is_null() ||
      timing.first_response_start_time.is_null() ||
      timing.first_loader_callback_time.is_null() ||
      timing.final_request_start_time.is_null() ||
      timing.final_response_start_time.is_null() ||
      timing.final_loader_callback_time.is_null() ||
      timing.navigation_commit_sent_time.is_null()) {
    return;
  }

  // Record the elapsed time from the navigation start milestone.
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSNavigationStartToFirstRequestStart,
                      timing.first_request_start_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramGWSNavigationStartToFirstResponseStart,
      timing.first_response_start_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramGWSNavigationStartToFirstLoaderCallback,
      timing.first_loader_callback_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSNavigationStartToFinalRequestStart,
                      timing.final_request_start_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramGWSNavigationStartToFinalResponseStart,
      timing.final_response_start_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramGWSNavigationStartToFinalLoaderCallback,
      timing.final_loader_callback_time - navigation_start_time);

  PAGE_LOAD_SHORT_HISTOGRAM(
      internal::kHistogramGWSConnectTimingFirstRequestDomainLookupDelay,
      timing.first_request_domain_lookup_delay);
  PAGE_LOAD_SHORT_HISTOGRAM(
      internal::kHistogramGWSConnectTimingFirstRequestConnectDelay,
      timing.first_request_connect_delay);
  PAGE_LOAD_SHORT_HISTOGRAM(
      internal::kHistogramGWSConnectTimingFirstRequestSslDelay,
      timing.first_request_ssl_delay);
  PAGE_LOAD_SHORT_HISTOGRAM(
      internal::kHistogramGWSConnectTimingFinalRequestDomainLookupDelay,
      timing.final_request_domain_lookup_delay);
  PAGE_LOAD_SHORT_HISTOGRAM(
      internal::kHistogramGWSConnectTimingFinalRequestConnectDelay,
      timing.final_request_connect_delay);
  PAGE_LOAD_SHORT_HISTOGRAM(
      internal::kHistogramGWSConnectTimingFinalRequestSslDelay,
      timing.final_request_ssl_delay);

  // Record latency trace events.
  RecordLatencyHitograms(timing.non_redirect_response_start_time);

  // Record trace events according to the navigation milestone.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "loading", "GWSNavigationStartToFirstRequestStart", TRACE_ID_LOCAL(this),
      navigation_start_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "loading", "GWSNavigationStartToFirstRequestStart", TRACE_ID_LOCAL(this),
      timing.first_request_start_time);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "loading", "GWSFirstRequestStartToFirstResponseStart",
      TRACE_ID_LOCAL(this), timing.first_request_start_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "loading", "GWSFirstRequestStartToFirstResponseStart",
      TRACE_ID_LOCAL(this), timing.first_response_start_time);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "loading", "GWSFirstResponseStartToFirstLoaderCallback",
      TRACE_ID_LOCAL(this), timing.first_response_start_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "loading", "GWSFirstResponseStartToFirstLoaderCallback",
      TRACE_ID_LOCAL(this), timing.first_loader_callback_time);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "loading", "GWSFirstLoadCallbackToFinalResponseStart",
      TRACE_ID_LOCAL(this), timing.first_loader_callback_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "loading", "GWSFirstLoadCallbackToFinalResponseStart",
      TRACE_ID_LOCAL(this), timing.final_response_start_time);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "loading", "GWSFinalResponseStartToFinalLoaderCallback",
      TRACE_ID_LOCAL(this), timing.final_response_start_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "loading", "GWSFinalResponseStartToFinalLoaderCallback",
      TRACE_ID_LOCAL(this), timing.final_loader_callback_time);
}

void GWSPageLoadMetricsObserver::RecordPreCommitHistograms() {
  base::UmaHistogramEnumeration(internal::kHistogramGWSNavigationSourceType,
                                source_type_);
}

bool GWSPageLoadMetricsObserver::IsFromNewTabPage(
    content::NavigationHandle* navigation_handle) {
  auto* start_instance = navigation_handle->GetStartingSiteInstance();
  if (!start_instance) {
    return false;
  }

  auto origin = start_instance->GetSiteURL();

  GURL ntp_url(chrome::kChromeUINewTabPageURL);
  return ntp_url.scheme_piece() == origin.scheme_piece() &&
         ntp_url.host_piece() == origin.host_piece();
}

std::string GWSPageLoadMetricsObserver::AddHistogramSuffix(
    const std::string& histogram_name) {
  std::string suffix =
      (is_first_navigation_ ? internal::kSuffixFirstNavigation
                            : internal::kSuffixSubsequentNavigation);
  if (!AfterStartupTaskUtils::IsBrowserStartupComplete()) {
    suffix += internal::kSuffixIsBrowserStarting;
  }

  if (IsNavigationFromNewTabPage(source_type_)) {
    suffix += internal::kSuffixFromNewTabPage;
  }

  return histogram_name + suffix;
}

void GWSPageLoadMetricsObserver::RecordLatencyHitograms(
    base::TimeTicks response_start_time) {
  const auto trace_id =
      TRACE_ID_WITH_SCOPE("GWSLatencyEvent", TRACE_ID_LOCAL(navigation_id_));
  // TODO(crbug.com/364278026): SRT starts from the time when the user submits
  // a query. Using the navigation start time may not perfect to measure SRT.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "navigation", "GWSLatency:SRT", trace_id,
      GetDelegate().GetNavigationStart());
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0("navigation", "GWSLatency:SRT",
                                                 trace_id, response_start_time);
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSSRT,
                      response_start_time - GetDelegate().GetNavigationStart());

  // Log some important CSI metrics only when related submetrics are recorded.
  std::optional<base::TimeDelta> hct_time;
  std::optional<base::TimeDelta> sct_time;

  if (aft_end_time_.has_value()) {
    // Currently `aft_start_time_` has the value of the server response time,
    // but in theory AFT starts at the end of SRT, the time when the client
    // receives the first byte of the header chunk.
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "navigation", "GWSLatency:AFT", trace_id, response_start_time);
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "navigation", "GWSLatency:AFT", trace_id,
        GetDelegate().GetNavigationStart() + aft_end_time_.value());
  }
  if (body_chunk_start_time_.has_value()) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "navigation", "GWSLatency:SCT", trace_id, response_start_time);
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "navigation", "GWSLatency:SCT", trace_id,
        GetDelegate().GetNavigationStart() + body_chunk_start_time_.value());
    sct_time = GetDelegate().GetNavigationStart() +
               body_chunk_start_time_.value() - response_start_time;
    PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSSCT, sct_time.value());
  }
  if (header_chunk_end_time_.has_value()) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "navigation", "GWSLatency:HCT", trace_id, response_start_time);
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "navigation", "GWSLatency:HCT", trace_id,
        GetDelegate().GetNavigationStart() + header_chunk_end_time_.value());
    hct_time = GetDelegate().GetNavigationStart() +
               header_chunk_end_time_.value() - response_start_time;
    PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSHCT, hct_time.value());
  }
  if (header_chunk_start_time_.has_value()) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "navigation", "GWSLatency:HST", trace_id, response_start_time);
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "navigation", "GWSLatency:HST", trace_id,
        GetDelegate().GetNavigationStart() + header_chunk_start_time_.value());
    PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSHST,
                        GetDelegate().GetNavigationStart() +
                            header_chunk_start_time_.value() -
                            response_start_time);
  }
  if (sct_time.has_value() && hct_time.has_value()) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSTimeBetweenHCTAndSCT,
                        sct_time.value() - hct_time.value());
  }
}
