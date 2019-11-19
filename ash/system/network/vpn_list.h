// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_VPN_LIST_H_
#define ASH_SYSTEM_NETWORK_VPN_LIST_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"

namespace ash {

class TrayNetworkStateModel;

// This delegate provides UI code in ash, e.g. |VPNListView|, with access to the
// list of VPN providers enabled in the primary user's profile. The delegate
// furthermore allows the UI code to request that a VPN provider show its "add
// network" dialog and allows UI code to request to launch Arc VPN provider.
class ASH_EXPORT VpnList : public TrayNetworkStateObserver {
 public:
  using VpnProvider = chromeos::network_config::mojom::VpnProvider;
  using VpnProviderPtr = chromeos::network_config::mojom::VpnProviderPtr;
  using VpnType = chromeos::network_config::mojom::VpnType;

  // An observer that is notified whenever the list of VPN providers enabled in
  // the primary user's profile changes.
  class Observer {
   public:
    virtual void OnVpnProvidersChanged() = 0;

   protected:
    virtual ~Observer();

   private:
    DISALLOW_ASSIGN(Observer);
  };

  explicit VpnList(TrayNetworkStateModel* model);
  ~VpnList() override;

  const std::vector<VpnProviderPtr>& extension_vpn_providers() {
    return extension_vpn_providers_;
  }
  const std::vector<VpnProviderPtr>& arc_vpn_providers() {
    return arc_vpn_providers_;
  }

  // Returns |true| if at least one third-party VPN provider or at least one Arc
  // VPN provider is enabled in the primary user's profile, in addition to the
  // built-in OpenVPN/L2TP provider.
  bool HaveExtensionOrArcVpnProviders() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // TrayNetworkStateObserver
  void VpnProvidersChanged() override;

  void SetVpnProvidersForTest(std::vector<VpnProviderPtr> providers);

 private:
  void OnGetVpnProviders(std::vector<VpnProviderPtr> providers);

  // Notify observers that the list of VPN providers enabled in the primary
  // user's profile has changed.
  void NotifyObservers();

  // Adds the built-in OpenVPN/L2TP provider to |extension_vpn_providers_|.
  void AddBuiltInProvider();

  TrayNetworkStateModel* model_;

  // Cache of VPN providers, including the built-in OpenVPN/L2TP provider and
  // other providers added by extensions in the primary user's profile.
  std::vector<VpnProviderPtr> extension_vpn_providers_;

  // Cache of Arc VPN providers. Will be sorted based on last launch time when
  // creating vpn list view.
  std::vector<VpnProviderPtr> arc_vpn_providers_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(VpnList);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_VPN_LIST_H_
