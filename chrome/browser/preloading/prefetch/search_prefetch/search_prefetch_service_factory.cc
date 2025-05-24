// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/search_preload/search_preload_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "content/public/browser/browser_context.h"

namespace {
ProfileSelections GetProfileSelections() {
  // Note SearchPrefetchService partially supports Incognito profile. For now,
  // it supports the on-press triggered search prefetch only. Other prefetches
  // must not be triggered in Incognito. crbug.com/394716358 for more details.
  ProfileSelection profile_selection = IsPrefetchIncognitoEnabled()
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
SearchPrefetchService* SearchPrefetchServiceFactory::GetForProfile(
    Profile* profile) {
  if (auto* self = GetInstance()) {
    return static_cast<SearchPrefetchService*>(
        self->GetServiceForBrowserContext(profile, /*create=*/true));
  }

  return nullptr;
}

// static
SearchPrefetchServiceFactory* SearchPrefetchServiceFactory::GetInstance() {
  static base::NoDestructor<SearchPrefetchServiceFactory> factory;
  if (features::IsDsePreload2Enabled()) {
    return nullptr;
  }

  return factory.get();
}

SearchPrefetchServiceFactory::SearchPrefetchServiceFactory()
    : ProfileKeyedServiceFactory("SearchPrefetchService",
                                 GetProfileSelections()) {}

SearchPrefetchServiceFactory::~SearchPrefetchServiceFactory() = default;

std::unique_ptr<KeyedService>
SearchPrefetchServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<SearchPrefetchService>(profile);
}
