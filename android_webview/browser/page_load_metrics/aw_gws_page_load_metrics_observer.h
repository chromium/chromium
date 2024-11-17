// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_GWS_PAGE_LOAD_METRICS_OBSERVER_H_
#define ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_GWS_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/google/browser/gws_page_load_metrics_observer.h"

namespace android_webview {

class AwGWSPageLoadMetricsObserver : public GWSPageLoadMetricsObserver {
 public:
  AwGWSPageLoadMetricsObserver();

  AwGWSPageLoadMetricsObserver(const AwGWSPageLoadMetricsObserver&) = delete;
  AwGWSPageLoadMetricsObserver& operator=(const AwGWSPageLoadMetricsObserver&) =
      delete;

 private:
  bool IsFromNewTabPage(content::NavigationHandle* navigation_handle) override;
  bool IsBrowserStartupComplete() override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_GWS_PAGE_LOAD_METRICS_OBSERVER_H_
