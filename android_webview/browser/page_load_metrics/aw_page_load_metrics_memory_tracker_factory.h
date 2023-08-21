// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_PAGE_LOAD_METRICS_MEMORY_TRACKER_FACTORY_H_
#define ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_PAGE_LOAD_METRICS_MEMORY_TRACKER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace page_load_metrics {
class PageLoadMetricsMemoryTracker;
}  // namespace page_load_metrics

namespace android_webview {

class AwPageLoadMetricsMemoryTrackerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static page_load_metrics::PageLoadMetricsMemoryTracker* GetForBrowserContext(
      content::BrowserContext* context);

  static AwPageLoadMetricsMemoryTrackerFactory* GetInstance();

  AwPageLoadMetricsMemoryTrackerFactory();

 private:
  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_PAGE_LOAD_METRICS_MEMORY_TRACKER_FACTORY_H_
