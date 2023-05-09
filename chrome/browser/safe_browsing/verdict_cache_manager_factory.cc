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
  return base::Singleton<VerdictCacheManagerFactory>::get();
}

VerdictCacheManagerFactory::VerdictCacheManagerFactory()
    : ProfileKeyedServiceFactory(
          "VerdictCacheManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

KeyedService* VerdictCacheManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new VerdictCacheManager(
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      HostContentSettingsMapFactory::GetForProfile(profile),
      profile->GetPrefs(),
      std::make_unique<SafeBrowsingSyncObserverImpl>(
          SyncServiceFactory::GetForProfile(profile)));
}

}  // namespace safe_browsing
