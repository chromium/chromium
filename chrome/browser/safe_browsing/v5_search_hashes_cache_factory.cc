// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/v5_search_hashes_cache_factory.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/safe_browsing/core/browser/db/v5_search_hashes_cache.h"
#include "content/public/browser/browser_context.h"

namespace safe_browsing {

// static
V5SearchHashesCache* V5SearchHashesCacheFactory::GetForProfile(
    Profile* profile) {
  return static_cast<V5SearchHashesCache*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
V5SearchHashesCacheFactory* V5SearchHashesCacheFactory::GetInstance() {
  static base::NoDestructor<V5SearchHashesCacheFactory> instance;
  return instance.get();
}

V5SearchHashesCacheFactory::V5SearchHashesCacheFactory()
    : ProfileKeyedServiceFactory(
          "V5SearchHashesCache",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HistoryServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
V5SearchHashesCacheFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<V5SearchHashesCache>(
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS));
}

}  // namespace safe_browsing
