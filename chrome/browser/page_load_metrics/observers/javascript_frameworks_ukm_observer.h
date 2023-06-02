// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_JAVASCRIPT_FRAMEWORKS_UKM_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_JAVASCRIPT_FRAMEWORKS_UKM_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "third_party/blink/public/common/loader/javascript_framework_detection.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"

// If URL-Keyed-Metrics (UKM) is enabled in the system, this is used to
// populate it with JavaScript framework-related page-load metrics.
class JavascriptFrameworksUkmObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  JavascriptFrameworksUkmObserver();

  JavascriptFrameworksUkmObserver(const JavascriptFrameworksUkmObserver&) =
      delete;
  JavascriptFrameworksUkmObserver& operator=(
      const JavascriptFrameworksUkmObserver&) = delete;

  ~JavascriptFrameworksUkmObserver() override;

  // page_load_metrics::PageLoadMetricsObserver
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  void OnJavaScriptFrameworksObserved(
      content::RenderFrameHost* rfh,
      const blink::JavaScriptFrameworkDetectionResult&) override;
  void OnComplete(const page_load_metrics::mojom::PageLoadTiming&) override;
  JavascriptFrameworksUkmObserver::ObservePolicy
  FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming&) override;
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;

 private:
  // Called towards the end of the page lifecycle to report metrics on the
  // frameworks detected.
  void RecordJavascriptFrameworkPageLoad();

  blink::JavaScriptFrameworkDetectionResult framework_detection_result_;

  bool is_in_prerendered_page_ = false;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_JAVASCRIPT_FRAMEWORKS_UKM_OBSERVER_H_
