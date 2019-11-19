// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_ANDROID_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_ANDROID_PAGE_LOAD_METRICS_OBSERVER_H_

#include <jni.h>

#include "base/macros.h"
#include "components/page_load_metrics/browser/observers/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace network {
class NetworkQualityTracker;
}

class GURL;

/** Forwards page load metrics to the Java side on Android. */
class AndroidPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  AndroidPageLoadMetricsObserver();

  // page_load_metrics::PageLoadMetricsObserver:
  // PageLoadMetricsObserver lifecycle callbacks
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  // PageLoadMetricsObserver event callbacks
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstInputInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_complete_info) override;
  void OnTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 protected:
  AndroidPageLoadMetricsObserver(
      network::NetworkQualityTracker* network_quality_tracker)
      : network_quality_tracker_(network_quality_tracker) {}

  virtual void ReportNewNavigation();

  virtual void ReportBufferedMetrics(
      const page_load_metrics::mojom::PageLoadTiming& timing);

  virtual void ReportNetworkQualityEstimate(
      net::EffectiveConnectionType connection_type,
      int64_t http_rtt_ms,
      int64_t transport_rtt_ms);

  virtual void ReportFirstContentfulPaint(int64_t navigation_start_tick,
                                          int64_t first_contentful_paint_ms);

  virtual void ReportFirstMeaningfulPaint(int64_t navigation_start_tick,
                                          int64_t first_meaningful_paint_ms);

  virtual void ReportLoadEventStart(int64_t navigation_start_tick,
                                    int64_t load_event_start_ms);

  virtual void ReportLoadedMainResource(int64_t dns_start_ms,
                                        int64_t dns_end_ms,
                                        int64_t connect_start_ms,
                                        int64_t connect_end_ms,
                                        int64_t request_start_ms,
                                        int64_t send_start_ms,
                                        int64_t send_end_ms);

  virtual void ReportFirstInputDelay(int64_t first_input_delay_ms);

 private:
  bool did_dispatch_on_main_resource_ = false;
  bool reported_buffered_metrics_ = false;
  int64_t navigation_id_ = -1;

  network::NetworkQualityTracker* network_quality_tracker_ = nullptr;

  page_load_metrics::LargestContentfulPaintHandler
      largest_contentful_paint_handler_;

  DISALLOW_COPY_AND_ASSIGN(AndroidPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_ANDROID_PAGE_LOAD_METRICS_OBSERVER_H_
