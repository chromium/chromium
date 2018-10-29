// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

namespace content {
class BrowserContext;
class NavigationHandle;
}

namespace data_reduction_proxy {
class DataReductionProxyData;
class DataReductionProxyPingbackClient;

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

// Observer responsible for recording core page load metrics releveant to
// DataReductionProxy.
class DataReductionProxyMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  DataReductionProxyMetricsObserver();
  ~DataReductionProxyMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnComplete(const page_load_metrics::mojom::PageLoadTiming& timing,
                  const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstLayout(const page_load_metrics::mojom::PageLoadTiming& timing,
                     const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstTextPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstImagePaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnParseStart(const page_load_metrics::mojom::PageLoadTiming& timing,
                    const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnParseStop(const page_load_metrics::mojom::PageLoadTiming& timing,
                   const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_compelte_info) override;
  void OnEventOccurred(const void* const event_key) override;
  void OnUserInput(const blink::WebInputEvent& event) override;

  // Exponentially bucket the number of bytes for privacy-implicated resources.
  // Input below 10KB returns 0.
  static int64_t ExponentiallyBucketBytes(int64_t bytes);

 private:
  // Sends the page load information to the pingback client.
  void SendPingback(const page_load_metrics::mojom::PageLoadTiming& timing,
                    const page_load_metrics::PageLoadExtraInfo& info,
                    bool app_background_occurred);

  // Records UMA of page size when the observer is about to be deleted.
  void RecordPageSizeUMA() const;

  // Gets the default DataReductionProxyPingbackClient. Overridden in testing.
  virtual DataReductionProxyPingbackClient* GetPingbackClient() const;

  // Used as a callback to getting a memory dump of the related renderer
  // process.
  void ProcessMemoryDump(
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> memory_dump);

  // Gets the memory coordinator for Chrome. Virtual for testing.
  virtual void RequestProcessDump(
      base::ProcessId pid,
      memory_instrumentation::MemoryInstrumentation::RequestGlobalDumpCallback
          callback);

  // Data related to this navigation.
  std::unique_ptr<DataReductionProxyData> data_;

  // The browser context this navigation is operating in.
  content::BrowserContext* browser_context_;

  // True if a Preview opt out occurred during this page load.
  bool opted_out_;

  // The number of resources that used data reduction proxy.
  int num_data_reduction_proxy_resources_;

  // The number of resources that did not come from cache.
  int num_network_resources_;

  // The total content network bytes that the user would have downloaded if they
  // were not using data reduction proxy for HTTP resources.
  int64_t insecure_original_network_bytes_;

  // The total content network bytes that the user would have downloaded if they
  // were not using data reduction proxy for HTTPS resources.
  int64_t secure_original_network_bytes_;

  // The total network bytes loaded through data reduction proxy. This value
  // only concerns HTTP traffic.
  int64_t network_bytes_proxied_;

  // The total network bytes used for HTTP resources.
  int64_t insecure_network_bytes_;

  // The total network bytes used for HTTPS resources.
  int64_t secure_network_bytes_;

  // The total cached bytes used for HTTP resources.
  int64_t insecure_cached_bytes_;

  // The total cached bytes used for HTTPS resources.
  int64_t secure_cached_bytes_;

  // The process ID of the main frame renderer during OnCommit.
  base::ProcessId process_id_;

  // The memory usage of the main frame renderer shortly after OnLoadEventStart.
  // Available after ProcessMemoryDump is called. 0 before that point.
  int64_t renderer_memory_usage_kb_;

  // A unique identifier to the child process of the render frame, stored in
  // case of a renderer crash.
  // Set at navigation commit time.
  int render_process_host_id_;

  // The number of touch events on the page.
  uint32_t touch_count_;

  // The number of scroll events on the page.
  uint32_t scroll_count_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DataReductionProxyMetricsObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyMetricsObserver);
};

}  // namespace data_reduction_proxy

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_H_
