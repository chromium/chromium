// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_PAGE_LOAD_METRICS_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_PAGE_LOAD_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace android_webview {

// MetricsProvider that interfaces with page_load_metrics.
class AwPageLoadMetricsProvider : public metrics::MetricsProvider {
 public:
  AwPageLoadMetricsProvider();

  AwPageLoadMetricsProvider(const AwPageLoadMetricsProvider&) = delete;
  AwPageLoadMetricsProvider& operator=(const AwPageLoadMetricsProvider&) =
      delete;

  ~AwPageLoadMetricsProvider() override;

  // metrics:MetricsProvider:
  void OnAppEnterBackground() override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_PAGE_LOAD_METRICS_PROVIDER_H_