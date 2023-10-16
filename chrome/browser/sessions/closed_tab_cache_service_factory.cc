// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/closed_tab_cache_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/api/declarative/rules_registry_service.h"
#endif

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

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // ClosedTabCacheService has an indirect dependency on the
  // RulesRegistryService through extensions::TabHelper::WebContentsDestroyed.
  DependsOn(extensions::RulesRegistryService::GetFactoryInstance());
#endif
}

// static
ClosedTabCacheService* ClosedTabCacheServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ClosedTabCacheService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ClosedTabCacheServiceFactory* ClosedTabCacheServiceFactory::GetInstance() {
  static base::NoDestructor<ClosedTabCacheServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
ClosedTabCacheServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ClosedTabCacheService>(
      Profile::FromBrowserContext(context));
}

bool ClosedTabCacheServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
