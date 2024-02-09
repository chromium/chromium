// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_DISABLE_BLUETOOTH_DIALOG_CONTROLLER_H_
#define ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_DISABLE_BLUETOOTH_DIALOG_CONTROLLER_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "base/functional/callback.h"

namespace ash {

// The DisableBluetoothDialogController displays a UI Blocking dialog which
// warns that disabling Bluetooth will disconnect currently paired HID input
// devices.
class ASH_EXPORT DisableBluetoothDialogController {
 public:
  DisableBluetoothDialogController() = default;

  DisableBluetoothDialogController(const DisableBluetoothDialogController&) =
      delete;
  DisableBluetoothDialogController& operator=(
      const DisableBluetoothDialogController&) = delete;

  virtual ~DisableBluetoothDialogController() = default;

  using ShowDialogCallback = base::OnceCallback<void(bool)>;
  using DeviceNamesList = std::vector<std::string>;

  virtual void ShowDialog(const DeviceNamesList& devices,
                          ShowDialogCallback callback) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_DISABLE_BLUETOOTH_DIALOG_CONTROLLER_H_
