// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/protocol_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"

namespace {

#define PROTOCOL_HISTOGRAM(name, protocol, sample)                         \
  switch (protocol) {                                                      \
    case page_load_metrics::NetworkProtocol::kHttp11:                      \
      PAGE_LOAD_HISTOGRAM("PageLoad.Clients.Protocol.H11." name, sample);  \
      break;                                                               \
    case page_load_metrics::NetworkProtocol::kHttp2:                       \
      PAGE_LOAD_HISTOGRAM("PageLoad.Clients.Protocol.H2." name, sample);   \
      break;                                                               \
    case page_load_metrics::NetworkProtocol::kQuic:                        \
      PAGE_LOAD_HISTOGRAM("PageLoad.Clients.Protocol.QUIC." name, sample); \
      break;                                                               \
    case page_load_metrics::NetworkProtocol::kOther:                       \
      break;                                                               \
  }

}  // namespace

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ProtocolPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return started_in_foreground ? CONTINUE_OBSERVING : STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ProtocolPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // All observing events are preprocessed by PageLoadTracker so that the
  // outermost page's observer instance sees gathered information. So, the
  // instance for FencedFrames doesn't need to do anything.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ProtocolPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This observer is interested in comparing performance among several
  // protocols. Including prerendering cases can be another factor to
  // differentiate performance, and it will be a noise for the original goal.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ProtocolPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  protocol_ = page_load_metrics::GetNetworkProtocol(
      navigation_handle->GetConnectionInfo());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ProtocolPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  return STOP_OBSERVING;
}

void ProtocolPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  PROTOCOL_HISTOGRAM("ParseTiming.NavigationToParseStart", protocol_,
                     timing.parse_timing->parse_start.value());
}

void ProtocolPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  PROTOCOL_HISTOGRAM("PaintTiming.NavigationToFirstContentfulPaint", protocol_,
                     timing.paint_timing->first_contentful_paint.value());
  PROTOCOL_HISTOGRAM("PaintTiming.ParseStartToFirstContentfulPaint", protocol_,
                     timing.paint_timing->first_contentful_paint.value() -
                         timing.parse_timing->parse_start.value());
}

void ProtocolPageLoadMetricsObserver::OnFirstMeaningfulPaintInMainFrameDocument(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  PROTOCOL_HISTOGRAM(
      "Experimental.PaintTiming.NavigationToFirstMeaningfulPaint", protocol_,
      timing.paint_timing->first_meaningful_paint.value());
}

void ProtocolPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  PROTOCOL_HISTOGRAM(
      "DocumentTiming.NavigationToDOMContentLoadedEventFired", protocol_,
      timing.document_timing->dom_content_loaded_event_start.value());
}

void ProtocolPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  PROTOCOL_HISTOGRAM("DocumentTiming.NavigationToLoadEventFired", protocol_,
                     timing.document_timing->load_event_start.value());
}
