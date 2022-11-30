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
SearchPrefetchServiceFactory* SearchPrefetchServiceFactory::GetInstance() {
  static base::NoDestructor<SearchPrefetchServiceFactory> factory;
  return factory.get();
}

SearchPrefetchServiceFactory::SearchPrefetchServiceFactory()
    : ProfileKeyedServiceFactory("SearchPrefetchService") {}

SearchPrefetchServiceFactory::~SearchPrefetchServiceFactory() = default;

KeyedService* SearchPrefetchServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new SearchPrefetchService(profile);
}
