// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SIGNED_EXCHANGE_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SIGNED_EXCHANGE_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {

// Expose metrics for tests.
extern const char kHistogramSignedExchangePrefix[];
extern const char kHistogramSignedExchangeParseStart[];
extern const char kHistogramSignedExchangeFirstInputDelay[];
extern const char kHistogramSignedExchangeFirstPaint[];
extern const char kHistogramSignedExchangeFirstContentfulPaint[];
extern const char kHistogramSignedExchangeParseStartToFirstContentfulPaint[];
extern const char kHistogramSignedExchangeFirstMeaningfulPaint[];
extern const char kHistogramSignedExchangeDomContentLoaded[];
extern const char kHistogramSignedExchangeLoad[];

extern const char kHistogramCachedSignedExchangePrefix[];
extern const char kHistogramCachedSignedExchangeParseStart[];
extern const char kHistogramCachedSignedExchangeFirstInputDelay[];
extern const char kHistogramCachedSignedExchangeFirstPaint[];
extern const char kHistogramCachedSignedExchangeFirstContentfulPaint[];
extern const char
    kHistogramCachedSignedExchangeParseStartToFirstContentfulPaint[];
extern const char kHistogramCachedSignedExchangeFirstMeaningfulPaint[];
extern const char kHistogramCachedSignedExchangeDomContentLoaded[];
extern const char kHistogramCachedSignedExchangeLoad[];

extern const char kHistogramNotCachedSignedExchangePrefix[];
extern const char kHistogramNotCachedSignedExchangeParseStart[];
extern const char kHistogramNotCachedSignedExchangeFirstInputDelay[];
extern const char kHistogramNotCachedSignedExchangeFirstPaint[];
extern const char kHistogramNotCachedSignedExchangeFirstContentfulPaint[];
extern const char
    kHistogramNotCachedSignedExchangeParseStartToFirstContentfulPaint[];
extern const char kHistogramNotCachedSignedExchangeFirstMeaningfulPaint[];
extern const char kHistogramNotCachedSignedExchangeDomContentLoaded[];
extern const char kHistogramNotCachedSignedExchangeLoad[];

extern const char kHistogramAltSubSxgSignedExchangePrefix[];
extern const char kHistogramAltSubSxgSignedExchangeParseStart[];
extern const char kHistogramAltSubSxgSignedExchangeFirstInputDelay[];
extern const char kHistogramAltSubSxgSignedExchangeFirstPaint[];
extern const char kHistogramAltSubSxgSignedExchangeFirstContentfulPaint[];
extern const char
    kHistogramAltSubSxgSignedExchangeParseStartToFirstContentfulPaint[];
extern const char kHistogramAltSubSxgSignedExchangeFirstMeaningfulPaint[];
extern const char kHistogramAltSubSxgSignedExchangeDomContentLoaded[];
extern const char kHistogramAltSubSxgSignedExchangeLoad[];

}  // namespace internal

class SignedExchangePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  SignedExchangePageLoadMetricsObserver();

  SignedExchangePageLoadMetricsObserver(
      const SignedExchangePageLoadMetricsObserver&) = delete;
  SignedExchangePageLoadMetricsObserver& operator=(
      const SignedExchangePageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnFirstInputInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnParseStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  // True iff the page main resource was served from disk cache.
  bool was_cached_ = false;

  // True iff prefetched alternative signed exchange was sent to the renderer
  // process.
  bool had_prefetched_alt_sxg_ = false;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SIGNED_EXCHANGE_PAGE_LOAD_METRICS_OBSERVER_H_
