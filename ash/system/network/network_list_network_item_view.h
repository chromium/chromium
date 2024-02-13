// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_NETWORK_ITEM_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_NETWORK_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/network/network_icon_animation_observer.h"
#include "ash/system/network/network_list_item_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"

namespace ash {

class ViewClickListener;

// This class encapsulates the logic of configuring the view shown for a single
// network (Mobile, Wifi and Ethernet) in the detailed Network page within the
// quick settings.
class ASH_EXPORT NetworkListNetworkItemView
    : public NetworkListItemView,
      public network_icon::AnimationObserver {
  METADATA_HEADER(NetworkListNetworkItemView, NetworkListItemView)

 public:
  explicit NetworkListNetworkItemView(ViewClickListener* listener);
  NetworkListNetworkItemView(const NetworkListNetworkItemView&) = delete;
  NetworkListNetworkItemView& operator=(const NetworkListNetworkItemView&) =
      delete;
  ~NetworkListNetworkItemView() override;

  // NetworkListItemView:
  void UpdateViewForNetwork(
      const chromeos::network_config::mojom::NetworkStatePropertiesPtr&
          network_properties) override;

 private:
  friend class NetworkListNetworkItemViewTest;

  // network_icon::AnimationObserver:
  void NetworkIconChanged() override;

  // ash::NetworkListItemView
  void OnThemeChanged() override;

  void SetupCellularSubtext();
  void SetupNetworkSubtext();
  void UpdateDisabledTextColor();
  void AddPowerStatusView();
  void AddPolicyView();
  std::u16string GenerateAccessibilityLabel(const std::u16string& label);
  std::u16string GenerateAccessibilityDescription();
  std::u16string GenerateAccessibilityDescriptionForEthernet(
      const std::u16string& connection_status);
  std::u16string GenerateAccessibilityDescriptionForWifi(
      const std::u16string& connection_status,
      int signal_strength);
  std::u16string GenerateAccessibilityDescriptionForCellular(
      const std::u16string& connection_status,
      int signal_strength);
  std::u16string GenerateAccessibilityDescriptionForTether(
      const std::u16string& connection_status,
      int signal_strength);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_NETWORK_ITEM_VIEW_H_