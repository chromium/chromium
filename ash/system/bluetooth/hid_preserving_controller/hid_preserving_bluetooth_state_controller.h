// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_HID_PRESERVING_BLUETOOTH_STATE_CONTROLLER_H_
#define ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_HID_PRESERVING_BLUETOOTH_STATE_CONTROLLER_H_

#include <memory>

#include "ash/public/cpp/bluetooth_config_service.h"
#include "ash/public/mojom/hid_preserving_bluetooth_state_controller.mojom.h"
#include "ash/system/bluetooth/hid_preserving_controller/disable_bluetooth_dialog_controller_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"

namespace ash {

// Implements the HidPreservingBluetoothStateController API, which is used to
// set Bluetooth system state on Chrome OS. This class instantiates helper
// classes and implements the API by delegating to these helpers.
class ASH_EXPORT HidPreservingBluetoothStateController
    : public mojom::HidPreservingBluetoothStateController {
 public:
  HidPreservingBluetoothStateController();

  HidPreservingBluetoothStateController(
      const HidPreservingBluetoothStateController&) = delete;
  HidPreservingBluetoothStateController& operator=(
      const HidPreservingBluetoothStateController&) = delete;

  ~HidPreservingBluetoothStateController() override;

  // Binds a PendingReceiver to this instance. Clients wishing to use the
  // HidPreservingBluetoothStateController API should use this function as an
  // entrypoint.
  void BindPendingReceiver(
      mojo::PendingReceiver<mojom::HidPreservingBluetoothStateController>
          pending_receiver);

  DisableBluetoothDialogController::DeviceNamesList*
  device_names_for_testing() {
    return &device_names_;
  }

 private:
  friend class HidPreservingBluetoothStateControllerTest;

  // mojom::HidPreservingBluetoothStateController:
  void TryToSetBluetoothEnabledState(
      bool enabled,
      mojom::HidWarningDialogSource source) override;

  // Called when warning dialog selection is made.
  // `enabled`: new state of Bluetooth device. Passed in from
  // `TryToSetBluetoothEnabledState`.
  // `show_dialog_result`: user selection, true if user chooses to disable
  // Blueooth device state.
  void OnShowCallback(bool enabled, bool show_dialog_result);

  // Returns a list of Bluetooth devices if only Bluetooth HID devices are
  // found, else returns an empty list.
  DisableBluetoothDialogController::DeviceNamesList
  GetBluetoothDeviceNamesIfOnlyHids();

  void SetBluetoothEnabledState(bool enabled);
  void BindToCrosBluetoothConfig();

  void SetDisableBluetoothDialogControllerForTest(
      std::unique_ptr<ash::DisableBluetoothDialogController> controller);
  DisableBluetoothDialogController* GetDisabledBluetoothDialogForTesting();

  DisableBluetoothDialogController::DeviceNamesList device_names_;
  std::unique_ptr<ash::DisableBluetoothDialogController>
      disable_bluetooth_dialog_controller_;
  mojo::Remote<bluetooth_config::mojom::CrosBluetoothConfig>
      cros_bluetooth_config_remote_;
  mojo::ReceiverSet<mojom::HidPreservingBluetoothStateController> receivers_;

  base::WeakPtrFactory<HidPreservingBluetoothStateController> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_HID_PRESERVING_BLUETOOTH_STATE_CONTROLLER_H_
