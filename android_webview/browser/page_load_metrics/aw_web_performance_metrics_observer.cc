// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/page_load_metrics/aw_web_performance_metrics_observer.h"

namespace android_webview {

const char* AwWebPerformanceMetricsObserver::GetObserverName() const {
  static const char kName[] = "AwWebPerformanceMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AwWebPerformanceMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AwWebPerformanceMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Fenced frames are not supported in WebView
  return STOP_OBSERVING;
}

}  // namespace android_webview
