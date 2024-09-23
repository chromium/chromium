// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/zero_suggest_cache_service_factory.h"

#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/omnibox_field_trial.h"

// static
ZeroSuggestCacheService* ZeroSuggestCacheServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ZeroSuggestCacheService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ZeroSuggestCacheServiceFactory* ZeroSuggestCacheServiceFactory::GetInstance() {
  static base::NoDestructor<ZeroSuggestCacheServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
ZeroSuggestCacheServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ZeroSuggestCacheService>(
      std::make_unique<ChromeAutocompleteSchemeClassifier>(profile),
      profile->GetPrefs(), OmniboxFieldTrial::kZeroSuggestCacheMaxSize.Get());
}

ZeroSuggestCacheServiceFactory::ZeroSuggestCacheServiceFactory()
    : ProfileKeyedServiceFactory(
          "ZeroSuggestCacheServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

ZeroSuggestCacheServiceFactory::~ZeroSuggestCacheServiceFactory() = default;
