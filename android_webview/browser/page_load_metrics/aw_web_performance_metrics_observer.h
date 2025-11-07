// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_WEB_PERFORMANCE_METRICS_OBSERVER_H_
#define ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_WEB_PERFORMANCE_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"

namespace android_webview {

// Forwards Web Performance metrics for a page load in WebView
// to the Java side of WebView.
class AwWebPerformanceMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  AwWebPerformanceMetricsObserver() = default;
  AwWebPerformanceMetricsObserver(const AwWebPerformanceMetricsObserver&) =
      delete;
  ~AwWebPerformanceMetricsObserver() override = default;

  AwWebPerformanceMetricsObserver& operator=(
      const AwWebPerformanceMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver:
  const char* GetObserverName() const override;

  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_WEB_PERFORMANCE_METRICS_OBSERVER_H_
