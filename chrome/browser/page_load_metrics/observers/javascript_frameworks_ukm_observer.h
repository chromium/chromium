// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_JAVASCRIPT_FRAMEWORKS_UKM_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_JAVASCRIPT_FRAMEWORKS_UKM_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

// If URL-Keyed-Metrics (UKM) is enabled in the system, this is used to
// populate it with JavaScript framework-related page-load metrics.
class JavascriptFrameworksUkmObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  JavascriptFrameworksUkmObserver();
  ~JavascriptFrameworksUkmObserver() override;

  // page_load_metrics::PageLoadMetricsObserver
  void OnLoadingBehaviorObserved(content::RenderFrameHost* rfh,
                                 int behavior_flag) override;
  void OnComplete(const page_load_metrics::mojom::PageLoadTiming&) override;
  JavascriptFrameworksUkmObserver::ObservePolicy
  FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming&) override;

 private:
  // Called every time an update to NextJS detection may occur.
  void DetectNextJS();

  // Called towards the end of the page lifecycle to report metrics on the
  // frameworks detected.
  void RecordJavascriptFrameworkPageLoad();

  bool nextjs_detected_ = false;
  DISALLOW_COPY_AND_ASSIGN(JavascriptFrameworksUkmObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_JAVASCRIPT_FRAMEWORKS_UKM_OBSERVER_H_
