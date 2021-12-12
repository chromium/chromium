// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_status_ui_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/public/cpp/toast_manager.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "chromeos/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

using chromeos::bluetooth_config::GetPairedDeviceName;
using chromeos::bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;

const char kBluetoothToastIdPrefix[] = "cros_bluetooth_device_toast_id-";
constexpr int kToastDurationMs = 6000;

}  // namespace

BluetoothDeviceStatusUiHandler::BluetoothDeviceStatusUiHandler() {
  DCHECK(ash::features::IsBluetoothRevampEnabled());
  GetBluetoothConfigService(
      remote_cros_bluetooth_config_.BindNewPipeAndPassReceiver());
  remote_cros_bluetooth_config_->ObserveDeviceStatusChanges(
      cros_bluetooth_device_status_observer_receiver_
          .BindNewPipeAndPassRemote());
}

BluetoothDeviceStatusUiHandler::~BluetoothDeviceStatusUiHandler() = default;

void BluetoothDeviceStatusUiHandler::OnDevicePaired(
    PairedBluetoothDevicePropertiesPtr device) {
  ash::ToastData toast_data(
      /*id=*/GetToastId(device.get()),
      /*text=*/
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIRED_OR_CONNECTED_TOAST,
          GetPairedDeviceName(device.get())),
      /*timeout_ms=*/kToastDurationMs,
      /*dismiss_text=*/absl::nullopt,
      /*visible_on_lock_screen=*/false);

  ShowToast(toast_data);
  device::RecordUiSurfaceDisplayed(device::BluetoothUiSurface::kPairedToast);
}

void BluetoothDeviceStatusUiHandler::OnDeviceDisconnected(
    PairedBluetoothDevicePropertiesPtr device) {
  ash::ToastData toast_data(
      /*id=*/GetToastId(device.get()),
      /*text=*/
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DISCONNECTED_TOAST,
          GetPairedDeviceName(device.get())),
      /*timeout_ms=*/kToastDurationMs,
      /*dismiss_text=*/absl::nullopt,
      /*visible_on_lock_screen=*/false);
  ShowToast(toast_data);
  device::RecordUiSurfaceDisplayed(
      device::BluetoothUiSurface::kConnectionToast);
}

void BluetoothDeviceStatusUiHandler::OnDeviceConnected(
    PairedBluetoothDevicePropertiesPtr device) {
  ash::ToastData toast_data(
      /*id=*/GetToastId(device.get()),
      /*text=*/
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIRED_OR_CONNECTED_TOAST,
          GetPairedDeviceName(device.get())),
      /*timeout_ms=*/kToastDurationMs,
      /*dismiss_text=*/absl::nullopt,
      /*visible_on_lock_screen=*/false);
  ShowToast(toast_data);
  device::RecordUiSurfaceDisplayed(
      device::BluetoothUiSurface::kDisconnectedToast);
}

void BluetoothDeviceStatusUiHandler::ShowToast(
    const ash::ToastData& toast_data) {
  ash::ToastManager::Get()->Show(toast_data);
}

std::string BluetoothDeviceStatusUiHandler::GetToastId(
    const chromeos::bluetooth_config::mojom::PairedBluetoothDeviceProperties*
        paired_device_properties) {
  return kBluetoothToastIdPrefix +
         base::ToLowerASCII(paired_device_properties->device_properties->id);
}

}  // namespace ash