// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/alwayson_vpn_pre_connect_url_allowlist_service.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/net/always_on_vpn_manager.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/network_settings_service_ash.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

AlwaysOnVpnPreConnectUrlAllowlistService::
    AlwaysOnVpnPreConnectUrlAllowlistService(
        content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  network_state_handler_observer_.Observe(
      ash::NetworkHandler::Get()->network_state_handler());

  PrefService* pref_service =
      user_prefs::UserPrefs::Get(browser_context_.get());

  // TODO(b/188864779, acostinas): After the ARC legacy migration is completed,
  // monitor the Always-on VPN state from the network profile property instead
  // of the user pref.
  profile_pref_change_registrar_.Init(pref_service);
  profile_pref_change_registrar_.Add(
      arc::prefs::kAlwaysOnVpnLockdown,
      base::BindRepeating(
          &AlwaysOnVpnPreConnectUrlAllowlistService::OnPrefChanged,
          base::Unretained(this)));
  profile_pref_change_registrar_.Add(
      policy::policy_prefs::kAlwaysOnVpnPreConnectUrlAllowlist,
      base::BindRepeating(
          &AlwaysOnVpnPreConnectUrlAllowlistService::OnPrefChanged,
          base::Unretained(this)));
  DeterminePreConnectUrlAllowlistEnforcement();
}

AlwaysOnVpnPreConnectUrlAllowlistService::
    ~AlwaysOnVpnPreConnectUrlAllowlistService() {
  network_state_handler_observer_.Reset();
  profile_pref_change_registrar_.RemoveAll();
}

void AlwaysOnVpnPreConnectUrlAllowlistService::SetAlwaysOnVpnManager(
    base::WeakPtr<arc::AlwaysOnVpnManager> always_on_vpn_manager) {
  always_on_vpn_manager_ = std::move(always_on_vpn_manager);
}

void AlwaysOnVpnPreConnectUrlAllowlistService::OnPrefChanged() {
  DeterminePreConnectUrlAllowlistEnforcement();
}

void AlwaysOnVpnPreConnectUrlAllowlistService::DefaultNetworkChanged(
    const ash::NetworkState* network) {
  DeterminePreConnectUrlAllowlistEnforcement();
}

void AlwaysOnVpnPreConnectUrlAllowlistService::
    DeterminePreConnectUrlAllowlistEnforcement() {
  // TODO(b/188864779, acostinas): After the ARC legacy migration is completed,
  // read the Always-on VPN state from the network profile property instead
  // of the user pref.
  bool is_alwayson_vpn_enabled =
      profile_pref_change_registrar_.prefs()->GetBoolean(
          arc::prefs::kAlwaysOnVpnLockdown);

  const ash::NetworkState* network =
      ash::NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  bool is_vpn_connected = network &&
                          network->GetNetworkTechnologyType() ==
                              ash::NetworkState::NetworkTechnologyType::kVPN &&
                          network->connection_state() == shill::kStateOnline;

  const base::Value::List& pre_vpn_connect_url_allowlist =
      profile_pref_change_registrar_.prefs()->GetList(
          policy::policy_prefs::kAlwaysOnVpnPreConnectUrlAllowlist);

  const bool use_vpn_preconnect_list = is_alwayson_vpn_enabled &&
                                       !is_vpn_connected &&
                                       !pre_vpn_connect_url_allowlist.empty();

  if (enforce_alwayson_pre_connect_url_allowlist_ == use_vpn_preconnect_list) {
    return;
  }

  enforce_alwayson_pre_connect_url_allowlist_ = use_vpn_preconnect_list;

  // TODO(b/188864779, acostinas): After the ARC legacy migration is completed,
  // replace the call to the `arc::AlwaysOnVpnManager` with a shill network
  // profile property. Shill needs to be aware of this value when it evaluates
  // whether to set the Always-on VPN lockdown mode for the device.
  if (always_on_vpn_manager_) {
    always_on_vpn_manager_->SetDelayLockdownUntilVpnConnectedState(
        enforce_alwayson_pre_connect_url_allowlist_);
  }
  PolicyBlocklistService* service =
      PolicyBlocklistFactory::GetForBrowserContext(browser_context_.get());
  service->SetAlwaysOnVpnPreConnectUrlAllowlistEnforced(
      enforce_alwayson_pre_connect_url_allowlist_);

  // Notify the Lacros browser instances (via the `NetworkSettingsService` mojo
  // crosapi) that user traffic should be restricted to the URL filters
  // configured in the AlwaysOnVpnPreConnectUrlAllowlist policy.
  if (crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->network_settings_service_ash()
        ->SetAlwaysOnVpnPreConnectUrlAllowlistEnforced(
            enforce_alwayson_pre_connect_url_allowlist_);
  }
}

void AlwaysOnVpnPreConnectUrlAllowlistService::OnShuttingDown() {
  network_state_handler_observer_.Reset();
  profile_pref_change_registrar_.RemoveAll();
}
}  // namespace ash
