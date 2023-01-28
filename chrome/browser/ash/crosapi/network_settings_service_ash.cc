// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/network_settings_service_ash.h"

#include <stdint.h>

#include "ash/constants/ash_pref_names.h"
#include "base/logging.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/network_settings_translation.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "chromeos/ash/components/network/proxy/proxy_config_service_impl.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/user_manager/user_manager.h"

namespace {

const char kPrefExtensionNameKey[] = "extension_name_key";
const char kPrefExtensionIdKey[] = "extension_id_key";
const char kPrefExtensionCanDisabledKey[] = "can_be_disabled_key";

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
  if (ash::NetworkHandler::IsInitialized()) {
    network_state_handler_observer_.Observe(
        ash::NetworkHandler::Get()->network_state_handler());
  }
  observers_.set_disconnect_handler(base::BindRepeating(
      &NetworkSettingsServiceAsh::OnDisconnect, base::Unretained(this)));
}

NetworkSettingsServiceAsh::~NetworkSettingsServiceAsh() {
  if (profile_manager_) {
    profile_manager_->RemoveObserver(this);
  }
}

void NetworkSettingsServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::NetworkSettingsService> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void NetworkSettingsServiceAsh::DefaultNetworkChanged(
    const ash::NetworkState* network) {
  if (!network) {
    cached_wpad_url_ = GURL();
    return;
  }
  cached_wpad_url_ = network->GetWebProxyAutoDiscoveryUrl();
  DetermineEffectiveProxy();
}

// static
void NetworkSettingsServiceAsh::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(
      ash::prefs::kLacrosProxyControllingExtension);
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

// Note that the this will update the value of the kProxy preference in the user
// store (in Lacros, the extension set proxy is stored in the extension store).
// Long term, we should deprecate propagating Lacros extension set proxies in
// Ash in favor of system extensions.
void NetworkSettingsServiceAsh::SetExtensionProxy(
    crosapi::mojom::ProxyConfigPtr proxy_config) {
  DCHECK(proxy_config)
      << "Received SetExtensionProxy request with missing proxy configuration.";
  PrefService* pref_service = GetPrimaryLoggedInUserProfilePrefs();
  DCHECK(pref_service);

  extension_controlling_proxy_.reset();

  if (!proxy_config->extension) {
    LOG(ERROR)
        << "Received extension proxy configuration without extension data";
    return;
  }

  extension_controlling_proxy_ = proxy_config->extension.Clone();

  // Required to display the extension which is controlling the proxy in the OS
  // Settings > Network > Proxy window.
  base::Value proxy_extension(base::Value::Type::DICT);
  proxy_extension.SetStringKey(kPrefExtensionNameKey,
                               proxy_config->extension->name);
  proxy_extension.SetStringKey(kPrefExtensionIdKey,
                               proxy_config->extension->id);
  proxy_extension.SetBoolKey(kPrefExtensionCanDisabledKey,
                             proxy_config->extension->can_be_disabled);
  pref_service->Set(ash::prefs::kLacrosProxyControllingExtension,
                    std::move(proxy_extension));

  pref_service->SetDict(proxy_config::prefs::kProxy,
                        CrosapiProxyToProxyConfig(std::move(proxy_config))
                            .GetDictionary()
                            .Clone());
}

void NetworkSettingsServiceAsh::ClearExtensionProxy() {
  ClearProxyPrefFromUserStore();
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
  PrefService* pref_service = GetPrimaryLoggedInUserProfilePrefs();
  DCHECK(pref_service);

  const PrefService::Preference* pref =
      pref_service->FindPreference(proxy_config::prefs::kProxy);

  if (pref && pref->IsManaged()) {
    // If the kProxy pref is set via a user policy, it is stored in the managed
    // store and has priority over the extension set proxy from Lacros (which in
    // Ash is stored in the user store). Removing the preference from the user
    // store will ensure that, if the extension gets removed from Lacros while
    // the proxy policy is active, the user doesn't get stuck with the old proxy
    // value from the user store.
    // Note that clearing the proxy pref from the user store will not affect the
    // value of the proxy pref set by the policy in the managed store, nor will
    // it affect the value of the proxy pref set by the Lacros extension in the
    // Lacros extension store.
    ClearProxyPrefFromUserStore();
  }
  DetermineEffectiveProxy();
}

void NetworkSettingsServiceAsh::ClearProxyPrefFromUserStore() {
  extension_controlling_proxy_.reset();
  PrefService* pref_service = GetPrimaryLoggedInUserProfilePrefs();
  DCHECK(pref_service);
  pref_service->ClearPref(ash::prefs::kLacrosProxyControllingExtension);
  pref_service->ClearPref(proxy_config::prefs::kProxy);
}

void NetworkSettingsServiceAsh::DetermineEffectiveProxy() {
  PrefService* pref_service = GetPrimaryLoggedInUserProfilePrefs();
  if (!pref_service)
    return;
  crosapi::mojom::ProxyConfigPtr new_proxy_config = ProxyConfigToCrosapiProxy(
      ash::ProxyConfigServiceImpl::GetActiveProxyConfigDictionary(pref_service,
                                                                  local_state_)
          .get(),
      cached_wpad_url_);

  new_proxy_config->extension = extension_controlling_proxy_.Clone();

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

void NetworkSettingsServiceAsh::OnProfileAdded(Profile* profile) {
  if (!ash::ProfileHelper::IsPrimaryProfile(profile) ||
      !GetPrimaryLoggedInUserProfilePrefs()) {
    // Primary profile pref store not available.
    return;
  }
  if (!crosapi::browser_util::IsLacrosEnabled()) {
    ClearExtensionProxy();
  }
}

void NetworkSettingsServiceAsh::OnProfileManagerDestroying() {
  profile_prefs_registrar_.reset();
  if (!profile_manager_)
    return;
  profile_manager_->RemoveObserver(this);
  profile_manager_ = nullptr;
}

}  // namespace crosapi
