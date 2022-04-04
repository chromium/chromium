// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/check.h"
#include "chromeos/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace ash {
namespace {
using chromeos::bluetooth_config::IsBluetoothEnabledOrEnabling;
using chromeos::bluetooth_config::mojom::AudioOutputCapability;
using chromeos::bluetooth_config::mojom::DeviceConnectionState;
}  // namespace

BluetoothDetailedViewController::BluetoothDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)),
      tray_controller_(tray_controller) {
  DCHECK(ash::features::IsBluetoothRevampEnabled());
  GetBluetoothConfigService(
      remote_cros_bluetooth_config_.BindNewPipeAndPassReceiver());
  remote_cros_bluetooth_config_->ObserveSystemProperties(
      cros_system_properties_observer_receiver_.BindNewPipeAndPassRemote());
}

BluetoothDetailedViewController::~BluetoothDetailedViewController() = default;

views::View* BluetoothDetailedViewController::CreateView() {
  DCHECK(!view_);
  std::unique_ptr<BluetoothDetailedView> bluetooth_detailed_view =
      BluetoothDetailedView::Factory::Create(detailed_view_delegate_.get(),
                                             /*delegate=*/this);
  view_ = bluetooth_detailed_view.get();
  device_list_controller_ = BluetoothDeviceListController::Factory::Create(
      bluetooth_detailed_view.get());
  BluetoothEnabledStateChanged();

  if (IsBluetoothEnabledOrEnabling(system_state_)) {
    device_list_controller_->UpdateDeviceList(connected_devices_,
                                              previously_connected_devices_);
  }

  // We are expected to return an unowned pointer that the caller is responsible
  // for deleting.
  return bluetooth_detailed_view.release()->GetAsView();
}

std::u16string BluetoothDetailedViewController::GetAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_BLUETOOTH_SETTINGS_ACCESSIBLE_DESCRIPTION);
}

void BluetoothDetailedViewController::OnPropertiesUpdated(
    chromeos::bluetooth_config::mojom::BluetoothSystemPropertiesPtr
        properties) {
  const bool has_bluetooth_enabled_state_changed =
      system_state_ != properties->system_state;
  system_state_ = properties->system_state;

  if (has_bluetooth_enabled_state_changed)
    BluetoothEnabledStateChanged();

  connected_devices_.clear();
  previously_connected_devices_.clear();

  for (auto& paired_device : properties->paired_devices) {
    if (paired_device->device_properties->connection_state ==
        DeviceConnectionState::kConnected) {
      connected_devices_.push_back(std::move(paired_device));
    } else {
      previously_connected_devices_.push_back(std::move(paired_device));
    }
  }
  if (device_list_controller_ && IsBluetoothEnabledOrEnabling(system_state_)) {
    device_list_controller_->UpdateDeviceList(connected_devices_,
                                              previously_connected_devices_);
  }
}

void BluetoothDetailedViewController::OnToggleClicked(bool new_state) {
  remote_cros_bluetooth_config_->SetBluetoothEnabledState(new_state);
}

void BluetoothDetailedViewController::OnPairNewDeviceRequested() {
  tray_controller_->CloseBubble();  // Deletes |this|.
  Shell::Get()->system_tray_model()->client()->ShowBluetoothPairingDialog(
      /*device_address=*/absl::nullopt);
}

void BluetoothDetailedViewController::OnDeviceListItemSelected(
    const chromeos::bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr&
        device) {
  // When CloseBubble() is called |device| will be deleted so we need to make a
  // copy of the device ID that was selected.
  const std::string device_id = device->device_properties->id;

  // Non-HID devices can be explicitly connected to, so we detect when this is
  // the case and attempt to connect to the device instead of navigating to the
  // Bluetooth Settings.
  if (device->device_properties->audio_capability ==
          AudioOutputCapability::kCapableOfAudioOutput &&
      device->device_properties->connection_state ==
          DeviceConnectionState::kNotConnected) {
    remote_cros_bluetooth_config_->Connect(device_id,
                                           /*callback=*/base::DoNothing());
    return;
  }
  tray_controller_->CloseBubble();  // Deletes |this|.
  Shell::Get()->system_tray_model()->client()->ShowBluetoothSettings(device_id);
}

void BluetoothDetailedViewController::BluetoothEnabledStateChanged() {
  const bool bluetooth_enabled_state =
      IsBluetoothEnabledOrEnabling(system_state_);
  if (view_)
    view_->UpdateBluetoothEnabledState(bluetooth_enabled_state);
  if (device_list_controller_) {
    device_list_controller_->UpdateBluetoothEnabledState(
        bluetooth_enabled_state);
  }
}

}  // namespace ash
