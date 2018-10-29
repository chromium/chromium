// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/protocol_page_load_metrics_observer.h"

#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ProtocolPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return started_in_foreground ? CONTINUE_OBSERVING : STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ProtocolPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  connection_info_ = navigation_handle->GetConnectionInfo();
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ProtocolPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  return STOP_OBSERVING;
}

void ProtocolPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  switch (connection_info_) {
    case net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY2:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY3:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_14:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_15:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP0_9:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_0:
    case net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS:
      return;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H11.ParseTiming.NavigationToParseStart",
          timing.parse_timing->parse_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP2:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H2.ParseTiming.NavigationToParseStart",
          timing.parse_timing->parse_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_UNKNOWN_VERSION:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_32:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_33:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_34:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_35:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_36:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_37:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_38:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_39:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_40:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_41:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_42:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_43:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_44:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_45:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_46:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_99:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.QUIC.ParseTiming.NavigationToParseStart",
          timing.parse_timing->parse_start.value());
      break;
  }
}

void ProtocolPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  switch (connection_info_) {
    case net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY2:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY3:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_14:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_15:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP0_9:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_0:
    case net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS:
      return;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H11.PaintTiming."
          "NavigationToFirstContentfulPaint",
          timing.paint_timing->first_contentful_paint.value());
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H11.PaintTiming."
          "ParseStartToFirstContentfulPaint",
          timing.paint_timing->first_contentful_paint.value() -
              timing.parse_timing->parse_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP2:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H2.PaintTiming."
          "NavigationToFirstContentfulPaint",
          timing.paint_timing->first_contentful_paint.value());
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H2.PaintTiming."
          "ParseStartToFirstContentfulPaint",
          timing.paint_timing->first_contentful_paint.value() -
              timing.parse_timing->parse_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_UNKNOWN_VERSION:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_32:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_33:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_34:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_35:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_36:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_37:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_38:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_39:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_40:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_41:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_42:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_43:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_44:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_45:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_46:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_99:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.QUIC.PaintTiming."
          "NavigationToFirstContentfulPaint",
          timing.paint_timing->first_contentful_paint.value());
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.QUIC.PaintTiming."
          "ParseStartToFirstContentfulPaint",
          timing.paint_timing->first_contentful_paint.value() -
              timing.parse_timing->parse_start.value());
      break;
  }
}

void ProtocolPageLoadMetricsObserver::OnFirstMeaningfulPaintInMainFrameDocument(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  switch (connection_info_) {
    case net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY2:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY3:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_14:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_15:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP0_9:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_0:
    case net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS:
      return;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H11.Experimental.PaintTiming."
          "NavigationToFirstMeaningfulPaint",
          timing.paint_timing->first_meaningful_paint.value());
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H11.Experimental.PaintTiming."
          "ParseStartToFirstMeaningfulPaint",
          timing.paint_timing->first_meaningful_paint.value() -
              timing.parse_timing->parse_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP2:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H2.Experimental.PaintTiming."
          "NavigationToFirstMeaningfulPaint",
          timing.paint_timing->first_meaningful_paint.value());
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H2.Experimental.PaintTiming."
          "ParseStartToFirstMeaningfulPaint",
          timing.paint_timing->first_meaningful_paint.value() -
              timing.parse_timing->parse_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_UNKNOWN_VERSION:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_32:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_33:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_34:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_35:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_36:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_37:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_38:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_39:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_40:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_41:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_42:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_43:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_44:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_45:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_46:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_99:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.QUIC.Experimental.PaintTiming."
          "NavigationToFirstMeaningfulPaint",
          timing.paint_timing->first_meaningful_paint.value());
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.QUIC.Experimental.PaintTiming."
          "ParseStartToFirstMeaningfulPaint",
          timing.paint_timing->first_meaningful_paint.value() -
              timing.parse_timing->parse_start.value());
      break;
  }
}

void ProtocolPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  switch (connection_info_) {
    case net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY2:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY3:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_14:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_15:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP0_9:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_0:
    case net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS:
      return;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H11.DocumentTiming."
          "NavigationToDOMContentLoadedEventFired",
          timing.document_timing->dom_content_loaded_event_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP2:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H2.DocumentTiming."
          "NavigationToDOMContentLoadedEventFired",
          timing.document_timing->dom_content_loaded_event_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_UNKNOWN_VERSION:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_32:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_33:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_34:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_35:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_36:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_37:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_38:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_39:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_40:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_41:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_42:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_43:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_44:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_45:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_46:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_99:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.QUIC.DocumentTiming."
          "NavigationToDOMContentLoadedEventFired",
          timing.document_timing->dom_content_loaded_event_start.value());
      break;
  }
}

void ProtocolPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  switch (connection_info_) {
    case net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY2:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY3:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_14:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_15:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP0_9:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_0:
    case net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS:
      return;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H11.DocumentTiming."
          "NavigationToLoadEventFired",
          timing.document_timing->load_event_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP2:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H2.DocumentTiming."
          "NavigationToLoadEventFired",
          timing.document_timing->load_event_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_UNKNOWN_VERSION:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_32:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_33:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_34:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_35:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_36:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_37:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_38:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_39:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_40:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_41:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_42:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_43:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_44:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_45:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_46:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_99:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.QUIC.DocumentTiming."
          "NavigationToLoadEventFired",
          timing.document_timing->load_event_start.value());
      break;
  }
}
