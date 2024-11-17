// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CHROME_GWS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CHROME_GWS_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/google/browser/gws_page_load_metrics_observer.h"

// Similar to GWSPageLoadMetricsObserver but adds suffixes that are only
// available on //chrome (e.g. FromNewTabPage).
class ChromeGWSPageLoadMetricsObserver : public GWSPageLoadMetricsObserver {
 public:
  ChromeGWSPageLoadMetricsObserver();

  ChromeGWSPageLoadMetricsObserver(const ChromeGWSPageLoadMetricsObserver&) =
      delete;
  ChromeGWSPageLoadMetricsObserver& operator=(
      const ChromeGWSPageLoadMetricsObserver&) = delete;

 private:
  bool IsFromNewTabPage(content::NavigationHandle* navigation_handle) override;
  bool IsBrowserStartupComplete() override;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CHROME_GWS_PAGE_LOAD_METRICS_OBSERVER_H_
