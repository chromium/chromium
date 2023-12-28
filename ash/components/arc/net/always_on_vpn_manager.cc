// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/always_on_vpn_manager.h"

#include <string>

#include "ash/components/arc/arc_prefs.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace arc {

AlwaysOnVpnManager::AlwaysOnVpnManager(PrefService* pref_service,
                                       bool delay_lockdown_until_vpn_connected)
    : delay_lockdown_until_vpn_connected_(delay_lockdown_until_vpn_connected) {
  registrar_.Init(pref_service);
  registrar_.Add(prefs::kAlwaysOnVpnPackage,
                 base::BindRepeating(&AlwaysOnVpnManager::OnPrefChanged,
                                     base::Unretained(this)));
  registrar_.Add(prefs::kAlwaysOnVpnLockdown,
                 base::BindRepeating(&AlwaysOnVpnManager::OnPrefChanged,
                                     base::Unretained(this)));
  // update once with values before we started listening
  OnPrefChanged();
}

AlwaysOnVpnManager::~AlwaysOnVpnManager() {
  std::string package =
      registrar_.prefs()->GetString(prefs::kAlwaysOnVpnPackage);
  bool lockdown = registrar_.prefs()->GetBoolean(prefs::kAlwaysOnVpnLockdown);
  if (lockdown && !package.empty()) {
    ash::NetworkHandler::Get()
        ->network_configuration_handler()
        ->SetManagerProperty(shill::kAlwaysOnVpnPackageProperty,
                             base::Value(std::string()));
  }
  registrar_.RemoveAll();
}

void AlwaysOnVpnManager::SetDelayLockdownUntilVpnConnectedState(bool enabled) {
  delay_lockdown_until_vpn_connected_ = enabled;
  OnPrefChanged();
}

base::WeakPtr<AlwaysOnVpnManager> AlwaysOnVpnManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AlwaysOnVpnManager::OnPrefChanged() {
  std::string always_on_vpn_package;
  bool lockdown = registrar_.prefs()->GetBoolean(prefs::kAlwaysOnVpnLockdown);

  // Only enforce lockdown mode for user traffic on the platform if lockdown
  // mode is enabled for an Android VPN and the browser user traffic is not
  // restricted by the AlwaysOnVpnPreConnectUrlAllowlist preference.
  // Shill enables lockdown mode if the Always-on VPN package name property is
  // set, i.e Shill always assumes that lockdown mode is enabled if Android
  // Always-on is set.
  // TODO(b/188864779): Add a new Always-on VPN Shill property for Android VPN
  // which can be used in Shill to distinguish when lockdown is enabled or not.
  if (lockdown && !delay_lockdown_until_vpn_connected_) {
    always_on_vpn_package =
        registrar_.prefs()->GetString(prefs::kAlwaysOnVpnPackage);
  }
  ash::NetworkHandler::Get()
      ->network_configuration_handler()
      ->SetManagerProperty(shill::kAlwaysOnVpnPackageProperty,
                           base::Value(always_on_vpn_package));
}

}  // namespace arc
