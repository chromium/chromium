// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/search/ntp_features.h"

// static
WallpaperSearchService* WallpaperSearchServiceFactory::GetForProfile(
    Profile* profile) {
  if (base::FeatureList::IsEnabled(
          ntp_features::kCustomizeChromeWallpaperSearch) &&
      base::FeatureList::IsEnabled(
          optimization_guide::features::kOptimizationGuideModelExecution)) {
    return static_cast<WallpaperSearchService*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }
  return nullptr;
}

// static
WallpaperSearchServiceFactory* WallpaperSearchServiceFactory::GetInstance() {
  static base::NoDestructor<WallpaperSearchServiceFactory> factory;
  return factory.get();
}

WallpaperSearchServiceFactory::WallpaperSearchServiceFactory()
    : ProfileKeyedServiceFactory(
          "WallpaperSearchService",
          // Uses same selections as OptimizationGuideKeyedService.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

WallpaperSearchServiceFactory::~WallpaperSearchServiceFactory() = default;

std::unique_ptr<KeyedService>
WallpaperSearchServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<WallpaperSearchService>(
      Profile::FromBrowserContext(context));
}
