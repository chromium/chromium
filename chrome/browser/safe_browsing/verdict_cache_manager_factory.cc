// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_sync_observer_impl.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "content/public/browser/browser_context.h"

namespace safe_browsing {

// static
VerdictCacheManager* VerdictCacheManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<VerdictCacheManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */ true));
}

// static
VerdictCacheManagerFactory* VerdictCacheManagerFactory::GetInstance() {
  static base::NoDestructor<VerdictCacheManagerFactory> instance;
  return instance.get();
}

VerdictCacheManagerFactory::VerdictCacheManagerFactory()
    : ProfileKeyedServiceFactory(
          "VerdictCacheManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
VerdictCacheManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<VerdictCacheManager>(
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      HostContentSettingsMapFactory::GetForProfile(profile),
      profile->GetPrefs(),
      std::make_unique<SafeBrowsingSyncObserverImpl>(
          SyncServiceFactory::GetForProfile(profile)));
}

}  // namespace safe_browsing
