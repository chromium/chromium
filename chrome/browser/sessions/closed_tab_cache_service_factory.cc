// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/closed_tab_cache_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"

ClosedTabCacheServiceFactory::ClosedTabCacheServiceFactory()
    : ProfileKeyedServiceFactory(
          "ClosedTabCacheService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
}

// static
ClosedTabCacheService* ClosedTabCacheServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ClosedTabCacheService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ClosedTabCacheServiceFactory* ClosedTabCacheServiceFactory::GetInstance() {
  return base::Singleton<ClosedTabCacheServiceFactory>::get();
}

KeyedService* ClosedTabCacheServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ClosedTabCacheService(static_cast<Profile*>(context));
}

bool ClosedTabCacheServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
