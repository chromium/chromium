// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/hid_preserving_controller/fake_disable_bluetooth_dialog_controller.h"

#include "ash/constants/ash_features.h"

namespace ash {

FakeDisableBluetoothDialogController::FakeDisableBluetoothDialogController() {
  CHECK(features::IsBluetoothDisconnectWarningEnabled());
}

FakeDisableBluetoothDialogController::~FakeDisableBluetoothDialogController() =
    default;

void FakeDisableBluetoothDialogController::ShowDialog(
    const DeviceNamesList& devices,
    DisableBluetoothDialogController::ShowDialogCallback callback) {
  show_dialog_call_count_++;
  show_dialog_callback_ = std::move(callback);
}

void FakeDisableBluetoothDialogController::CompleteShowDialogCallback(
    bool show_dialog_result) {
  std::move(show_dialog_callback_).Run(show_dialog_result);
}

}  // namespace ash
