// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"
#include "chrome/browser/browser_process.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"

namespace internal {
const char kHistogramDocWriteBlockFirstContentfulPaint[] =
    "PageLoad.Clients.DocWrite.Block.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramDocWriteBlockParseStartToFirstContentfulPaint[] =
    "PageLoad.Clients.DocWrite.Block.PaintTiming."
    "ParseStartToFirstContentfulPaint";
const char kHistogramDocWriteBlockParseBlockedOnScriptLoad[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming.ParseBlockedOnScriptLoad";

const char kBackgroundHistogramDocWriteBlockParseBlockedOnScriptLoad[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming.ParseBlockedOnScriptLoad."
    "Background";
}  // namespace internal

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DocumentWritePageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class is interested in events that are dispatched only for the primary
  // page or preprocessed by PageLoadTracker to be per-outermost page. So, no
  // need to forward events at the observer layer.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DocumentWritePageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class measures effect of `document.write()` on parsing and FCP.
  // As `document.write()` is strongly discouraged [1], we think it is enough to
  // record non prerendered case and this class doesn't support prerendering.
  //
  // [1]
  // https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#dom-document-write-dev
  return STOP_OBSERVING;
}

void DocumentWritePageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (GetDelegate().GetMainFrameMetadata().behavior_flags &
      blink::LoadingBehaviorFlag::kLoadingBehaviorDocumentWriteBlock) {
    LogDocumentWriteBlockFirstContentfulPaint(timing);
  }
}

void DocumentWritePageLoadMetricsObserver::OnParseStop(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (GetDelegate().GetMainFrameMetadata().behavior_flags &
      blink::LoadingBehaviorFlag::kLoadingBehaviorDocumentWriteBlock) {
    LogDocumentWriteBlockParseStop(timing);
  }
}

void DocumentWritePageLoadMetricsObserver::
    LogDocumentWriteBlockFirstContentfulPaint(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramDocWriteBlockFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value() -
            timing.parse_timing->parse_start.value());
  }
}

void DocumentWritePageLoadMetricsObserver::LogDocumentWriteBlockParseStop(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_stop, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteBlockParseBlockedOnScriptLoad,
        timing.parse_timing->parse_blocked_on_script_load_duration.value());
  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramDocWriteBlockParseBlockedOnScriptLoad,
        timing.parse_timing->parse_blocked_on_script_load_duration.value());
  }
}
