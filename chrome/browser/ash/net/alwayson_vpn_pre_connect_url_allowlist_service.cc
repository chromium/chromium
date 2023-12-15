// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/alwayson_vpn_pre_connect_url_allowlist_service.h"

#include "ash/components/arc/arc_prefs.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
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

  enforce_alwayson_pre_connect_url_allowlist_ =
      is_alwayson_vpn_enabled && !is_vpn_connected &&
      !pre_vpn_connect_url_allowlist.empty();
}

void AlwaysOnVpnPreConnectUrlAllowlistService::OnShuttingDown() {
  network_state_handler_observer_.Reset();
  profile_pref_change_registrar_.RemoveAll();
}
}  // namespace ash
