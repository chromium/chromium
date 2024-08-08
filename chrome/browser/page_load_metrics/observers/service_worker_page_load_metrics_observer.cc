// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/service_worker_page_load_metrics_observer.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/observers/from_gws_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "net/http/http_response_headers.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"

namespace internal {

const char kHistogramServiceWorkerParseStart[] =
    "PageLoad.Clients.ServiceWorker2.ParseTiming.NavigationToParseStart";
const char kHistogramServiceWorkerParseStartForwardBack[] =
    "PageLoad.Clients.ServiceWorker2.ParseTiming.NavigationToParseStart."
    "LoadType.ForwardBackNavigation";
const char kHistogramServiceWorkerParseStartForwardBackNoStore[] =
    "PageLoad.Clients.ServiceWorker2.ParseTiming.NavigationToParseStart."
    "LoadType.ForwardBackNavigation.NoStore";
const char kBackgroundHistogramServiceWorkerParseStart[] =
    "PageLoad.Clients.ServiceWorker2.ParseTiming.NavigationToParseStart."
    "Background";
const char kHistogramServiceWorkerFirstPaint[] =
    "PageLoad.Clients.ServiceWorker2.PaintTiming.NavigationToFirstPaint";
const char kHistogramServiceWorkerFirstContentfulPaint[] =
    "PageLoad.Clients.ServiceWorker2.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramServiceWorkerFirstContentfulPaintForwardBack[] =
    "PageLoad.Clients.ServiceWorker2.PaintTiming."
    "NavigationToFirstContentfulPaint.LoadType.ForwardBackNavigation";
const char kHistogramServiceWorkerFirstContentfulPaintForwardBackNoStore[] =
    "PageLoad.Clients.ServiceWorker2.PaintTiming."
    "NavigationToFirstContentfulPaint.LoadType.ForwardBackNavigation.NoStore";
const char kHistogramServiceWorkerFirstContentfulPaintSkippableFetchHandler[] =
    "PageLoad.Clients.ServiceWorker2.PaintTiming."
    "NavigationToFirstContentfulPaint.SkippableFetchHandler";
const char
    kHistogramServiceWorkerFirstContentfulPaintNonSkippableFetchHandler[] =
        "PageLoad.Clients.ServiceWorker2.PaintTiming."
        "NavigationToFirstContentfulPaint.NonSkippableFetchHandler";
const char
    kHistogramServiceWorkerFirstContentfulPaintRaceNetworkRequestEligible[] =
        "PageLoad.Clients.ServiceWorker2.PaintTiming."
        "NavigationToFirstContentfulPaint.RaceNetworkRequestEligible";
const char kBackgroundHistogramServiceWorkerFirstContentfulPaint[] =
    "PageLoad.Clients.ServiceWorker2.PaintTiming."
    "NavigationToFirstContentfulPaint.Background";
const char kHistogramServiceWorkerParseStartToFirstContentfulPaint[] =
    "PageLoad.Clients.ServiceWorker2.PaintTiming."
    "ParseStartToFirstContentfulPaint";
const char kHistogramServiceWorkerDomContentLoaded[] =
    "PageLoad.Clients.ServiceWorker2.DocumentTiming."
    "NavigationToDOMContentLoadedEventFired";
const char kHistogramServiceWorkerLoad[] =
    "PageLoad.Clients.ServiceWorker2.DocumentTiming.NavigationToLoadEventFired";
const char kHistogramServiceWorkerLargestContentfulPaint[] =
    "PageLoad.Clients.ServiceWorker2.PaintTiming."
    "NavigationToLargestContentfulPaint2";
const char
    kHistogramServiceWorkerLargestContentfulPaintSkippableFetchHandler[] =
        "PageLoad.Clients.ServiceWorker2.PaintTiming."
        "NavigationToLargestContentfulPaint2.SkippableFetchHandler";
const char
    kHistogramServiceWorkerLargestContentfulPaintNonSkippableFetchHandler[] =
        "PageLoad.Clients.ServiceWorker2.PaintTiming."
        "NavigationToLargestContentfulPaint2.NonSkippableFetchHandler";
// Record LCP when the page is eligible for RaceNetworkRequest.
// note: This doesn't mean RaceNetworkRequest is actually dispatched.
const char
    kHistogramServiceWorkerLargestContentfulPaintRaceNetworkRequestEligible[] =
        "PageLoad.Clients.ServiceWorker2.PaintTiming."
        "NavigationToLargestContentfulPaint2.RaceNetworkRequestEligible";

const char kHistogramServiceWorkerParseStartSearch[] =
    "PageLoad.Clients.ServiceWorker2.ParseTiming.NavigationToParseStart.search";
const char kHistogramServiceWorkerFirstContentfulPaintSearch[] =
    "PageLoad.Clients.ServiceWorker2.PaintTiming."
    "NavigationToFirstContentfulPaint.search";
const char kHistogramServiceWorkerParseStartToFirstContentfulPaintSearch[] =
    "PageLoad.Clients.ServiceWorker2.PaintTiming."
    "ParseStartToFirstContentfulPaint.search";
const char kHistogramServiceWorkerDomContentLoadedSearch[] =
    "PageLoad.Clients.ServiceWorker2.DocumentTiming."
    "NavigationToDOMContentLoadedEventFired.search";
const char kHistogramServiceWorkerLoadSearch[] =
    "PageLoad.Clients.ServiceWorker2.DocumentTiming.NavigationToLoadEventFired."
    "search";

const char kHistogramNoServiceWorkerFirstContentfulPaintSearch[] =
    "PageLoad.Clients.NoServiceWorker2.PaintTiming."
    "NavigationToFirstContentfulPaint.search";
const char kHistogramNoServiceWorkerParseStartToFirstContentfulPaintSearch[] =
    "PageLoad.Clients.NoServiceWorker2.PaintTiming."
    "ParseStartToFirstContentfulPaint.search";
const char kHistogramNoServiceWorkerDomContentLoadedSearch[] =
    "PageLoad.Clients.NoServiceWorker2.DocumentTiming."
    "NavigationToDOMContentLoadedEventFired.search";
const char kHistogramNoServiceWorkerLoadSearch[] =
    "PageLoad.Clients.NoServiceWorker2.DocumentTiming."
    "NavigationToLoadEventFired.search";

const char kHistogramServiceWorkerFirstContentfulPaintDocs[] =
    "PageLoad.Clients.ServiceWorker2.PaintTiming."
    "NavigationToFirstContentfulPaint.docs";
const char kHistogramNoServiceWorkerFirstContentfulPaintDocs[] =
    "PageLoad.Clients.NoServiceWorker2.PaintTiming."
    "NavigationToFirstContentfulPaint.docs";

// The naming of the following histogram does not follow typical convention of
// other histograms. This is because this metrics is ServiceWorker static
// routing API related, and is intended to be consistent with
// `ServiceWorker.RouterEvaluator.*` metrics, which are recorded in
// //chrome/browser/page_load_metrics/observers/
// service_worker_page_load_metrics_observer.cc. Since we need to record this
// metrics on complete, we are recording them here.
const char kHistogramServiceWorkerSubresourceTotalRouterEvaluationTime[] =
    "ServiceWorker.RouterEvaluator.SubresourceTotalEvaluationTime";

}  // namespace internal

