// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/local_image_search_service_factory.h"

#include "chrome/browser/ash/app_list/search/local_image_search/local_image_search_service.h"
#include "chrome/browser/ash/app_list/search/search_features.h"

namespace app_list {
namespace {

ProfileSelections BuildLocalImageSearchServiceProfileSelections() {
  if (search_features::IsLauncherImageSearchEnabled()) {
    return ProfileSelections::Builder()
        // Works only with regular profiles and not off the record (OTR).
        .WithRegular(ProfileSelection::kOriginalOnly)
        .WithAshInternals(ProfileSelection::kNone)
        .Build();
  }

  return ProfileSelections::BuildNoProfilesSelected();
}

}  // namespace

// static
LocalImageSearchService* LocalImageSearchServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<LocalImageSearchService*>(
      LocalImageSearchServiceFactory::GetInstance()
          ->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
LocalImageSearchServiceFactory* LocalImageSearchServiceFactory::GetInstance() {
  static base::NoDestructor<LocalImageSearchServiceFactory> instance;
  return instance.get();
}

LocalImageSearchServiceFactory::LocalImageSearchServiceFactory()
    : ProfileKeyedServiceFactory(
          "LocalImageSearchService",
          BuildLocalImageSearchServiceProfileSelections()) {}

LocalImageSearchServiceFactory::~LocalImageSearchServiceFactory() = default;

std::unique_ptr<KeyedService>
LocalImageSearchServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // In Ash, GuestSession uses Regular Profile, for which we will try to create
  // the service. Do not create the service for Guest Session.
  if (profile->IsGuestSession()) {
    return nullptr;
  }

  return std::make_unique<LocalImageSearchService>(profile);
}

bool LocalImageSearchServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // If true, it initializes the storage at log in, so that the worker can watch
  // files in the background.
  return true;
}

}  // namespace app_list
