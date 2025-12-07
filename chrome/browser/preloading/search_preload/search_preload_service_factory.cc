// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/search_preload/search_preload_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/preloading/search_preload/search_preload_features.h"
#include "chrome/browser/preloading/search_preload/search_preload_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

namespace {

ProfileSelections GetProfileSelections() {
  // Note `SearchPreloadService` partially supports Incognito profile. For now,
  // it supports the on-press triggered search prefetch only. Other prefetches
  // must not be triggered in Incognito. For more details, see
  // https://crbug.com/394716358.
  ProfileSelection profile_selection =
      features::IsDsePreload2OnPressIncognitoEnabled()
          ? ProfileSelection::kOwnInstance
          : ProfileSelection::kOriginalOnly;
  return ProfileSelections::Builder()
      .WithRegular(profile_selection)
      // TODO(crbug.com/40257657): Check if this
      // service is needed in Guest mode.
      .WithGuest(profile_selection)
      // TODO(crbug.com/41488885): Check if this
      // service is needed for Ash Internals.
      .WithAshInternals(profile_selection)
      .Build();
}

}  // namespace

// static
SearchPreloadService* SearchPreloadServiceFactory::GetForProfile(
    Profile* profile) {
  if (auto* self = GetInstance()) {
    return static_cast<SearchPreloadService*>(
        self->GetServiceForBrowserContext(profile, /*create=*/true));
  }

  return nullptr;
}

// static
SearchPreloadServiceFactory* SearchPreloadServiceFactory::GetInstance() {
  static base::NoDestructor<SearchPreloadServiceFactory> factory;
  if (features::IsDsePreload2Enabled()) {
    return factory.get();
  }

  return nullptr;
}

SearchPreloadServiceFactory::SearchPreloadServiceFactory()
    : ProfileKeyedServiceFactory("SearchPreloadService",
                                 GetProfileSelections()) {}

SearchPreloadServiceFactory::~SearchPreloadServiceFactory() = default;

std::unique_ptr<KeyedService>
SearchPreloadServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<SearchPreloadService>(profile);
}
