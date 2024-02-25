// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_NET_ALWAYS_ON_VPN_MANAGER_H_
#define ASH_COMPONENTS_ARC_NET_ALWAYS_ON_VPN_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace arc {

class AlwaysOnVpnManager {
 public:
  AlwaysOnVpnManager(PrefService* pref_service,
                     bool delay_lockdown_until_vpn_connected);

  AlwaysOnVpnManager(const AlwaysOnVpnManager&) = delete;
  AlwaysOnVpnManager& operator=(const AlwaysOnVpnManager&) = delete;

  ~AlwaysOnVpnManager();

  // Calling this method with `enabled` = true will prevent VPN lockdown to be
  // configured on Chrome OS until the Always-on VPN is connected.q
  void SetDelayLockdownUntilVpnConnectedState(bool enabled);

  base::WeakPtr<AlwaysOnVpnManager> GetWeakPtr();

 private:
  // Callback for the registrar
  void OnPrefChanged();

  bool delay_lockdown_until_vpn_connected_ = false;

  PrefChangeRegistrar registrar_;

  base::WeakPtrFactory<AlwaysOnVpnManager> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_NET_ALWAYS_ON_VPN_MANAGER_H_
