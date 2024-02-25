// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_FAKE_DISABLE_BLUETOOTH_DIALOG_CONTROLLER_H_
#define ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_FAKE_DISABLE_BLUETOOTH_DIALOG_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/bluetooth/hid_preserving_controller/disable_bluetooth_dialog_controller.h"

namespace ash {

class ASH_EXPORT FakeDisableBluetoothDialogController
    : public DisableBluetoothDialogController {
 public:
  FakeDisableBluetoothDialogController();

  FakeDisableBluetoothDialogController(
      const FakeDisableBluetoothDialogController&) = delete;
  FakeDisableBluetoothDialogController& operator=(
      const FakeDisableBluetoothDialogController&) = delete;

  ~FakeDisableBluetoothDialogController() override;

  // DisableBluetoothDialogController:
  void ShowDialog(const DeviceNamesList& devices,
                  ShowDialogCallback callback) override;

  void CompleteShowDialogCallback(bool show_dialog_result);
  size_t show_dialog_call_count() const { return show_dialog_call_count_; }

 private:
  DisableBluetoothDialogController::ShowDialogCallback show_dialog_callback_;
  size_t show_dialog_call_count_ = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_FAKE_DISABLE_BLUETOOTH_DIALOG_CONTROLLER_H_