namespace {

bool IsDocsSite(const GURL& url) {
  return url.host_piece() == "docs.google.com";
}

bool IsForwardBackLoad(ui::PageTransition transition) {
  return transition & ui::PAGE_TRANSITION_FORWARD_BACK;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ServiceWorkerResourceLoadStatus {
  kMainResourceFallbackAndSubResourceFallback = 0,
  kMainResourceFallbackAndSubResourceNotFallback = 1,
  kMainResourceFallbackAndSubResourceMixed = 2,
  kMainResourceFallbackAndNoSubResource = 3,
  kMainResourceNotFallbackAndSubResourceFallback = 4,
  kMainResourceNotFallbackAndSubResourceNotFallback = 5,
  kMainResourceNotFallbackAndSubResourceMixed = 6,
  kMainResourceNotFallbackAndNoSubResource = 7,
};

}  // namespace

ServiceWorkerPageLoadMetricsObserver::ServiceWorkerPageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ServiceWorkerPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // All events this class is interested in are preprocessed and forwarded at
  // PageLoadTracker and observer doesn't need to care for forwarding.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ServiceWorkerPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // As this class are interested in evaluating the performance gain through the
  // service workers, we don't count in prerendered metrics.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ServiceWorkerPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  transition_ = navigation_handle->GetPageTransition();
  const net::HttpResponseHeaders* headers =
      navigation_handle->GetResponseHeaders();
  if (headers) {
    was_no_store_main_resource_ =
        headers->HasHeaderValue("cache-control", "no-store");
  }
  return CONTINUE_OBSERVING;
}

