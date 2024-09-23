// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/url_language_histogram_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/pref_registry/pref_registry_syncable.h"

// static
UrlLanguageHistogramFactory* UrlLanguageHistogramFactory::GetInstance() {
  static base::NoDestructor<UrlLanguageHistogramFactory> instance;
  return instance.get();
}

// static
language::UrlLanguageHistogram*
UrlLanguageHistogramFactory::GetForBrowserContext(
    content::BrowserContext* const browser_context) {
  return static_cast<language::UrlLanguageHistogram*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

UrlLanguageHistogramFactory::UrlLanguageHistogramFactory()
    : ProfileKeyedServiceFactory(
          "UrlLanguageHistogram",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

UrlLanguageHistogramFactory::~UrlLanguageHistogramFactory() = default;

std::unique_ptr<KeyedService>
UrlLanguageHistogramFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* const browser_context) const {
  Profile* const profile = Profile::FromBrowserContext(browser_context);
  return std::make_unique<language::UrlLanguageHistogram>(profile->GetPrefs());
}

void UrlLanguageHistogramFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* const registry) {
  language::UrlLanguageHistogram::RegisterProfilePrefs(registry);
}
