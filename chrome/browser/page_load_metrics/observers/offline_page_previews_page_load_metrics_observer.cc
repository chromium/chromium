// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/offline_page_previews_page_load_metrics_observer.h"

#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

namespace previews {

namespace internal {

const char kHistogramOfflinePreviewsDOMContentLoadedEventFired[] =
    "PageLoad.Clients.Previews.OfflinePages.DocumentTiming."
    "NavigationToDOMContentLoadedEventFired";
const char kHistogramOfflinePreviewsFirstLayout[] =
    "PageLoad.Clients.Previews.OfflinePages.DocumentTiming."
    "NavigationToFirstLayout";
const char kHistogramOfflinePreviewsLoadEventFired[] =
    "PageLoad.Clients.Previews.OfflinePages.DocumentTiming."
    "NavigationToLoadEventFired";
const char kHistogramOfflinePreviewsFirstContentfulPaint[] =
    "PageLoad.Clients.Previews.OfflinePages.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramOfflinePreviewsParseStart[] =
    "PageLoad.Clients.Previews.OfflinePages.ParseTiming.NavigationToParseStart";
const char kHistogramOfflinePreviewsPageEndReason[] =
    "Previews.PageEndReason.Offline";

}  // namespace internal

OfflinePagePreviewsPageLoadMetricsObserver::
    OfflinePagePreviewsPageLoadMetricsObserver() {}

OfflinePagePreviewsPageLoadMetricsObserver::
    ~OfflinePagePreviewsPageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
OfflinePagePreviewsPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  return IsOfflinePreview(navigation_handle->GetWebContents())
             ? CONTINUE_OBSERVING
             : STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
OfflinePagePreviewsPageLoadMetricsObserver::ShouldObserveMimeType(
    const std::string& mime_type) const {
  // On top of base-supported types, support MHTML. Offline previews are served
  // as MHTML (multipart/related).
  return PageLoadMetricsObserver::ShouldObserveMimeType(mime_type) ==
                     CONTINUE_OBSERVING ||
                 mime_type == "multipart/related"
             ? CONTINUE_OBSERVING
             : STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
OfflinePagePreviewsPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordPageLoadMetrics();
  return STOP_OBSERVING;
}

void OfflinePagePreviewsPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordPageLoadMetrics();
}

void OfflinePagePreviewsPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->dom_content_loaded_event_start,
          GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramOfflinePreviewsDOMContentLoadedEventFired,
      timing.document_timing->dom_content_loaded_event_start.value());
}

void OfflinePagePreviewsPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramOfflinePreviewsLoadEventFired,
                      timing.document_timing->load_event_start.value());
}

void OfflinePagePreviewsPageLoadMetricsObserver::OnFirstLayout(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->first_layout, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramOfflinePreviewsFirstLayout,
                      timing.document_timing->first_layout.value());
}

void OfflinePagePreviewsPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramOfflinePreviewsFirstContentfulPaint,
                      timing.paint_timing->first_contentful_paint.value());
}

void OfflinePagePreviewsPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_start, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramOfflinePreviewsParseStart,
                      timing.parse_timing->parse_start.value());
}

bool OfflinePagePreviewsPageLoadMetricsObserver::IsOfflinePreview(
    content::WebContents* web_contents) const {
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflinePageTabHelper* tab_helper =
      offline_pages::OfflinePageTabHelper::FromWebContents(web_contents);
  return tab_helper && tab_helper->GetOfflinePreviewItem();
#else
  return false;
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
}

void OfflinePagePreviewsPageLoadMetricsObserver::RecordPageLoadMetrics() {
  UMA_HISTOGRAM_ENUMERATION(
      internal::kHistogramOfflinePreviewsPageEndReason,
      GetDelegate().GetPageEndReason(),
      page_load_metrics::PageEndReason::PAGE_END_REASON_COUNT);
}

}  // namespace previews
