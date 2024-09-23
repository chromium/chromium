// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

// static
SearchPrefetchService* SearchPrefetchServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SearchPrefetchService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SearchPrefetchService* SearchPrefetchServiceFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<SearchPrefetchService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
SearchPrefetchServiceFactory* SearchPrefetchServiceFactory::GetInstance() {
  static base::NoDestructor<SearchPrefetchServiceFactory> factory;
  return factory.get();
}

SearchPrefetchServiceFactory::SearchPrefetchServiceFactory()
    : ProfileKeyedServiceFactory(
          "SearchPrefetchService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

SearchPrefetchServiceFactory::~SearchPrefetchServiceFactory() = default;

std::unique_ptr<KeyedService>
SearchPrefetchServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<SearchPrefetchService>(profile);
}
