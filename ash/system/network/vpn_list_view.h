// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_VPN_LIST_VIEW_H_
#define ASH_SYSTEM_NETWORK_VPN_LIST_VIEW_H_

#include <map>
#include <string>

#include "ash/system/network/network_state_list_detailed_view.h"
#include "ash/system/network/vpn_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"

class PrefRegistrySimple;

namespace views {
class View;
}

namespace ash {
namespace tray {

// A list of VPN providers and networks that shows VPN providers and networks in
// a hierarchical layout, allowing the user to see at a glance which provider a
// network belongs to. The only exception is the currently connected or
// connecting network, which is detached from its provider and moved to the top.
// If there is a connected network, a disconnect button is shown next to its
// name.
//
// Disconnected networks are arranged in shill's priority order within each
// provider and the providers are arranged in the order of their highest
// priority network. Clicking on a disconnected network triggers a connection
// attempt. Clicking on the currently connected or connecting network shows its
// configuration dialog. Clicking on a provider shows the provider's "add
// network" dialog.
class VPNListView : public NetworkStateListDetailedView,
                    public VpnList::Observer {
 public:
  using VpnProviderPtr = chromeos::network_config::mojom::VpnProviderPtr;

  VPNListView(DetailedViewDelegate* delegate, LoginStatus login);
  ~VPNListView() override;

  // Make following functions publicly accessible for VPNListNetworkEntry.
  using NetworkStateListDetailedView::SetupConnectedScrollListItem;
  using NetworkStateListDetailedView::SetupConnectingScrollListItem;

  // NetworkStateListDetailedView:
  void UpdateNetworkList() override;
  bool IsNetworkEntry(views::View* view, std::string* guid) const override;

  // VpnList::Observer:
  void OnVpnProvidersChanged() override;

  // See Shell::RegisterProfilePrefs().
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // views::View:
  const char* GetClassName() const override;

 private:
  using NetworkStateList =
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>;
  void OnGetNetworkStateList(NetworkStateList networks);

  // Adds a network to the list.
  void AddNetwork(
      const chromeos::network_config::mojom::NetworkStateProperties* network);

  // Adds the VPN provider identified by |vpn_provider| to the list, along with
  // any networks that may belong to this provider. Takes ownership of
  // |vpn_provider|.
  void AddProviderAndNetworks(VpnProviderPtr vpn_provider,
                              const NetworkStateList& networks);

  // Finds VPN provider from |providers| that matches given |network|. Then adds
  // the VPN provider along with any networks that belong to this provider. Will
  // also remove the match from |providers| to avoid showing duplicate provider
  // entry in VPN list view.
  // Returns true if finds a match, returns false otherwise.
  bool ProcessProviderForNetwork(
      const chromeos::network_config::mojom::NetworkStateProperties* network,
      const NetworkStateList& networks,
      std::vector<VpnProviderPtr>* providers);

  // Adds all available VPN providers and networks to the list.
  void AddProvidersAndNetworks(const NetworkStateList& networks);

  // A mapping from each VPN provider's list entry to the provider.
  std::map<const views::View* const, VpnProviderPtr> provider_view_map_;

  // A mapping from each network's list entry to the network's guid.
  std::map<const views::View* const, std::string> network_view_guid_map_;

  // Whether the list is currently empty (i.e., the next entry added will become
  // the topmost entry).
  bool list_empty_ = true;

  base::WeakPtrFactory<VPNListView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VPNListView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_VPN_LIST_VIEW_H_
