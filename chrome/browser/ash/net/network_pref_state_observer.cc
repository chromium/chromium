// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_pref_state_observer.h"

#include "base/check.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/sync_wifi/wifi_configuration_sync_service.h"
#include "chromeos/ash/components/sync_wifi/wifi_configuration_sync_service_provider.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

NetworkPrefStateObserver::NetworkPrefStateObserver(PrefService& local_state)
    : local_state_(local_state) {
  // Initialize NetworkHandler with device prefs only.
  NetworkHandler::Get()->InitializePrefServices(
      /*logged_in_profile_prefs=*/nullptr, &local_state);

  session_observation_.Observe(session_manager::SessionManager::Get());
}

NetworkPrefStateObserver::~NetworkPrefStateObserver() {
  NetworkHandler::Get()->ShutdownPrefServices();
}

void NetworkPrefStateObserver::OnUserProfileLoaded(
    const AccountId& account_id) {
  // Reinitialize the NetworkHandler's pref service when the primary user logs
  // in. Other users are ignored because only the primary user's network
  // configuration is used on ChromeOS.
  const session_manager::Session* const primary_session =
      session_manager::SessionManager::Get()->GetPrimarySession();
  if (!primary_session || primary_session->account_id() != account_id) {
    return;
  }

  // The Profile has been created by now, so its prefs are attached to the User.
  PrefService* const profile_prefs = user_manager::UserManager::Get()
                                         ->FindUserAndModify(account_id)
                                         ->GetProfilePrefs();
  CHECK(profile_prefs);

  NetworkHandler::Get()->SetIsEnterpriseManaged(
      InstallAttributes::Get()->IsEnterpriseManaged());
  NetworkHandler::Get()->InitializePrefServices(profile_prefs,
                                                &local_state_.get());
  session_observation_.Reset();

  auto* wifi_sync_service =
      WifiConfigurationSyncServiceProvider::Get().Find(account_id);
  if (wifi_sync_service) {
    wifi_sync_service->SetNetworkMetadataStore(
        NetworkHandler::Get()->network_metadata_store()->GetWeakPtr());
  }
}

}  // namespace ash
