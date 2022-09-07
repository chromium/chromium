// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_VIEW_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/system/network/network_icon_animation_observer.h"
#include "ash/system/network/network_info.h"
#include "ash/system/network/network_state_list_detailed_view.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-forward.h"

namespace views {
class Separator;
class View;
}  // namespace views

namespace ash {

class HoverHighlightView;
class NetworkSectionHeaderView;
class MobileSectionHeaderView;
class TrayInfoLabel;
class TriView;
class WifiSectionHeaderView;

// A list of available networks of a given type. This class is used for all
// network types except VPNs. For VPNs, see the |VPNList| class.
class NetworkListView : public NetworkStateListDetailedView,
                        public network_icon::AnimationObserver {
 public:
  NetworkListView(DetailedViewDelegate* delegate, LoginStatus login);

  NetworkListView(const NetworkListView&) = delete;
  NetworkListView& operator=(const NetworkListView&) = delete;

  ~NetworkListView() override;

  // NetworkStateListDetailedView:
  void UpdateNetworkList() override;
  bool IsNetworkEntry(views::View* view, std::string* guid) const override;

  // views::View:
  const char* GetClassName() const override;

 private:
  void OnGetNetworkStateList(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  // Refreshes a list of child views, updates |network_map_| and
  // |network_guid_map_| and performs layout making sure selected view if any is
  // scrolled into view.
  void UpdateNetworkListInternal();

  // Adds new or updates existing child views including header row and messages.
  // Returns a set of guids for the added network connections.
  std::unique_ptr<std::set<std::string>> UpdateNetworkListEntries();

  bool ShouldMobileDataSectionBeShown();

  // Creates the view which displays a warning message, if a VPN or proxy is
  // being used.
  TriView* CreateConnectionWarning();

  // Updates |view| with the information in |info|.
  void UpdateViewForNetwork(HoverHighlightView* view, const NetworkInfo& info);

  // Creates a battery icon next to the name of Tether networks indicating
  // the battery percentage of the mobile device that is being used as a
  // hotspot.
  views::View* CreatePowerStatusView(const NetworkInfo& info);

  // Creates a policy icon next to the name of managed networks indicating
  // that the network is managed by policy. Returns |nullptr| if the network is
  // not managed by policy.
  views::View* CreatePolicyView(const NetworkInfo& info);

  // Adds or updates child views representing the network connections when
  // |is_wifi| is matching the attribute of a network connection starting at
  // |child_index|. Returns a set of guids for the added network
  // connections.
  std::unique_ptr<std::set<std::string>> UpdateNetworkChildren(
      chromeos::network_config::mojom::NetworkType type,
      size_t child_index);
  void UpdateNetworkChild(size_t index, const NetworkInfo* info);

  // Reorders children of |scroll_content()| as necessary placing |view| at
  // |index|.
  void PlaceViewAtIndex(views::View* view, size_t index);

  // Creates an info label with text specified by |message_id| and adds it to
  // |scroll_content()| if necessary or updates the text and reorders the
  // |scroll_content()| placing the info label at |insertion_index|. When
  // |message_id| is zero removes the |*info_label_ptr| from the
  // |scroll_content()| and destroys it. |info_label_ptr| is an in/out parameter
  // and is only modified if the info label is created or destroyed.
  void UpdateInfoLabel(int message_id,
                       size_t insertion_index,
                       TrayInfoLabel** info_label_ptr);

  // Updates a cellular/Wi-Fi header row |view| and reorders the
  // |scroll_content()| placing the |view| at |child_index|. Returns the index
  // where the next child should be inserted, i.e., the index directly after the
  // last inserted child.
  size_t UpdateNetworkSectionHeader(
      chromeos::network_config::mojom::NetworkType type,
      bool enabled,
      size_t child_index,
      NetworkSectionHeaderView* view,
      views::Separator** separator_view);

  // network_icon::AnimationObserver:
  void NetworkIconChanged() override;

  // Returns true if the info should be updated to the view for network,
  // otherwise false.
  bool NeedUpdateViewForNetwork(const NetworkInfo& info) const;

  // Creates an accessibility label for given network.
  std::u16string GenerateAccessibilityLabel(const NetworkInfo& info);

  // Creates an accessibility description for the given network that includes
  // all details that are shown in the ui.
  std::u16string GenerateAccessibilityDescription(const NetworkInfo& info);

  bool needs_relayout_ = false;

  // Owned by the views heirarchy.
  TrayInfoLabel* mobile_status_message_ = nullptr;
  TrayInfoLabel* wifi_status_message_ = nullptr;
  MobileSectionHeaderView* mobile_header_view_ = nullptr;
  WifiSectionHeaderView* wifi_header_view_ = nullptr;
  views::Separator* mobile_separator_view_ = nullptr;
  views::Separator* wifi_separator_view_ = nullptr;
  TriView* connection_warning_ = nullptr;

  bool vpn_connected_ = false;
  bool wifi_has_networks_ = false;
  bool tether_has_networks_ = false;
  bool mobile_has_networks_ = false;

  // An owned list of network info.
  std::vector<std::unique_ptr<NetworkInfo>> network_list_;

  using NetworkMap = std::map<views::View*, std::string>;
  NetworkMap network_map_;

  // A map of network guids to their view.
  using NetworkGuidMap = std::map<std::string, HoverHighlightView*>;
  NetworkGuidMap network_guid_map_;

  // Save a map of network guids to their infos against current |network_list_|.
  using NetworkInfoMap = std::map<std::string, std::unique_ptr<NetworkInfo>>;
  NetworkInfoMap last_network_info_map_;

  base::WeakPtrFactory<NetworkListView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_VIEW_H_
