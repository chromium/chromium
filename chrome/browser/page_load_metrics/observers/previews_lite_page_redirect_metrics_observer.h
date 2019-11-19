// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PREVIEWS_LITE_PAGE_REDIRECT_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PREVIEWS_LITE_PAGE_REDIRECT_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer_base.h"

// Sets UKM metrics for Lite Page Redirect previews.
class PreviewsLitePageRedirectMetricsObserver
    : public data_reduction_proxy::DataReductionProxyMetricsObserverBase {
 public:
  PreviewsLitePageRedirectMetricsObserver();
  ~PreviewsLitePageRedirectMetricsObserver() override;

  // data_reduction_proxy::DataReductionProxyMetricsObserverBase:
  ObservePolicy OnCommitCalled(content::NavigationHandle* handle,
                               ukm::SourceId source_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageRedirectMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PREVIEWS_LITE_PAGE_REDIRECT_METRICS_OBSERVER_H_
