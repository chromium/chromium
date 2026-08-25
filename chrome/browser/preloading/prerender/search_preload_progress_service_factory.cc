// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/search_preload_progress_service_factory.h"

#include "chrome/browser/preloading/prerender/search_preload_progress_service.h"
#include "chrome/browser/profiles/profile.h"

// static
SearchPreloadProgressService*
SearchPreloadProgressServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SearchPreloadProgressService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
SearchPreloadProgressServiceFactory*
SearchPreloadProgressServiceFactory::GetInstance() {
  static base::NoDestructor<SearchPreloadProgressServiceFactory> instance;
  return instance.get();
}

SearchPreloadProgressServiceFactory::SearchPreloadProgressServiceFactory()
    : ProfileKeyedServiceFactory(
          "SearchPreloadProgressService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .Build()) {}

SearchPreloadProgressServiceFactory::~SearchPreloadProgressServiceFactory() =
    default;

std::unique_ptr<KeyedService>
SearchPreloadProgressServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return std::make_unique<SearchPreloadProgressService>();
}
