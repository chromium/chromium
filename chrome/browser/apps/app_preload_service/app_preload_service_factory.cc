// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/app_preload_service_factory.h"

#include "chrome/browser/apps/app_preload_service/app_preload_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"

namespace apps {

AppPreloadServiceFactory::AppPreloadServiceFactory()
    : ProfileKeyedServiceFactory(
          "AppPreloadService",
          // Service is available in Kiosk, Guest, and Regular but not in
          // incognito profiles.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

AppPreloadServiceFactory::~AppPreloadServiceFactory() = default;

// static
AppPreloadService* AppPreloadServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AppPreloadService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AppPreloadServiceFactory* AppPreloadServiceFactory::GetInstance() {
  static base::NoDestructor<AppPreloadServiceFactory> instance;
  return instance.get();
}

// static
bool AppPreloadServiceFactory::IsAvailable(Profile* profile) {
  return base::FeatureList::IsEnabled(features::kAppPreloadService);
}

KeyedService* AppPreloadServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!IsAvailable(profile)) {
    return nullptr;
  }
  return new AppPreloadService(profile);
}

bool AppPreloadServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace apps
