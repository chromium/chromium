// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/hid_preserving_controller/hid_preserving_bluetooth_state_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/hid_preserving_bluetooth_state_controller.mojom.h"
#include "ash/system/bluetooth/hid_preserving_controller/disable_bluetooth_dialog_controller_impl.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/device_event_log/device_event_log.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"

namespace ash {

HidPreservingBluetoothStateController::HidPreservingBluetoothStateController() {
  CHECK(features::IsBluetoothDisconnectWarningEnabled());

  // Asynchronously bind to CrosBluetoothConfig so that we don't attempt
  // to bind to it before it has initialized.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &HidPreservingBluetoothStateController::BindToCrosBluetoothConfig,
          weak_ptr_factory_.GetWeakPtr()));
}

HidPreservingBluetoothStateController::
    ~HidPreservingBluetoothStateController() = default;

void HidPreservingBluetoothStateController::BindPendingReceiver(
    mojo::PendingReceiver<mojom::HidPreservingBluetoothStateController>
        pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void HidPreservingBluetoothStateController::TryToSetBluetoothEnabledState(
    bool enabled) {
  // If we are not disabling Bluetooth, no action is needed.
  if (enabled) {
    SetBluetoothEnabledState(enabled);
    return;
  }

  auto device_names = GetBluetoothDeviceNamesIfOnlyHids();
  if (device_names.empty()) {
    BLUETOOTH_LOG(DEBUG) << "No Bluetooth devices found, disabling Bluetooth";
    SetBluetoothEnabledState(enabled);
    return;
  }

  if (!disable_bluetooth_dialog_controller_) {
    disable_bluetooth_dialog_controller_ =
        std::make_unique<DisableBluetoothDialogControllerImpl>();
  }

  BLUETOOTH_LOG(EVENT)
      << "Showing warning dialog: number of Bluetooth HID devices connected: "
      << device_names.size();
  disable_bluetooth_dialog_controller_->ShowDialog(
      device_names,
      base::BindOnce(&HidPreservingBluetoothStateController::OnShowCallback,
                     weak_ptr_factory_.GetWeakPtr(), enabled));
}

void HidPreservingBluetoothStateController::OnShowCallback(
    bool enabled,
    bool show_dialog_result) {
  BLUETOOTH_LOG(USER) << "Warning dialog result: " << show_dialog_result;

  // User decided not to disable bluetooth.
  if (!show_dialog_result) {
    return;
  }

  SetBluetoothEnabledState(enabled);
}

DisableBluetoothDialogController::DeviceNamesList
HidPreservingBluetoothStateController::GetBluetoothDeviceNamesIfOnlyHids() {
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();

  DisableBluetoothDialogController::DeviceNamesList bluetooth_devices;
  if (device_data_manager->GetTouchscreenDevices().size() > 0 ||
      device_data_manager->GetPointingStickDevices().size() > 0 ||
      device_data_manager->GetTouchpadDevices().size() > 0) {
    return bluetooth_devices;
  }

  for (const auto& keyboard : device_data_manager->GetKeyboardDevices()) {
    // A non-Bluetooth HID is connected, return an empty list.
    if (keyboard.type != ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH) {
      return DisableBluetoothDialogController::DeviceNamesList();
    }
    bluetooth_devices.push_back(keyboard.name);
  }

  for (const auto& mice : device_data_manager->GetMouseDevices()) {
    // A non-Bluetooth HID is connected, return an empty list.
    if (mice.type != ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH) {
      return DisableBluetoothDialogController::DeviceNamesList();
    }
    bluetooth_devices.push_back(mice.name);
  }

  return bluetooth_devices;
}

void HidPreservingBluetoothStateController::SetBluetoothEnabledState(
    bool enabled) {
  CHECK(cros_bluetooth_config_remote_);
  cros_bluetooth_config_remote_->SetBluetoothEnabledState(enabled);
}

void HidPreservingBluetoothStateController::BindToCrosBluetoothConfig() {
  GetBluetoothConfigService(
      cros_bluetooth_config_remote_.BindNewPipeAndPassReceiver());
}

void HidPreservingBluetoothStateController::
    SetDisableBluetoothDialogControllerForTest(
        std::unique_ptr<ash::DisableBluetoothDialogController> controller) {
  disable_bluetooth_dialog_controller_ = std::move(controller);
}

DisableBluetoothDialogController*
HidPreservingBluetoothStateController::GetDisabledBluetoothDialogForTesting() {
  CHECK(disable_bluetooth_dialog_controller_);
  return disable_bluetooth_dialog_controller_.get();
}
}  // namespace ash