void ServiceWorkerPageLoadMetricsObserver::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!IsServiceWorkerControlled() ||
      !page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_paint, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerFirstPaint,
                      timing.paint_timing->first_paint.value());
}

void ServiceWorkerPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!IsServiceWorkerControlled()) {
    if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
            timing.paint_timing->first_contentful_paint, GetDelegate())) {
      return;
    }

    if (page_load_metrics::IsGoogleSearchResultUrl(GetDelegate().GetUrl())) {
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramNoServiceWorkerFirstContentfulPaintSearch,
          timing.paint_timing->first_contentful_paint.value());
      PAGE_LOAD_HISTOGRAM(
          internal::
              kHistogramNoServiceWorkerParseStartToFirstContentfulPaintSearch,
          timing.paint_timing->first_contentful_paint.value() -
              timing.parse_timing->parse_start.value());
    } else if (IsDocsSite(GetDelegate().GetUrl())) {
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramNoServiceWorkerFirstContentfulPaintDocs,
          timing.paint_timing->first_contentful_paint.value());
    }
    return;
  }
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramServiceWorkerFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value());
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerFirstContentfulPaint,
                      timing.paint_timing->first_contentful_paint.value());
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value() -
          timing.parse_timing->parse_start.value());

  if (IsForwardBackLoad(transition_)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramServiceWorkerFirstContentfulPaintForwardBack,
        timing.paint_timing->first_contentful_paint.value());
    if (was_no_store_main_resource_) {
      PAGE_LOAD_HISTOGRAM(
          internal::
              kHistogramServiceWorkerFirstContentfulPaintForwardBackNoStore,
          timing.paint_timing->first_contentful_paint.value());
    }
  }

  if (page_load_metrics::IsGoogleSearchResultUrl(GetDelegate().GetUrl())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramServiceWorkerFirstContentfulPaintSearch,
        timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramServiceWorkerParseStartToFirstContentfulPaintSearch,
        timing.paint_timing->first_contentful_paint.value() -
            timing.parse_timing->parse_start.value());
  } else if (IsDocsSite(GetDelegate().GetUrl())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramServiceWorkerFirstContentfulPaintDocs,
        timing.paint_timing->first_contentful_paint.value());
  }

  if (IsServiceWorkerFetchHandlerSkippable()) {
    PAGE_LOAD_HISTOGRAM(
        internal::
            kHistogramServiceWorkerFirstContentfulPaintSkippableFetchHandler,
        timing.paint_timing->first_contentful_paint.value());
  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::
            kHistogramServiceWorkerFirstContentfulPaintNonSkippableFetchHandler,
        timing.paint_timing->first_contentful_paint.value());
  }

  if (IsServiceWorkerEligibleForRaceNetworkRequest()) {
    PAGE_LOAD_HISTOGRAM(
        internal::
            kHistogramServiceWorkerFirstContentfulPaintRaceNetworkRequestEligible,
        timing.paint_timing->first_contentful_paint.value());
  }
}

void ServiceWorkerPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->dom_content_loaded_event_start,
          GetDelegate())) {
    return;
  }
  if (!IsServiceWorkerControlled()) {
    if (!page_load_metrics::IsGoogleSearchResultUrl(GetDelegate().GetUrl()))
      return;
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramNoServiceWorkerDomContentLoadedSearch,
        timing.document_timing->dom_content_loaded_event_start.value());
    return;
  }
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramServiceWorkerDomContentLoaded,
      timing.document_timing->dom_content_loaded_event_start.value());
  if (page_load_metrics::IsGoogleSearchResultUrl(GetDelegate().GetUrl())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramServiceWorkerDomContentLoadedSearch,
        timing.document_timing->dom_content_loaded_event_start.value());
  }
}

void ServiceWorkerPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, GetDelegate()))
    return;
  if (!IsServiceWorkerControlled()) {
    if (!page_load_metrics::IsGoogleSearchResultUrl(GetDelegate().GetUrl()))
      return;
    PAGE_LOAD_HISTOGRAM(internal::kHistogramNoServiceWorkerLoadSearch,
                        timing.document_timing->load_event_start.value());
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerLoad,
                      timing.document_timing->load_event_start.value());
  if (page_load_metrics::IsGoogleSearchResultUrl(GetDelegate().GetUrl())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerLoadSearch,
                        timing.document_timing->load_event_start.value());
  }
}

void ServiceWorkerPageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!IsServiceWorkerControlled())
    return;
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_timestamp, GetDelegate())) {
    return;
  }
}

void ServiceWorkerPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!IsServiceWorkerControlled())
    return;

  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_start, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerParseStart,
                        timing.parse_timing->parse_start.value());

    if (page_load_metrics::IsGoogleSearchResultUrl(GetDelegate().GetUrl())) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerParseStartSearch,
                          timing.parse_timing->parse_start.value());
    }
    if (IsForwardBackLoad(transition_)) {
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramServiceWorkerParseStartForwardBack,
          timing.parse_timing->parse_start.value());
      if (was_no_store_main_resource_) {
        PAGE_LOAD_HISTOGRAM(
            internal::kHistogramServiceWorkerParseStartForwardBackNoStore,
            timing.parse_timing->parse_start.value());
      }
    }
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramServiceWorkerParseStart,
                        timing.parse_timing->parse_start.value());
  }
}

void ServiceWorkerPageLoadMetricsObserver::OnLoadingBehaviorObserved(
    content::RenderFrameHost* rfh,
    int behavior_flags) {
  if (!IsServiceWorkerControlled() || logged_ukm_event_)
    return;
  ukm::builders::PageLoad_ServiceWorkerControlled(
      GetDelegate().GetPageUkmSourceId())
      .Record(ukm::UkmRecorder::Get());
  logged_ukm_event_ = true;
}

void ServiceWorkerPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordTimingHistograms();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ServiceWorkerPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  //  This follows UmaPageLoadMetricsObserver.
  if (GetDelegate().DidCommit())
    RecordTimingHistograms();
  return STOP_OBSERVING;
}

void ServiceWorkerPageLoadMetricsObserver::RecordTimingHistograms() {
  if (!IsServiceWorkerControlled())
    return;

  const page_load_metrics::ContentfulPaintTimingInfo&
      all_frames_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .MergeMainFrameAndSubframes();
  if (all_frames_largest_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          all_frames_largest_contentful_paint.Time(), GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerLargestContentfulPaint,
                        all_frames_largest_contentful_paint.Time().value());
    if (IsServiceWorkerFetchHandlerSkippable()) {
      PAGE_LOAD_HISTOGRAM(
          internal::
              kHistogramServiceWorkerLargestContentfulPaintSkippableFetchHandler,
          all_frames_largest_contentful_paint.Time().value());
    } else {
      PAGE_LOAD_HISTOGRAM(
          internal::
              kHistogramServiceWorkerLargestContentfulPaintNonSkippableFetchHandler,
          all_frames_largest_contentful_paint.Time().value());
    }
    if (IsServiceWorkerEligibleForRaceNetworkRequest()) {
      PAGE_LOAD_HISTOGRAM(
          internal::
              kHistogramServiceWorkerLargestContentfulPaintRaceNetworkRequestEligible,
          all_frames_largest_contentful_paint.Time().value());
    }
  }
  RecordSubresourceLoad();
}

