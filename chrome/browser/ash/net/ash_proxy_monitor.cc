// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/ash_proxy_monitor.h"

#include <stdint.h>

#include "ash/constants/ash_pref_names.h"
#include "base/logging.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
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

// TODO(acostinas,b/267158784) Remove this method after the new version of
// Lacros, which sets the proxy via the mojo Prefs service, is deployed to Ash
// (after minimum four releases to keep up with the version skew).
// When the proxy is set by an extension in the primary profile, the
// kProxy pref used to be stored in the user store. Now the pref is stored in
// the standalone browser store but we need to ensure that the old value is not
// lingering in the user store. This pref cannot be set by the user in the UI
// because proxies can only be set per network in Chrome OS.
void CleanupPrefFromUserStore(Profile* profile) {
  if (!profile) {
    return;
  }
  const PrefService::Preference* pref =
      profile->GetPrefs()->FindPreference(proxy_config::prefs::kProxy);
  DCHECK(pref) << "kProxy pref called before being registered.";

  if (!pref->HasUserSetting()) {
    return;
  }
  profile->GetPrefs()->ClearPref(proxy_config::prefs::kProxy);
}
}  // namespace

namespace ash {

AshProxyMonitor::ExtensionMetadata::ExtensionMetadata(const std::string& name,
                                                      const std::string& id,
                                                      bool can_be_disabled) {
  this->name = name;
  this->id = id;
  this->can_be_disabled = can_be_disabled;
}

AshProxyMonitor::AshProxyMonitor(PrefService* local_state,
                                 ProfileManager* profile_manager)
    : local_state_(local_state), profile_manager_(profile_manager) {
  if (profile_manager_) {
    profile_manager_observation_.Observe(profile_manager_);
  }
  // Uninitialized in unit_tests.
  if (ash::NetworkHandler::IsInitialized()) {
    network_state_handler_observer_.Observe(
        ash::NetworkHandler::Get()->network_state_handler());
  }
}

AshProxyMonitor::~AshProxyMonitor() = default;

void AshProxyMonitor::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AshProxyMonitor::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
void AshProxyMonitor::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(
      ash::prefs::kLacrosProxyControllingExtension);
}

void AshProxyMonitor::DefaultNetworkChanged(const ash::NetworkState* network) {
  if (!network) {
    cached_wpad_url_ = GURL();
    return;
  }
  if (cached_wpad_url_ != network->GetWebProxyAutoDiscoveryUrl()) {
    OnProxyChanged(network->GetWebProxyAutoDiscoveryUrl());
    return;
  }
  OnProxyChanged(std::nullopt);
}

void AshProxyMonitor::OnShuttingDown() {
  network_state_handler_observer_.Reset();
}

void AshProxyMonitor::OnProxyChanged(std::optional<GURL> wpad_url) {
  if (!primary_profile_) {
    return;
  }

  bool send_update = false;

  if (wpad_url.has_value() && wpad_url != cached_wpad_url_) {
    cached_wpad_url_ = wpad_url;
    send_update = true;
  }

  std::unique_ptr<ProxyConfigDictionary> proxy_dict =
      ProxyConfigServiceImpl::GetActiveProxyConfigDictionary(
          primary_profile_->GetPrefs(), local_state_);
  if (!proxy_dict) {
    proxy_dict = std::make_unique<ProxyConfigDictionary>(
        ProxyConfigDictionary::CreateDirect());
  }

  if (!cached_proxy_config_ ||
      cached_proxy_config_->GetDictionary() != proxy_dict->GetDictionary()) {
    cached_proxy_config_ = std::move(proxy_dict);
    send_update = true;
  }

  if (send_update) {
    NotifyObservers();
  }
}

void AshProxyMonitor::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnProxyChanged();
  }
}

