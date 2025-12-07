// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_tracker_factory.h"

#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/page_load_metrics/browser/features.h"
#include "content/public/browser/browser_context.h"

namespace {

std::unique_ptr<KeyedService> BuildFromGWSNavigationAndKeepAliveRequestTracker(
    content::BrowserContext* context) {
  return std::make_unique<FromGWSNavigationAndKeepAliveRequestTracker>(context);
}

}  // namespace

// static
FromGWSNavigationAndKeepAliveRequestTrackerFactory*
FromGWSNavigationAndKeepAliveRequestTrackerFactory::GetInstance() {
  static base::NoDestructor<FromGWSNavigationAndKeepAliveRequestTrackerFactory>
      instance;
  if (!base::FeatureList::IsEnabled(
          page_load_metrics::features::kBeaconLeakageLogging)) {
    return nullptr;
  }
  return instance.get();
}

// static
FromGWSNavigationAndKeepAliveRequestTracker*
FromGWSNavigationAndKeepAliveRequestTrackerFactory::GetForProfile(
    Profile* profile) {
  if (!base::FeatureList::IsEnabled(
          page_load_metrics::features::kBeaconLeakageLogging)) {
    return nullptr;
  }

  return static_cast<FromGWSNavigationAndKeepAliveRequestTracker*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
FromGWSNavigationAndKeepAliveRequestTrackerFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildFromGWSNavigationAndKeepAliveRequestTracker);
}

FromGWSNavigationAndKeepAliveRequestTrackerFactory::
    FromGWSNavigationAndKeepAliveRequestTrackerFactory()
    : ProfileKeyedServiceFactory(
          "FromGWSNavigationAndKeepAliveRequestTracker",
          ProfileSelections::Builder()
              // Tracks requests from every type of owned profile instances.
              // Don't track across OTR and Origin profiles.
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {}

FromGWSNavigationAndKeepAliveRequestTrackerFactory::
    ~FromGWSNavigationAndKeepAliveRequestTrackerFactory() = default;

std::unique_ptr<KeyedService>
FromGWSNavigationAndKeepAliveRequestTrackerFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  return std::make_unique<FromGWSNavigationAndKeepAliveRequestTracker>(context);
}
