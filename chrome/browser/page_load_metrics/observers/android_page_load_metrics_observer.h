// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_ANDROID_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_ANDROID_PAGE_LOAD_METRICS_OBSERVER_H_

#include <jni.h>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "net/nqe/effective_connection_type.h"

namespace network {
class NetworkQualityTracker;
}

class GURL;

/** Forwards page load metrics to the Java side on Android. */
class AndroidPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  AndroidPageLoadMetricsObserver();

  AndroidPageLoadMetricsObserver(const AndroidPageLoadMetricsObserver&) =
      delete;
  AndroidPageLoadMetricsObserver& operator=(
      const AndroidPageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;

  // PageLoadMetricsObserver lifecycle callbacks
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
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

 protected:
  AndroidPageLoadMetricsObserver(
      network::NetworkQualityTracker* network_quality_tracker)
      : network_quality_tracker_(network_quality_tracker) {}

  virtual void ReportNewNavigation(int64_t navigation_id);
  virtual void ReportActivation(int64_t activating_navigation_id,
                                base::TimeTicks activation_start_tick);

  virtual void ReportBufferedMetrics(
      const page_load_metrics::mojom::PageLoadTiming& timing);

  virtual void ReportNetworkQualityEstimate(
      net::EffectiveConnectionType connection_type,
      int64_t http_rtt_ms,
      int64_t transport_rtt_ms);

  virtual void ReportFirstContentfulPaint(
      base::TimeTicks navigation_start_tick,
      base::TimeDelta first_contentful_paint);

  virtual void ReportFirstMeaningfulPaint(
      base::TimeTicks navigation_start_tick,
      base::TimeDelta first_meaningful_paint);

  virtual void ReportLoadEventStart(base::TimeTicks navigation_start_tick,
                                    base::TimeDelta load_event_start);

  virtual void ReportLoadedMainResource(int64_t dns_start_ms,
                                        int64_t dns_end_ms,
                                        int64_t connect_start_ms,
                                        int64_t connect_end_ms,
                                        int64_t request_start_ms,
                                        int64_t send_start_ms,
                                        int64_t send_end_ms);

  virtual void ReportFirstInputDelay(base::TimeDelta first_input_delay);

 private:
  bool IsPrerendering();

  bool did_dispatch_on_main_resource_ = false;
  bool reported_buffered_metrics_ = false;
  int64_t navigation_id_ = -1;

  raw_ptr<network::NetworkQualityTracker> network_quality_tracker_ = nullptr;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_ANDROID_PAGE_LOAD_METRICS_OBSERVER_H_
