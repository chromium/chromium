// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/network_settings_service_ash.h"

#include <stdint.h>

#include "base/logging.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/network_settings_translation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/proxy/proxy_config_service_impl.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/user_manager/user_manager.h"

namespace {

// Returns the preference service of the primary user session profile, or
// nullptr if no user session has started.
PrefService* GetPrimaryLoggedInUserProfilePrefs() {
  // Check login state first.
  if (!user_manager::UserManager::IsInitialized() ||
      !user_manager::UserManager::Get()->IsUserLoggedIn() ||
      ProfileManager::GetPrimaryUserProfile() == nullptr) {
    return nullptr;
  }
  return ProfileManager::GetPrimaryUserProfile()->GetPrefs();
}

}  // namespace
namespace crosapi {

NetworkSettingsServiceAsh::NetworkSettingsServiceAsh(PrefService* local_state)
    : local_state_(local_state),
      profile_manager_(g_browser_process->profile_manager()) {
  if (profile_manager_) {
    profile_manager_->AddObserver(this);
  }
  // Uninitialized in unit_tests.
  if (chromeos::NetworkHandler::IsInitialized()) {
    chromeos::NetworkHandler::Get()->network_state_handler()->AddObserver(
        this, FROM_HERE);
  }
  observers_.set_disconnect_handler(base::BindRepeating(
      &NetworkSettingsServiceAsh::OnDisconnect, base::Unretained(this)));
}

NetworkSettingsServiceAsh::~NetworkSettingsServiceAsh() {
  // Uninitialized in unit_tests.
  if (chromeos::NetworkHandler::IsInitialized()) {
    chromeos::NetworkHandler::Get()->network_state_handler()->RemoveObserver(
        this, FROM_HERE);
  }
  if (profile_manager_)
    profile_manager_->RemoveObserver(this);
}

void NetworkSettingsServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::NetworkSettingsService> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void NetworkSettingsServiceAsh::DefaultNetworkChanged(
    const chromeos::NetworkState* network) {
  if (!network) {
    cached_wpad_url_ = GURL();
    return;
  }
  cached_wpad_url_ = network->GetWebProxyAutoDiscoveryUrl();
  DetermineEffectiveProxy();
}

void NetworkSettingsServiceAsh::SendProxyConfigToObservers() {
  if (!cached_proxy_config_) {
    LOG(ERROR) << "No proxy configuration to forward";
    return;
  }
  for (const auto& obs : observers_) {
    obs->OnProxyChanged(cached_proxy_config_.Clone());
  }
}

void NetworkSettingsServiceAsh::AddNetworkSettingsObserver(
    mojo::PendingRemote<mojom::NetworkSettingsObserver> observer) {
  StartTrackingPrefChanges();
  mojo::Remote<mojom::NetworkSettingsObserver> remote(std::move(observer));
  if (cached_proxy_config_) {
    remote->OnProxyChanged(cached_proxy_config_.Clone());
  }
  observers_.Add(std::move(remote));
}

void NetworkSettingsServiceAsh::StartTrackingPrefChanges() {
  if (profile_prefs_registrar_)
    return;  // already listening to pref changes

  PrefService* pref_service = GetPrimaryLoggedInUserProfilePrefs();
  DCHECK(pref_service) << "Pref service for primary profile is not initialized";
  profile_prefs_registrar_ = std::make_unique<PrefChangeRegistrar>();
  profile_prefs_registrar_->Init(pref_service);
  profile_prefs_registrar_->Add(
      proxy_config::prefs::kProxy,
      base::BindRepeating(&NetworkSettingsServiceAsh::OnPrefChanged,
                          base::Unretained(this)));
}

void NetworkSettingsServiceAsh::OnPrefChanged() {
  DetermineEffectiveProxy();
}

void NetworkSettingsServiceAsh::DetermineEffectiveProxy() {
  PrefService* pref_service = GetPrimaryLoggedInUserProfilePrefs();
  if (!pref_service)
    return;
  crosapi::mojom::ProxyConfigPtr new_proxy_config = ProxyConfigToCrosapiProxy(
      chromeos::ProxyConfigServiceImpl::GetActiveProxyConfigDictionary(
          pref_service, local_state_)
          .get(),
      cached_wpad_url_);

  // Trigger a proxy settings sync to Lacros if the proxy configuration changes.
  if (!cached_proxy_config_ ||
      !new_proxy_config->Equals(*cached_proxy_config_)) {
    cached_proxy_config_ = std::move(new_proxy_config);
    SendProxyConfigToObservers();
  }
}

void NetworkSettingsServiceAsh::OnDisconnect(mojo::RemoteSetElementId mojo_id) {
  observers_.Remove(mojo_id);
  if (!observers_.empty())
    return;
  // Stop observing proxy pref.
  profile_prefs_registrar_.reset();
}

void NetworkSettingsServiceAsh::OnProfileManagerDestroying() {
  profile_prefs_registrar_.reset();

  if (!profile_manager_)
    return;
  profile_manager_->RemoveObserver(this);
  profile_manager_ = nullptr;
}

}  // namespace crosapi
