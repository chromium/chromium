// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/signed_exchange_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"

namespace {

const char kAmpProjectCdnHost[] = "cdn.ampproject.org";
const char kWebPkgCacheHost[] = "webpkgcache.com";

bool IsServedFromGoogleCache(content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle->IsSignedExchangeInnerResponse());
  const std::vector<GURL>& redirects = navigation_handle->GetRedirectChain();
  if (redirects.size() <= 1)
    return false;
  const GURL& signed_exchange_outer_url = redirects[redirects.size() - 2];
  // Note: GURL::DomainIs() doesn't check eTLD+1. So for example,
  // GURL("example-com.cdn.ampproject.org").DomainIs("cdn.ampproject.org")
  // returns true.
  return signed_exchange_outer_url.DomainIs(kAmpProjectCdnHost) ||
         signed_exchange_outer_url.DomainIs(kWebPkgCacheHost);
}

}  // namespace

namespace internal {

#define HISTOGRAM_SXG_PREFIX "PageLoad.Clients.SignedExchange."
#define HISTOGRAM_CACHED_SXG_PREFIX "PageLoad.Clients.SignedExchange.Cached."
#define HISTOGRAM_NOTCACHED_SXG_PREFIX \
  "PageLoad.Clients.SignedExchange.NotCached."
#define HISTOGRAM_ALT_SUB_SXG_PREFIX \
  "PageLoad.Clients.SignedExchange.AltSubSXG."

constexpr char kHistogramSignedExchangePrefix[] = HISTOGRAM_SXG_PREFIX;
constexpr char kHistogramCachedSignedExchangePrefix[] =
    HISTOGRAM_CACHED_SXG_PREFIX;
constexpr char kHistogramNotCachedSignedExchangePrefix[] =
    HISTOGRAM_NOTCACHED_SXG_PREFIX;
constexpr char kHistogramAltSubSxgSignedExchangePrefix[] =
    HISTOGRAM_ALT_SUB_SXG_PREFIX;

#define SXG_LOAD_METRIC_VARIABLE(name, suffix)               \
  constexpr char kHistogramSignedExchange##name[] =          \
      HISTOGRAM_SXG_PREFIX suffix;                           \
  constexpr char kHistogramCachedSignedExchange##name[] =    \
      HISTOGRAM_CACHED_SXG_PREFIX suffix;                    \
  constexpr char kHistogramNotCachedSignedExchange##name[] = \
      HISTOGRAM_NOTCACHED_SXG_PREFIX suffix;                 \
  constexpr char kHistogramAltSubSxgSignedExchange##name[] = \
      HISTOGRAM_ALT_SUB_SXG_PREFIX suffix;

SXG_LOAD_METRIC_VARIABLE(ParseStart, "ParseTiming.NavigationToParseStart")
SXG_LOAD_METRIC_VARIABLE(FirstInputDelay, "InteractiveTiming.FirstInputDelay3")
SXG_LOAD_METRIC_VARIABLE(FirstPaint, "PaintTiming.NavigationToFirstPaint")
SXG_LOAD_METRIC_VARIABLE(FirstContentfulPaint,
                         "PaintTiming.NavigationToFirstContentfulPaint")
SXG_LOAD_METRIC_VARIABLE(ParseStartToFirstContentfulPaint,
                         "PaintTiming.ParseStartToFirstContentfulPaint")
SXG_LOAD_METRIC_VARIABLE(
    FirstMeaningfulPaint,
    "Experimental.PaintTiming.NavigationToFirstMeaningfulPaint")
SXG_LOAD_METRIC_VARIABLE(
    ParseStartToFirstMeaningfulPaint,
    "Experimental.PaintTiming.ParseStartToFirstMeaningfulPaint")
SXG_LOAD_METRIC_VARIABLE(
    DomContentLoaded,
    "DocumentTiming.NavigationToDOMContentLoadedEventFired")
SXG_LOAD_METRIC_VARIABLE(Load, "DocumentTiming.NavigationToLoadEventFired")

#define SXG_PAGE_LOAD_HISTOGRAM(name, sample)                                \
  {                                                                          \
    const base::TimeDelta value = sample;                                    \
    PAGE_LOAD_HISTOGRAM(internal::kHistogramSignedExchange##name, value);    \
    if (was_cached_) {                                                       \
      PAGE_LOAD_HISTOGRAM(internal::kHistogramCachedSignedExchange##name,    \
                          value);                                            \
    } else {                                                                 \
      PAGE_LOAD_HISTOGRAM(internal::kHistogramNotCachedSignedExchange##name, \
                          value);                                            \
    }                                                                        \
    if (had_prefetched_alt_sxg_) {                                           \
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAltSubSxgSignedExchange##name, \
                          value);                                            \
    }                                                                        \
  }

#undef SXG_LOAD_METRIC_VARIABLE
#undef HISTOGRAM_ALT_SUB_SXG_PREFIX
#undef HISTOGRAM_CACHED_SXG_PREFIX
#undef HISTOGRAM_NOTCACHED_SXG_PREFIX
#undef HISTOGRAM_SXG_PREFIX

}  // namespace internal

