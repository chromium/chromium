// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/unified_bluetooth_detailed_view_controller.h"

#include <set>
#include <string>
#include <utility>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/stl_util.h"

using device::mojom::BluetoothSystem;
using device::mojom::BluetoothDeviceInfo;
using device::mojom::BluetoothDeviceInfoPtr;

namespace ash {

namespace {

// Updates bluetooth device |device| in the |list|. If it is new, append to the
// end of the |list|; otherwise, keep it at the same place, but update the data
// with new device info provided by |device|.
void UpdateBluetoothDeviceListHelper(BluetoothDeviceList* list,
                                     BluetoothDeviceInfoPtr new_device) {
  for (auto& device : *list) {
    if (device->address == new_device->address) {
      device.Swap(&new_device);
      return;
    }
  }

  list->push_back(std::move(new_device));
}

// Removes the obsolete BluetoothDevices from |list|, if they are not in the
// |new_device_address_list|.
void RemoveObsoleteBluetoothDevicesFromList(
    BluetoothDeviceList* device_list,
    const std::set<BluetoothAddress>& new_device_address_list) {
  base::EraseIf(*device_list, [&new_device_address_list](
                                  const BluetoothDeviceInfoPtr& info) {
    return !base::Contains(new_device_address_list, info->address);
  });
}

}  // namespace

UnifiedBluetoothDetailedViewController::UnifiedBluetoothDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {
  Shell::Get()->tray_bluetooth_helper()->AddObserver(this);
}

UnifiedBluetoothDetailedViewController::
    ~UnifiedBluetoothDetailedViewController() {
  // Stop discovering bluetooth devices when exiting BT detailed view.
  TrayBluetoothHelper* helper = Shell::Get()->tray_bluetooth_helper();
  if (!helper)
    return;

  helper->RemoveObserver(this);

  if (helper->HasBluetoothDiscoverySession()) {
    helper->StopBluetoothDiscovering();
  }
}

views::View* UnifiedBluetoothDetailedViewController::CreateView() {
  DCHECK(!view_);
  view_ = new tray::BluetoothDetailedView(
      detailed_view_delegate_.get(),
      Shell::Get()->session_controller()->login_status());
  OnBluetoothSystemStateChanged();
  return view_;
}

void UnifiedBluetoothDetailedViewController::OnBluetoothSystemStateChanged() {
  auto* helper = Shell::Get()->tray_bluetooth_helper();
  const BluetoothSystem::State bluetooth_state = helper->GetBluetoothState();

  if (bluetooth_state == BluetoothSystem::State::kPoweredOn) {
    // If Bluetooth was just turned on, start discovering.
    Shell::Get()->tray_bluetooth_helper()->StartBluetoothDiscovering();
  }

  UpdateDeviceListAndUI();
}

void UnifiedBluetoothDetailedViewController::OnBluetoothScanStateChanged() {
  UpdateDeviceListAndUI();
}

void UnifiedBluetoothDetailedViewController::OnBluetoothDeviceListChanged() {
  UpdateDeviceListAndUI();
}

void UnifiedBluetoothDetailedViewController::UpdateDeviceListAndUI() {
  UpdateBluetoothDeviceList();

  auto* helper = Shell::Get()->tray_bluetooth_helper();
  bool bluetooth_on =
      helper->GetBluetoothState() == BluetoothSystem::State::kPoweredOn;

  // Update toggle.
  view_->SetToggleIsOn(bluetooth_on);

  // Update loading indicator.
  if (helper->HasBluetoothDiscoverySession())
    view_->ShowLoadingIndicator();
  else
    view_->HideLoadingIndicator();

  // Update scroll list or show "BT disabled" panel
  if (bluetooth_on) {
    view_->HideBluetoothDisabledPanel();
    view_->UpdateDeviceScrollList(connected_devices_, connecting_devices_,
                                  paired_not_connected_devices_,
                                  discovered_not_paired_devices_);

    return;
  }

  // If Bluetooth is disabled, show a panel which only indicates that it is
  // disabled, instead of the scroller with Bluetooth devices.
  view_->ShowBluetoothDisabledPanel();
}

void UnifiedBluetoothDetailedViewController::UpdateBluetoothDeviceList() {
  std::set<BluetoothAddress> new_connecting_devices;
  std::set<BluetoothAddress> new_connected_devices;
  std::set<BluetoothAddress> new_paired_not_connected_devices;
  std::set<BluetoothAddress> new_discovered_not_paired_devices;

  for (const auto& device :
       Shell::Get()->tray_bluetooth_helper()->GetAvailableBluetoothDevices()) {
    auto device_clone = device->Clone();
    if (device->connection_state ==
        BluetoothDeviceInfo::ConnectionState::kConnecting) {
      new_connecting_devices.insert(device->address);
      UpdateBluetoothDeviceListHelper(&connecting_devices_,
                                      std::move(device_clone));
    } else if (device->connection_state ==
                   BluetoothDeviceInfo::ConnectionState::kConnected &&
               device->is_paired) {
      new_connected_devices.insert(device->address);
      UpdateBluetoothDeviceListHelper(&connected_devices_,
                                      std::move(device_clone));
    } else if (device->is_paired) {
      new_paired_not_connected_devices.insert(device->address);
      UpdateBluetoothDeviceListHelper(&paired_not_connected_devices_,
                                      std::move(device_clone));
    } else {
      new_discovered_not_paired_devices.insert(device->address);
      UpdateBluetoothDeviceListHelper(&discovered_not_paired_devices_,
                                      std::move(device_clone));
    }
  }
  RemoveObsoleteBluetoothDevicesFromList(&connecting_devices_,
                                         new_connecting_devices);
  RemoveObsoleteBluetoothDevicesFromList(&connected_devices_,
                                         new_connected_devices);
  RemoveObsoleteBluetoothDevicesFromList(&paired_not_connected_devices_,
                                         new_paired_not_connected_devices);
  RemoveObsoleteBluetoothDevicesFromList(&discovered_not_paired_devices_,
                                         new_discovered_not_paired_devices);
}

}  // namespace ash
