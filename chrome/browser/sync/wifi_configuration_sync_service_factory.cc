// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/wifi_configuration_sync_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chromeos/components/sync_wifi/pending_network_configuration_tracker_impl.h"
#include "chromeos/components/sync_wifi/wifi_configuration_bridge.h"
#include "chromeos/components/sync_wifi/wifi_configuration_sync_service.h"
#include "chromeos/network/network_handler.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync/model/model_type_store_service.h"

// static
chromeos::sync_wifi::WifiConfigurationSyncService*
WifiConfigurationSyncServiceFactory::GetForProfile(Profile* profile,
                                                   bool create) {
  if (!ShouldRunInProfile(profile)) {
    return nullptr;
  }

  return static_cast<chromeos::sync_wifi::WifiConfigurationSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, create));
}

// static
WifiConfigurationSyncServiceFactory*
WifiConfigurationSyncServiceFactory::GetInstance() {
  return base::Singleton<WifiConfigurationSyncServiceFactory>::get();
}

// static
bool WifiConfigurationSyncServiceFactory::ShouldRunInProfile(
    const Profile* profile) {
  // Run when signed in to a real account.  Skip during tests when network stack
  // has not been initialized.
  return profile && chromeos::ProfileHelper::IsRegularProfile(profile) &&
         !profile->IsOffTheRecord() &&
         chromeos::NetworkHandler::IsInitialized();
}

WifiConfigurationSyncServiceFactory::WifiConfigurationSyncServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "WifiConfigurationSyncService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
}

WifiConfigurationSyncServiceFactory::~WifiConfigurationSyncServiceFactory() =
    default;

KeyedService* WifiConfigurationSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new chromeos::sync_wifi::WifiConfigurationSyncService(
      chrome::GetChannel(), profile->GetPrefs(),
      ModelTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory());
}

void WifiConfigurationSyncServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  chromeos::sync_wifi::PendingNetworkConfigurationTrackerImpl::
      RegisterProfilePrefs(registry);
  chromeos::sync_wifi::WifiConfigurationBridge::RegisterPrefs(registry);
}
