// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/mojom/hid_preserving_bluetooth_state_controller.mojom.h"
#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/bluetooth/bluetooth_device_list_controller.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class UnifiedSystemTrayController;

// This class encapsulates the logic to update the detailed Bluetooth device
// page within the quick settings and translate user interaction with the
// detailed view into Bluetooth state changes.
class ASH_EXPORT BluetoothDetailedViewController
    : public DetailedViewController,
      public bluetooth_config::mojom::SystemPropertiesObserver,
      public BluetoothDetailedView::Delegate {
 public:
  explicit BluetoothDetailedViewController(
      UnifiedSystemTrayController* tray_controller);
  BluetoothDetailedViewController(const BluetoothDetailedViewController&) =
      delete;
  BluetoothDetailedViewController& operator=(
      const BluetoothDetailedViewController&) = delete;
  ~BluetoothDetailedViewController() override;

 protected:
  using PairedBluetoothDevicePropertiesPtrs =
      std::vector<bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr>;

 private:
  // DetailedViewController:
  std::unique_ptr<views::View> CreateView() override;
  std::u16string GetAccessibleName() const override;
  void ShutDown() override;

  // bluetooth_config::mojom::SystemPropertiesObserver:
  void OnPropertiesUpdated(bluetooth_config::mojom::BluetoothSystemPropertiesPtr
                               properties) override;

  // BluetoothDetailedView::Delegate:
  void OnToggleClicked(bool new_state) override;
  void OnPairNewDeviceRequested() override;
  void OnDeviceListItemSelected(
      const bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr& device)
      override;

  // Used to update |view_| and |device_list_controller_| when the cached
  // Bluetooth state has changed.
  void BluetoothEnabledStateChanged();

  // Adds fake devices for manual testing to `previously_connected_devices_`
  // and `connected_devices_`.
  void AddFakeBluetoothDevices();

  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  mojo::Remote<bluetooth_config::mojom::CrosBluetoothConfig>
      remote_cros_bluetooth_config_;
  mojo::Receiver<bluetooth_config::mojom::SystemPropertiesObserver>
      cros_system_properties_observer_receiver_{this};

  mojo::Remote<mojom::HidPreservingBluetoothStateController>
      remote_hid_preserving_bluetooth_;

  bluetooth_config::mojom::BluetoothSystemState system_state_ =
      bluetooth_config::mojom::BluetoothSystemState::kUnavailable;
  raw_ptr<BluetoothDetailedView, DanglingUntriaged> view_ = nullptr;
  std::unique_ptr<BluetoothDeviceListController> device_list_controller_;
  PairedBluetoothDevicePropertiesPtrs connected_devices_;
  PairedBluetoothDevicePropertiesPtrs previously_connected_devices_;
  raw_ptr<UnifiedSystemTrayController> tray_controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_CONTROLLER_H_
