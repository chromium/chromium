// Copyright 2022 The Chromium Authors
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
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// This class encapsulates the logic to update the detailed Network device
// page within the quick settings and translate user interaction with the
// detailed view into Network state changes.
class ASH_EXPORT NetworkDetailedViewController
    : public DetailedViewController,
      public NetworkDetailedNetworkView::Delegate,
      public bluetooth_config::mojom::SystemPropertiesObserver {
 public:
  explicit NetworkDetailedViewController(
      UnifiedSystemTrayController* tray_controller);
  NetworkDetailedViewController(const NetworkDetailedViewController&) = delete;
  NetworkDetailedViewController& operator=(
      const NetworkDetailedViewController&) = delete;
  ~NetworkDetailedViewController() override;

  // DetailedViewController:
  std::unique_ptr<views::View> CreateView() override;
  std::u16string GetAccessibleName() const override;
  void ShutDown() override;

 private:
  // NetworkDetailedView::Delegate:
  void OnNetworkListItemSelected(
      const chromeos::network_config::mojom::NetworkStatePropertiesPtr& network)
      override;

  // NetworkDetailedNetworkView::Delegate:
  void OnMobileToggleClicked(bool new_state) override;
  void OnWifiToggleClicked(bool new_state) override;

  // bluetooth_config::mojom::SystemPropertiesObserver:
  void OnPropertiesUpdated(bluetooth_config::mojom::BluetoothSystemPropertiesPtr
                               properties) override;

  const raw_ptr<TrayNetworkStateModel> model_;

  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  bool waiting_to_initialize_bluetooth_ = false;

  mojo::Remote<bluetooth_config::mojom::CrosBluetoothConfig>
      remote_cros_bluetooth_config_;
  mojo::Receiver<bluetooth_config::mojom::SystemPropertiesObserver>
      cros_system_properties_observer_receiver_{this};

  bluetooth_config::mojom::BluetoothSystemState bluetooth_system_state_ =
      bluetooth_config::mojom::BluetoothSystemState::kUnavailable;

  raw_ptr<NetworkDetailedNetworkView, DanglingUntriaged>
      network_detailed_view_ = nullptr;
  std::unique_ptr<NetworkListViewController> network_list_view_controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_DETAILED_VIEW_CONTROLLER_H_
