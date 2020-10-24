// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PORTAL_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PORTAL_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "services/metrics/public/cpp/ukm_source.h"

class PortalPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // Returns a UkmPageLoadMetricsObserver, or nullptr if it is not needed.
  static std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
  CreateIfNeeded();

  PortalPageLoadMetricsObserver();
  PortalPageLoadMetricsObserver(const PortalPageLoadMetricsObserver&) = delete;
  PortalPageLoadMetricsObserver operator=(
      const PortalPageLoadMetricsObserver&) = delete;
  ~PortalPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;

  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void DidActivatePortal(base::TimeTicks activation_time) override;

 private:
  // Records timing related metrics available in Portal.Activate event.
  void RecordTimingMetrics(
      const page_load_metrics::mojom::PageLoadTiming& timing);

  // Reports the time that a portal painted after being activated.
  void ReportPortalActivatedPaint(
      const base::Optional<base::TimeTicks>& portal_activated_paint);

  // Navigation start time in wall time.
  base::TimeTicks navigation_start_;

  // Time that the portal host requested that the portal should be activated.
  base::Optional<base::TimeTicks> portal_activation_time_;

  // Time between portal actithat the portal has painted after a portal
  // activation.
  base::Optional<base::TimeTicks> portal_paint_time_;

  // True if the page started hidden, or ever became hidden.
  bool was_hidden_ = false;

  // True if this page was loaded in a portal and never activated.
  bool is_portal_ = false;

  // The connection info for the committed URL.
  base::Optional<net::HttpResponseInfo::ConnectionInfo> connection_info_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PORTAL_PAGE_LOAD_METRICS_OBSERVER_H_
