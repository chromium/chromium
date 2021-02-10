// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_MEMORY_TRACKER_FACTORY_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_MEMORY_TRACKER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace page_load_metrics {

class PageLoadMetricsMemoryTracker;

class PageLoadMetricsMemoryTrackerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static PageLoadMetricsMemoryTracker* GetForBrowserContext(
      content::BrowserContext* context);

  static PageLoadMetricsMemoryTrackerFactory* GetInstance();

  PageLoadMetricsMemoryTrackerFactory();

 private:
  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_MEMORY_TRACKER_FACTORY_H_
