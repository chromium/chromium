// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service_factory.h"

#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

// static
SearchPrefetchService* SearchPrefetchServiceFactory::GetForProfile(
    Profile* profile) {
  if (SearchPrefetchServiceIsEnabled()) {
    return static_cast<SearchPrefetchService*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }
  return nullptr;
}

// static
SearchPrefetchServiceFactory* SearchPrefetchServiceFactory::GetInstance() {
  static base::NoDestructor<SearchPrefetchServiceFactory> factory;
  return factory.get();
}

SearchPrefetchServiceFactory::SearchPrefetchServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SearchPrefetchService",
          BrowserContextDependencyManager::GetInstance()) {}

SearchPrefetchServiceFactory::~SearchPrefetchServiceFactory() = default;

KeyedService* SearchPrefetchServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new SearchPrefetchService(profile);
}
