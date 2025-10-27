// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_ALWAYSON_VPN_PRE_CONNECT_URL_ALLOWLIST_SERVICE_H_
#define CHROME_BROWSER_ASH_NET_ALWAYSON_VPN_PRE_CONNECT_URL_ALLOWLIST_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PolicyBlocklistService;
class PrefService;

namespace arc {
class AlwaysOnVpnManager;
}

namespace ash {

// This service determines if the browser can have (limited) network
// connectivity while an Always-on VPN is configured in lockdown mode on the
// device but not yet connected.
// The service sets the local-state state preference
// enterprise::kEnforceAlwaysOnVpnPreConnectUrlAllowlist accordingly.
class AlwaysOnVpnPreConnectUrlAllowlistService
    : public KeyedService,
      public ash::NetworkStateHandlerObserver {
 public:
  // LINT.IfChange(Deps)
  AlwaysOnVpnPreConnectUrlAllowlistService(
      PrefService* pref_service,
      PolicyBlocklistService* policy_blocklist_service);
  // LINT.ThenChange(//chrome/browser/ash/net/alwayson_vpn_pre_connect_url_allowlist_service_factory.cc:Deps)

  AlwaysOnVpnPreConnectUrlAllowlistService(
      const AlwaysOnVpnPreConnectUrlAllowlistService&) = delete;
  AlwaysOnVpnPreConnectUrlAllowlistService& operator=(
      const AlwaysOnVpnPreConnectUrlAllowlistService&) = delete;
  ~AlwaysOnVpnPreConnectUrlAllowlistService() override;

  bool enforce_alwayson_pre_connect_url_allowlist() {
    return enforce_alwayson_pre_connect_url_allowlist_;
  }

  // Sets a pointer to the `AlwaysOnVpnManager` instance. The
  // `AlwaysOnVpnPreConnectUrlAllowlistService` will use the pointer to instruct
  // the `AlwaysOnVpnManager` instance to delay or apply the VPN lockdown mode,
  // depending on the network and pref settings (see the
  // `DeterminePreConnectUrlAllowlistEnforcement` method).
  void SetAlwaysOnVpnManager(
      base::WeakPtr<arc::AlwaysOnVpnManager> always_on_vpn_manager);

 private:
  // NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const ash::NetworkState* network) override;
  void OnShuttingDown() override;

  // Takes into account all the factors which allow the
  // `policy::policy_prefs::kAlwaysOnVpnPreConnectUrlAllowlist` to be
  // enforced on the device and updates the local state pref
  // `policy::policy_prefs::kEnforceAlwaysOnVpnPreConnectUrlAllowlist`
  // accordingly.
  void DeterminePreConnectUrlAllowlistEnforcement();

  void OnPrefChanged();

  PrefChangeRegistrar profile_pref_change_registrar_;

  bool enforce_alwayson_pre_connect_url_allowlist_ = false;

  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<PolicyBlocklistService> policy_blocklist_service_;

  base::WeakPtr<arc::AlwaysOnVpnManager> always_on_vpn_manager_;

  base::ScopedObservation<ash::NetworkStateHandler,
                          ash::NetworkStateHandlerObserver>
      network_state_handler_observer_{this};
};

}  // namespace ash
#endif  // CHROME_BROWSER_ASH_NET_ALWAYSON_VPN_PRE_CONNECT_URL_ALLOWLIST_SERVICE_H_
