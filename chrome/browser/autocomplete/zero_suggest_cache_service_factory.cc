// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/zero_suggest_cache_service_factory.h"

#include "components/omnibox/browser/omnibox_field_trial.h"

// static
ZeroSuggestCacheService* ZeroSuggestCacheServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ZeroSuggestCacheService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ZeroSuggestCacheServiceFactory* ZeroSuggestCacheServiceFactory::GetInstance() {
  return base::Singleton<ZeroSuggestCacheServiceFactory>::get();
}

KeyedService* ZeroSuggestCacheServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ZeroSuggestCacheService(
      OmniboxFieldTrial::kZeroSuggestCacheMaxSize.Get());
}

ZeroSuggestCacheServiceFactory::ZeroSuggestCacheServiceFactory()
    : ProfileKeyedServiceFactory("ZeroSuggestCacheServiceFactory") {}

ZeroSuggestCacheServiceFactory::~ZeroSuggestCacheServiceFactory() = default;
