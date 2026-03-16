// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_TOP_CHROME_WEBUI_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_TOP_CHROME_WEBUI_METRICS_OBSERVER_H_

#include <string>

#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}

// Records generic performance metrics for TopChrome UI. These metrics are
// designed to be comparable between WebUI and native implementations, e.g.
// TopChromeUI.OmniboxPopup.RequestToFirstContentfulPaint.
// Any TopChrome UI that intends to use this observer must call from both the
// Views and WebUI implementations if they both exist.
//
// This class is different from NonTabPageLoadMetricsObserver which only records
// metrics for all non-tab WebUI, and the metrics names it records are not
// View-compatible.
class TopChromeWebUIMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  explicit TopChromeWebUIMetricsObserver(std::string webui_name);

  TopChromeWebUIMetricsObserver(const TopChromeWebUIMetricsObserver&) = delete;
  TopChromeWebUIMetricsObserver& operator=(
      const TopChromeWebUIMetricsObserver&) = delete;

  ~TopChromeWebUIMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy OnPrerenderStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy ShouldObserveScheme(
      const GURL& url) const override;

  // Emits the RequestToFirstContentfulPaint static histogram for native UIs.
  static void RecordFirstContentfulPaint(const std::string& webui_name,
                                         base::TimeDelta duration);

 private:
  const std::string webui_name_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_TOP_CHROME_WEBUI_METRICS_OBSERVER_H_
