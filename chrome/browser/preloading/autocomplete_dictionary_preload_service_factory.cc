// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/autocomplete_dictionary_preload_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/preloading/autocomplete_dictionary_preload_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "content/public/browser/browser_context.h"

namespace {
ProfileSelections GetProfileSelections() {
  // This profile selection logic was copied from SearchPrefetchService where
  // the autocomplete dictionary preloading was originally implemented.
  // Actually, this service may not be necessary for some profiles (see TODOs
  // below).
  ProfileSelection profile_selection = ProfileSelection::kOriginalOnly;
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
AutocompleteDictionaryPreloadService*
AutocompleteDictionaryPreloadServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AutocompleteDictionaryPreloadService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
AutocompleteDictionaryPreloadServiceFactory*
AutocompleteDictionaryPreloadServiceFactory::GetInstance() {
  static base::NoDestructor<AutocompleteDictionaryPreloadServiceFactory>
      factory;
  return factory.get();
}

AutocompleteDictionaryPreloadServiceFactory::
    AutocompleteDictionaryPreloadServiceFactory()
    : ProfileKeyedServiceFactory("AutocompleteDictionaryPreloadService",
                                 GetProfileSelections()) {}

AutocompleteDictionaryPreloadServiceFactory::
    ~AutocompleteDictionaryPreloadServiceFactory() = default;

std::unique_ptr<KeyedService> AutocompleteDictionaryPreloadServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<AutocompleteDictionaryPreloadService>(*profile);
}
