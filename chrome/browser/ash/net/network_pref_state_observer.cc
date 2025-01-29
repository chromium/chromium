// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_pref_state_observer.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/wifi_configuration_sync_service_factory.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/sync_wifi/wifi_configuration_sync_service.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

void InitializeNetworkPrefServices(Profile* profile, PrefService* local_state) {
  NetworkHandler::Get()->InitializePrefServices(
      profile ? profile->GetPrefs() : nullptr, local_state);
}

}  // namespace

NetworkPrefStateObserver::NetworkPrefStateObserver(PrefService& local_state)
    : local_state_(local_state) {
  // Initialize NetworkHandler with device prefs only.
  InitializeNetworkPrefServices(/*profile=*/nullptr, &local_state);

  session_observation_.Observe(session_manager::SessionManager::Get());
}

NetworkPrefStateObserver::~NetworkPrefStateObserver() {
  NetworkHandler::Get()->ShutdownPrefServices();
}

void NetworkPrefStateObserver::OnUserProfileLoaded(
    const AccountId& account_id) {
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  CHECK(profile);

  // Reinitialize the NetworkHandler's pref service when the primary user logs
  // in. Other profiles are ignored because only the primary user's network
  // configuration is used on Chrome OS.
  if (ProfileHelper::IsPrimaryProfile(profile)) {
    NetworkHandler::Get()->SetIsEnterpriseManaged(
        InstallAttributes::Get()->IsEnterpriseManaged());
    InitializeNetworkPrefServices(profile, &local_state_.get());
    session_observation_.Reset();

    auto* wifi_sync_service =
        WifiConfigurationSyncServiceFactory::GetForProfile(profile,
                                                           /*create=*/false);
    if (wifi_sync_service) {
      wifi_sync_service->SetNetworkMetadataStore(
          NetworkHandler::Get()->network_metadata_store()->GetWeakPtr());
    }
  }
}

}  // namespace ash