bool ServiceWorkerPageLoadMetricsObserver::IsServiceWorkerControlled() {
  return (GetDelegate().GetMainFrameMetadata().behavior_flags &
          blink::LoadingBehaviorFlag::
              kLoadingBehaviorServiceWorkerControlled) != 0;
}

bool ServiceWorkerPageLoadMetricsObserver::
    IsServiceWorkerFetchHandlerSkippable() {
  DCHECK(IsServiceWorkerControlled());
  return (GetDelegate().GetMainFrameMetadata().behavior_flags &
          blink::LoadingBehaviorFlag::
              kLoadingBehaviorServiceWorkerFetchHandlerSkippable) != 0;
}

bool ServiceWorkerPageLoadMetricsObserver::
    IsServiceWorkerEligibleForRaceNetworkRequest() {
  CHECK(IsServiceWorkerControlled());
  return (GetDelegate().GetMainFrameMetadata().behavior_flags &
          blink::LoadingBehaviorFlag::
              kLoadingBehaviorServiceWorkerRaceNetworkRequest);
}

void ServiceWorkerPageLoadMetricsObserver::RecordSubresourceLoad() {
  const auto& optional_metrics = GetDelegate().GetSubresourceLoadMetrics();
  if (!optional_metrics) {
    return;
  }
  auto metrics = *optional_metrics;
  // serviceworker's subresource load must always be smaller than
  // or equals to total subresource loads.
  if (metrics.number_of_subresource_loads_handled_by_service_worker >
      metrics.number_of_subresources_loaded) {
    // If the data is not set or invalid, it should not be worth recording.
    return;
  }

  ServiceWorkerResourceLoadStatus status;
  if (GetDelegate().GetMainFrameMetadata().behavior_flags &
      blink::LoadingBehaviorFlag::
          kLoadingBehaviorServiceWorkerMainResourceFetchFallback) {
    if (metrics.number_of_subresource_loads_handled_by_service_worker == 0) {
      status = ServiceWorkerResourceLoadStatus::
          kMainResourceFallbackAndSubResourceFallback;
    } else if (metrics.number_of_subresources_loaded ==
               metrics.number_of_subresource_loads_handled_by_service_worker) {
      if (metrics.number_of_subresources_loaded == 0) {
        status = ServiceWorkerResourceLoadStatus::
            kMainResourceFallbackAndSubResourceNotFallback;
      } else {
        status = ServiceWorkerResourceLoadStatus::
            kMainResourceFallbackAndNoSubResource;
      }
    } else {
      status = ServiceWorkerResourceLoadStatus::
          kMainResourceFallbackAndSubResourceMixed;
    }
  } else {
    if (metrics.number_of_subresource_loads_handled_by_service_worker == 0) {
      status = ServiceWorkerResourceLoadStatus::
          kMainResourceNotFallbackAndSubResourceFallback;
    } else if (metrics.number_of_subresources_loaded ==
               metrics.number_of_subresource_loads_handled_by_service_worker) {
      if (metrics.number_of_subresources_loaded == 0) {
        status = ServiceWorkerResourceLoadStatus::
            kMainResourceNotFallbackAndSubResourceNotFallback;
      } else {
        status = ServiceWorkerResourceLoadStatus::
            kMainResourceNotFallbackAndNoSubResource;
      }
    } else {
      status = ServiceWorkerResourceLoadStatus::
          kMainResourceNotFallbackAndSubResourceMixed;
    }
  }

  // We calculate the number of fallbacks here.
  uint32_t number_of_fallback =
      metrics.number_of_subresources_loaded -
      metrics.number_of_subresource_loads_handled_by_service_worker;
  int64_t fallback_ratio = -1;
  if (metrics.number_of_subresources_loaded > 0) {
    fallback_ratio =
        100 * number_of_fallback / metrics.number_of_subresources_loaded;
  }

  ukm::builders::ServiceWorker_OnLoad builder(
      GetDelegate().GetPageUkmSourceId());
  builder.SetMainAndSubResourceLoadLocation(static_cast<int64_t>(status))
      .SetTotalSubResourceLoad(ukm::GetExponentialBucketMinForCounts1000(
          metrics.number_of_subresources_loaded))
      .SetTotalSubResourceFallback(
          ukm::GetExponentialBucketMinForCounts1000(number_of_fallback))
      .SetSubResourceFallbackRatio(fallback_ratio);
  if (metrics.service_worker_subresource_load_metrics) {
    const auto& sw_metrics = *metrics.service_worker_subresource_load_metrics;
    builder.SetImageHandled(sw_metrics.image_handled)
        .SetImageFallback(sw_metrics.image_fallback)
        .SetCSSStyleSheetHandled(sw_metrics.css_handled)
        .SetCSSStyleSheetFallback(sw_metrics.css_fallback)
        .SetScriptHandled(sw_metrics.script_handled)
        .SetScriptFallback(sw_metrics.script_fallback)
        .SetFontHandled(sw_metrics.font_handled)
        .SetFontFallback(sw_metrics.font_fallback)
        .SetSVGDocumentHandled(sw_metrics.svg_handled)
        .SetSVGDocumentFallback(sw_metrics.svg_fallback)
        .SetXSLStyleSheetHandled(sw_metrics.xsl_handled)
        .SetXSLStyleSheetFallback(sw_metrics.xsl_fallback)
        .SetLinkPrefetchHandled(sw_metrics.link_prefetch_handled)
        .SetLinkPrefetchFallback(sw_metrics.link_prefetch_fallback)
        .SetTextTrackHandled(sw_metrics.text_track_handled)
        .SetTextTrackFallback(sw_metrics.text_track_fallback)
        .SetAudioHandled(sw_metrics.audio_handled)
        .SetAudioFallback(sw_metrics.audio_fallback)
        .SetVideoHandled(sw_metrics.video_handled)
        .SetVideoFallback(sw_metrics.video_fallback)
        .SetManifestHandled(sw_metrics.manifest_handled)
        .SetManifestFallback(sw_metrics.manifest_fallback)
        .SetSpeculationRulesHandled(sw_metrics.speculation_rules_handled)
        .SetSpeculationRulesFallback(sw_metrics.speculation_rules_fallback)
        .SetDictionaryHandled(sw_metrics.dictionary_handled)
        .SetDictionaryFallback(sw_metrics.dictionary_fallback)
        .SetMatchedCacheRouterSourceCount(
            ukm::GetExponentialBucketMinForCounts1000(
                sw_metrics.matched_cache_router_source_count))
        .SetMatchedFetchEventRouterSourceCount(
            ukm::GetExponentialBucketMinForCounts1000(
                sw_metrics.matched_fetch_event_router_source_count))
        .SetMatchedNetworkRouterSourceCount(
            ukm::GetExponentialBucketMinForCounts1000(
                sw_metrics.matched_network_router_source_count))
        .SetMatchedRaceNetworkAndFetchRouterSourceCount(
            ukm::GetExponentialBucketMinForCounts1000(
                sw_metrics.matched_race_network_and_fetch_router_source_count));

    if (!sw_metrics.total_router_evaluation_time_for_subresources.is_zero()) {
      PAGE_LOAD_SHORT_HISTOGRAM(
          internal::kHistogramServiceWorkerSubresourceTotalRouterEvaluationTime,
          sw_metrics.total_router_evaluation_time_for_subresources);
    }
  }
  builder.Record(ukm::UkmRecorder::Get());
}