SignedExchangePageLoadMetricsObserver::SignedExchangePageLoadMetricsObserver() {
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SignedExchangePageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  if (navigation_handle->IsSignedExchangeInnerResponse()) {
    ukm::builders::PageLoad_SignedExchange builder(source_id);
    if (IsServedFromGoogleCache(navigation_handle))
      builder.SetServedFromGoogleCache(1);
    builder.Record(ukm::UkmRecorder::Get());

    was_cached_ = navigation_handle->WasResponseCached();
    had_prefetched_alt_sxg_ =
        navigation_handle->HasPrefetchedAlternativeSubresourceSignedExchange();
    return CONTINUE_OBSERVING;
  }

  return STOP_OBSERVING;
}

void SignedExchangePageLoadMetricsObserver::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_paint, GetDelegate())) {
    return;
  }

  SXG_PAGE_LOAD_HISTOGRAM(FirstPaint, timing.paint_timing->first_paint.value());
}

void SignedExchangePageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    return;
  }

  SXG_PAGE_LOAD_HISTOGRAM(FirstContentfulPaint,
                          timing.paint_timing->first_contentful_paint.value());
  SXG_PAGE_LOAD_HISTOGRAM(ParseStartToFirstContentfulPaint,
                          timing.paint_timing->first_contentful_paint.value() -
                              timing.parse_timing->parse_start.value());
}

void SignedExchangePageLoadMetricsObserver::
    OnFirstMeaningfulPaintInMainFrameDocument(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, GetDelegate())) {
    return;
  }

  SXG_PAGE_LOAD_HISTOGRAM(FirstMeaningfulPaint,
                          timing.paint_timing->first_meaningful_paint.value());
  SXG_PAGE_LOAD_HISTOGRAM(ParseStartToFirstMeaningfulPaint,
                          timing.paint_timing->first_meaningful_paint.value() -
                              timing.parse_timing->parse_start.value());
}

void SignedExchangePageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->dom_content_loaded_event_start,
          GetDelegate())) {
    return;
  }

  SXG_PAGE_LOAD_HISTOGRAM(
      DomContentLoaded,
      timing.document_timing->dom_content_loaded_event_start.value());
}

void SignedExchangePageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, GetDelegate())) {
    return;
  }

  SXG_PAGE_LOAD_HISTOGRAM(Load,
                          timing.document_timing->load_event_start.value());
}

void SignedExchangePageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_timestamp, GetDelegate())) {
    return;
  }

  // Copied from the UmaPageLoadMetricsObserver implementation.
  UMA_HISTOGRAM_CUSTOM_TIMES(
      internal::kHistogramSignedExchangeFirstInputDelay,
      timing.interactive_timing->first_input_delay.value(),
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(60),
      50);
  if (was_cached_) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        internal::kHistogramCachedSignedExchangeFirstInputDelay,
        timing.interactive_timing->first_input_delay.value(),
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(60),
        50);
  } else {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        internal::kHistogramNotCachedSignedExchangeFirstInputDelay,
        timing.interactive_timing->first_input_delay.value(),
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(60),
        50);
  }
  if (had_prefetched_alt_sxg_) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        internal::kHistogramAltSubSxgSignedExchangeFirstInputDelay,
        timing.interactive_timing->first_input_delay.value(),
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(60),
        50);
  }
}

void SignedExchangePageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_start, GetDelegate())) {
    return;
  }

  SXG_PAGE_LOAD_HISTOGRAM(ParseStart, timing.parse_timing->parse_start.value());
}
