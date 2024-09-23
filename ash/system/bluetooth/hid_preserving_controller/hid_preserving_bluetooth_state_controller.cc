// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/hid_preserving_controller/hid_preserving_bluetooth_state_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/hid_preserving_bluetooth_state_controller.mojom.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/bluetooth/hid_preserving_controller/disable_bluetooth_dialog_controller_impl.h"
#include "ash/system/bluetooth/hid_preserving_controller/hid_preserving_bluetooth_metrics.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
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
    bool enabled,
    mojom::HidWarningDialogSource source) {
  // If we are not disabling Bluetooth, no action is needed.
  if (enabled) {
    SetBluetoothEnabledState(enabled);
    return;
  }

  device_names_ = GetBluetoothDeviceNamesIfOnlyHids();
  if (device_names_.empty()) {
    BLUETOOTH_LOG(DEBUG) << "No Bluetooth devices found, disabling Bluetooth";
    SetBluetoothEnabledState(enabled);
    bluetooth::RecordHidPoweredStateDisableBehavior(/*dialog_shown=*/false);
    return;
  }

  if (!disable_bluetooth_dialog_controller_) {
    disable_bluetooth_dialog_controller_ =
        std::make_unique<DisableBluetoothDialogControllerImpl>();
  }

  bluetooth::RecordHidWarningDialogSource(source);
  bluetooth::RecordHidPoweredStateDisableBehavior(/*dialog_shown=*/true);
  BLUETOOTH_LOG(EVENT)
      << "Showing warning dialog: number of Bluetooth HID devices connected: "
      << device_names_.size();
  disable_bluetooth_dialog_controller_->ShowDialog(
      device_names_,
      base::BindOnce(&HidPreservingBluetoothStateController::OnShowCallback,
                     weak_ptr_factory_.GetWeakPtr(), enabled));
}

void HidPreservingBluetoothStateController::OnShowCallback(
    bool enabled,
    bool show_dialog_result) {
  BLUETOOTH_LOG(USER) << "Warning dialog result: " << show_dialog_result;
  bluetooth::RecordHidWarningUserAction(show_dialog_result);
  // User decided not to disable bluetooth.
  if (!show_dialog_result) {
    return;
  }

  SetBluetoothEnabledState(enabled);
}

DisableBluetoothDialogController::DeviceNamesList
HidPreservingBluetoothStateController::GetBluetoothDeviceNamesIfOnlyHids() {
  // TODO(b/333439577): Update this to only use
  // InputDeviceSettingsControllerImpl API.
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  InputDeviceSettingsControllerImpl* input_device_settings_controller =
      Shell::Get()->input_device_settings_controller();
  DisableBluetoothDialogController::DeviceNamesList bluetooth_devices;

  for (const auto& touchscreen : device_data_manager->GetTouchscreenDevices()) {
    // A non-Bluetooth HID is connected, return an empty list.
    if (touchscreen.type != ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH) {
      BLUETOOTH_LOG(DEBUG) << "Non-Bluetooth touchscreen found: "
                           << touchscreen.name << " Type: " << touchscreen.type;
      return DisableBluetoothDialogController::DeviceNamesList();
    }
    bluetooth_devices.push_back(touchscreen.name);
  }

  for (const auto& graphics_tablet :
       device_data_manager->GetGraphicsTabletDevices()) {
    // If graphics tablet device is not in list of expected devices do not
    // consider this device. see (b/332394154)
    const mojom::GraphicsTablet* found_device =
        input_device_settings_controller->GetGraphicsTablet(graphics_tablet.id);

    if (found_device == nullptr) {
      continue;
    }

    // A non-Bluetooth HID is connected, return an empty list.
    if (graphics_tablet.type != ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH) {
      BLUETOOTH_LOG(DEBUG) << "Non-Bluetooth graphics tablet found: "
                           << graphics_tablet.name
                           << ", Type: " << graphics_tablet.type;
      return DisableBluetoothDialogController::DeviceNamesList();
    }
    bluetooth_devices.push_back(graphics_tablet.name);
  }

  for (const auto& pointing_stick :
       device_data_manager->GetPointingStickDevices()) {
    // If pointing stick device is not in list of expected devices do not
    // consider this device. see (b/332394154)
    const mojom::PointingStick* found_device =
        input_device_settings_controller->GetPointingStick(pointing_stick.id);

    if (found_device == nullptr) {
      continue;
    }

    // A non-Bluetooth HID is connected, return an empty list.
    if (pointing_stick.type != ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH) {
      BLUETOOTH_LOG(DEBUG) << "Non-Bluetooth pointing_stick found: "
                           << pointing_stick.name
                           << " Type: " << pointing_stick.type;
      return DisableBluetoothDialogController::DeviceNamesList();
    }
    bluetooth_devices.push_back(pointing_stick.name);
  }

  for (const auto& touchpad : device_data_manager->GetTouchpadDevices()) {
    // If touchpad device is not in list of expected devices do not consider
    // this device. see (b/332394154)
    const mojom::Touchpad* found_device =
        input_device_settings_controller->GetTouchpad(touchpad.id);

    if (found_device == nullptr) {
      continue;
    }

    // A non-Bluetooth HID is connected, return an empty list.
    if (touchpad.type != ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH) {
      BLUETOOTH_LOG(DEBUG) << "Non-Bluetooth keyboard found: " << touchpad.name
                           << " Type: " << touchpad.type;
      return DisableBluetoothDialogController::DeviceNamesList();
    }
    bluetooth_devices.push_back(touchpad.name);
  }

  for (const auto& keyboard : device_data_manager->GetKeyboardDevices()) {
    // If keyboard device is not in list of expected devices do not consider
    // this device. see (b/332394154)
    const mojom::Keyboard* found_device =
        input_device_settings_controller->GetKeyboard(keyboard.id);

    if (found_device == nullptr) {
      continue;
    }

    // A non-Bluetooth HID is connected, return an empty list.
    if (keyboard.type != ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH) {
      BLUETOOTH_LOG(DEBUG) << "Non-Bluetooth keyboard found: " << keyboard.name
                           << " Type: " << keyboard.type;
      return DisableBluetoothDialogController::DeviceNamesList();
    }
    bluetooth_devices.push_back(keyboard.name);
  }

  for (const auto& mice : device_data_manager->GetMouseDevices()) {
    // If mouse device is not in list of expected devices do not consider this
    // device. see (b/332394154)
    const mojom::Mouse* found_device =
        input_device_settings_controller->GetMouse(mice.id);

    if (found_device == nullptr) {
      continue;
    }

    // A non-Bluetooth HID is connected, return an empty list.
    if (mice.type != ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH) {
      BLUETOOTH_LOG(DEBUG) << "Non-Bluetooth mouse found: " << mice.name
                           << ", Type: " << mice.type;
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
