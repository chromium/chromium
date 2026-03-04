// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/search_prewarm_progress_service_factory.h"

#include "chrome/browser/preloading/prerender/search_prewarm_progress_service.h"
#include "chrome/browser/profiles/profile.h"

// static
SearchPrewarmProgressService*
SearchPrewarmProgressServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SearchPrewarmProgressService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
SearchPrewarmProgressServiceFactory*
SearchPrewarmProgressServiceFactory::GetInstance() {
  static base::NoDestructor<SearchPrewarmProgressServiceFactory> instance;
  return instance.get();
}

SearchPrewarmProgressServiceFactory::SearchPrewarmProgressServiceFactory()
    : ProfileKeyedServiceFactory(
          "SearchPrewarmProgressService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .Build()) {}

SearchPrewarmProgressServiceFactory::~SearchPrewarmProgressServiceFactory() =
    default;

std::unique_ptr<KeyedService>
SearchPrewarmProgressServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return std::make_unique<SearchPrewarmProgressService>();
}
