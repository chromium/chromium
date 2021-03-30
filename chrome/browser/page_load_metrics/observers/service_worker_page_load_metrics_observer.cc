// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/service_worker_page_load_metrics_observer.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/observers/from_gws_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "net/http/http_response_headers.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"

namespace internal {

const char kHistogramServiceWorkerPageTransition[] =
    "PageLoad.Clients.ServiceWorker2.PageTransition";

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
const char kHistogramServiceWorkerFirstInputDelay[] =
    "PageLoad.Clients.ServiceWorker2.InteractiveTiming.FirstInputDelay3";
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

}  // namespace internal

namespace {

bool IsDocsSite(const GURL& url) {
  return url.host_piece() == "docs.google.com";
}

bool IsForwardBackLoad(ui::PageTransition transition) {
  return transition & ui::PAGE_TRANSITION_FORWARD_BACK;
}

}  // namespace

ServiceWorkerPageLoadMetricsObserver::ServiceWorkerPageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ServiceWorkerPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
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

  // Copied from the UmaPageLoadMetricsObserver implementation.
  UMA_HISTOGRAM_CUSTOM_TIMES(
      internal::kHistogramServiceWorkerFirstInputDelay,
      timing.interactive_timing->first_input_delay.value(),
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(60),
      50);
}

void ServiceWorkerPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!IsServiceWorkerControlled())
    return;

  // TODO(falken): It may be cleaner to record page transition in OnCommit() but
  // at that point we don't yet know if the page is controlled by a service
  // worker. It should be possible to plumb the information there since the
  // browser process already sends the controller service worker in the
  // navigation commit IPC.
  UMA_HISTOGRAM_ENUMERATION(
      internal::kHistogramServiceWorkerPageTransition,
      static_cast<int>(ui::PageTransitionStripQualifier(transition_)),
      static_cast<int>(ui::PAGE_TRANSITION_LAST_CORE) + 1);

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
  }
}

bool ServiceWorkerPageLoadMetricsObserver::IsServiceWorkerControlled() {
  return (GetDelegate().GetMainFrameMetadata().behavior_flags &
          blink::LoadingBehaviorFlag::
              kLoadingBehaviorServiceWorkerControlled) != 0;
}
