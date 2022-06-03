// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/page_load_metrics/aw_page_load_metrics_provider.h"

#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/lifecycle/aw_contents_lifecycle_notifier.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"

namespace android_webview {

AwPageLoadMetricsProvider::AwPageLoadMetricsProvider() = default;

AwPageLoadMetricsProvider::~AwPageLoadMetricsProvider() = default;

void AwPageLoadMetricsProvider::OnAppEnterBackground() {
  std::vector<const AwContents*> all_aw_contents(
      AwContentsLifecycleNotifier::GetInstance().GetAllAwContents());
  for (auto* aw_contents : all_aw_contents) {
    page_load_metrics::MetricsWebContentsObserver* observer =
        page_load_metrics::MetricsWebContentsObserver::FromWebContents(
            aw_contents->web_contents());
    if (observer)
      observer->FlushMetricsOnAppEnterBackground();
  }
}

}  // namespace android_webview