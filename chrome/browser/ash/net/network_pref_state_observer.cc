// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_pref_state_observer.h"

#include "base/check.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/sync_wifi/wifi_configuration_sync_service.h"
#include "chromeos/ash/components/sync_wifi/wifi_configuration_sync_service_provider.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

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
  const user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();
  if (!user_manager->IsPrimaryUser(user_manager->FindUser(account_id))) {
    return;
  }

  // Only the user's prefs are needed, so they are read off the browser context
  // rather than through a Profile.
  content::BrowserContext* const browser_context =
      BrowserContextHelper::Get()->GetBrowserContextByAccountId(account_id);
  CHECK(browser_context);

  NetworkHandler::Get()->SetIsEnterpriseManaged(
      InstallAttributes::Get()->IsEnterpriseManaged());
  NetworkHandler::Get()->InitializePrefServices(
      user_prefs::UserPrefs::Get(browser_context), &local_state_.get());
  session_observation_.Reset();

  auto* wifi_sync_service =
      WifiConfigurationSyncServiceProvider::Get().Find(account_id);
  if (wifi_sync_service) {
    wifi_sync_service->SetNetworkMetadataStore(
        NetworkHandler::Get()->network_metadata_store()->GetWeakPtr());
  }
}

}  // namespace ash
