// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer_base.h"

namespace data_reduction_proxy {

namespace internal {

// Various UMA histogram names for DataReductionProxy core page load metrics.
extern const char kHistogramDataReductionProxyPrefix[];
extern const char kHistogramDataReductionProxyLitePagePrefix[];

// Byte and request specific histogram suffixes.
extern const char kResourcesPercentProxied[];
extern const char kBytesPercentProxied[];
extern const char kBytesCompressionRatio[];
extern const char kBytesInflationPercent[];
extern const char kNetworkResources[];
extern const char kResourcesProxied[];
extern const char kResourcesNotProxied[];
extern const char kNetworkBytes[];
extern const char kBytesProxied[];
extern const char kBytesNotProxied[];
extern const char kBytesOriginal[];
extern const char kBytesSavings[];
extern const char kBytesInflation[];

}  // namespace internal

// Observer responsible for recording core page load metrics histograms relevant
// to DataReductionProxy.
class DataReductionProxyMetricsObserver
    : public DataReductionProxyMetricsObserverBase {
 public:
  DataReductionProxyMetricsObserver();
  ~DataReductionProxyMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstLayout(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstImagePaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnParseStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnParseStop(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  // Records UMA of page size when the observer is about to be deleted.
  void RecordPageSizeUMA() const;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyMetricsObserver);
};

}  // namespace data_reduction_proxy

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_H_
