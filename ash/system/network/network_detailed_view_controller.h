// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_NETWORK_NETWORK_DETAILED_VIEW_CONTROLLER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/system/network/network_detailed_network_view.h"
#include "ash/system/network/network_detailed_view.h"
#include "ash/system/network/network_list_view_controller.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// This class encapsulates the logic to update the detailed Network device
// page within the quick settings and translate user interaction with the
// detailed view into Network state changes.
class ASH_EXPORT NetworkDetailedViewController
    : public DetailedViewController,
      public NetworkDetailedNetworkView::Delegate {
 public:
  explicit NetworkDetailedViewController(
      UnifiedSystemTrayController* tray_controller);
  NetworkDetailedViewController(const NetworkDetailedViewController&) = delete;
  NetworkDetailedViewController& operator=(
      const NetworkDetailedViewController&) = delete;
  ~NetworkDetailedViewController() override;

  // DetailedViewControllerBase:
  views::View* CreateView() override;
  std::u16string GetAccessibleName() const override;

 private:
  // NetworkDetailedView::Delegate:
  void OnNetworkListItemSelected(
      const chromeos::network_config::mojom::NetworkStatePropertiesPtr& network)
      override;

  // NetworkDetailedNetworkView::Delegate:
  void OnMobileToggleClicked(bool new_state) override;
  void OnWifiToggleClicked(bool new_state) override;

  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  NetworkDetailedNetworkView* network_detailed_view_ = nullptr;
  std::unique_ptr<NetworkListViewController> network_list_view_controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_DETAILED_VIEW_CONTROLLER_H_
