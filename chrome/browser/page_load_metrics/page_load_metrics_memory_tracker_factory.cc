// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/page_load_metrics_memory_tracker_factory.h"

#include "base/no_destructor.h"
#include "components/page_load_metrics/browser/page_load_metrics_memory_tracker.h"

namespace page_load_metrics {

PageLoadMetricsMemoryTracker*
PageLoadMetricsMemoryTrackerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<page_load_metrics::PageLoadMetricsMemoryTracker*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

PageLoadMetricsMemoryTrackerFactory*
PageLoadMetricsMemoryTrackerFactory::GetInstance() {
  static base::NoDestructor<PageLoadMetricsMemoryTrackerFactory> instance;
  return instance.get();
}

PageLoadMetricsMemoryTrackerFactory::PageLoadMetricsMemoryTrackerFactory()
    : ProfileKeyedServiceFactory(
          "PageLoadMetricsMemoryTracker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

bool PageLoadMetricsMemoryTrackerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return base::FeatureList::IsEnabled(features::kV8PerFrameMemoryMonitoring);
}

std::unique_ptr<KeyedService>
PageLoadMetricsMemoryTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<page_load_metrics::PageLoadMetricsMemoryTracker>();
}

}  // namespace page_load_metrics
