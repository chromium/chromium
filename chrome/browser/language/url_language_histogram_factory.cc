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
  return base::Singleton<UrlLanguageHistogramFactory>::get();
}

// static
language::UrlLanguageHistogram*
UrlLanguageHistogramFactory::GetForBrowserContext(
    content::BrowserContext* const browser_context) {
  return static_cast<language::UrlLanguageHistogram*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

UrlLanguageHistogramFactory::UrlLanguageHistogramFactory()
    : ProfileKeyedServiceFactory("UrlLanguageHistogram") {}

UrlLanguageHistogramFactory::~UrlLanguageHistogramFactory() {}

KeyedService* UrlLanguageHistogramFactory::BuildServiceInstanceFor(
    content::BrowserContext* const browser_context) const {
  Profile* const profile = Profile::FromBrowserContext(browser_context);
  return new language::UrlLanguageHistogram(profile->GetPrefs());
}

void UrlLanguageHistogramFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* const registry) {
  language::UrlLanguageHistogram::RegisterProfilePrefs(registry);
}