void AshProxyMonitor::OnProfileAdded(Profile* profile) {
  if (!user_manager::UserManager::Get()->IsPrimaryUser(
          BrowserContextHelper::Get()->GetUserByBrowserContext(profile))) {
    return;
  }
  primary_profile_ = profile;

  CleanupPrefFromUserStore(primary_profile_);
  profile_prefs_registrar_ = std::make_unique<PrefChangeRegistrar>();
  profile_prefs_registrar_->Init(primary_profile_->GetPrefs());
  // This can be triggered by user policy changes or extensions running in Ash
  // or Lacros. The `wpad_url` can only be configured by DHCP and/or DNS
  // discovery methods.
  profile_prefs_registrar_->Add(
      proxy_config::prefs::kProxy,
      base::BindRepeating(&AshProxyMonitor::OnProxyChanged,
                          base::Unretained(this), /*wpad_url*/ std::nullopt));
}

void AshProxyMonitor::OnProfileMarkedForPermanentDeletion(Profile* profile) {
  if (!primary_profile_ || primary_profile_ != profile) {
    return;
  }
  profile_prefs_registrar_.reset();
  primary_profile_ = nullptr;
}

void AshProxyMonitor::OnProfileManagerDestroying() {
  profile_prefs_registrar_.reset();
  primary_profile_ = nullptr;

  if (!profile_manager_) {
    return;
  }
  profile_manager_observation_.Reset();
  profile_manager_ = nullptr;
}

void AshProxyMonitor::SetProfileForTesting(Profile* profile) {
  OnProfileAdded(profile);
}

ProxyConfigDictionary* AshProxyMonitor::GetLatestProxyConfig() const {
  return cached_proxy_config_.get();
}

GURL AshProxyMonitor::GetLatestWpadUrl() const {
  return cached_wpad_url_.value_or(GURL());
}

bool AshProxyMonitor::IsLacrosExtensionControllingProxy() const {
  if (!primary_profile_) {
    return false;
  }
  auto* pref =
      primary_profile_->GetPrefs()->FindPreference(proxy_config::prefs::kProxy);
  return pref && pref->IsStandaloneBrowserControlled();
}

void AshProxyMonitor::SetLacrosExtensionControllingProxyInfo(
    const std::string& name,
    const std::string& id,
    bool can_be_disabled) {
  DCHECK(primary_profile_) << "The primary profile is not initialized";
  CleanupPrefFromUserStore(primary_profile_);
  primary_profile_->GetPrefs()->SetDict(
      ash::prefs::kLacrosProxyControllingExtension,
      base::Value::Dict()
          .Set(kPrefExtensionNameKey, name)
          .Set(kPrefExtensionIdKey, id)
          .Set(kPrefExtensionCanDisabledKey, can_be_disabled));
  NotifyObservers();
}

void AshProxyMonitor::ClearLacrosExtensionControllingProxyInfo() {
  DCHECK(primary_profile_) << "The primary profile is not initialized";
  CleanupPrefFromUserStore(primary_profile_);
  primary_profile_->GetPrefs()->ClearPref(
      ash::prefs::kLacrosProxyControllingExtension);
  NotifyObservers();
}

std::optional<AshProxyMonitor::ExtensionMetadata>
AshProxyMonitor::GetLacrosExtensionControllingTheProxy() const {
  if (!IsLacrosExtensionControllingProxy()) {
    return std::nullopt;
  }
  const base::Value::Dict& dictionary = primary_profile_->GetPrefs()->GetDict(
      ash::prefs::kLacrosProxyControllingExtension);
  const std::string* extension_name =
      dictionary.FindString(kPrefExtensionNameKey);

  if (!extension_name) {
    // Ash received the proxy config from Lacros via the Prefs service but the
    // metadata of the extension, which is sent from Lacros via the
    // NetworkSettings service, is not yet received.
    return std::nullopt;
  }

  const std::string* extension_id = dictionary.FindString(kPrefExtensionIdKey);
  return ExtensionMetadata(
      extension_name ? *extension_name : std::string(),
      extension_id ? *extension_id : std::string(),
      dictionary.FindBool(kPrefExtensionCanDisabledKey).value_or(false));
}

}  // namespace ash
