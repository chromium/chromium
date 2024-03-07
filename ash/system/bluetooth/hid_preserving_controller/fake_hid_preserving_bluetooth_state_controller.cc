// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/hid_preserving_controller/fake_hid_preserving_bluetooth_state_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/hid_preserving_bluetooth_state_controller.mojom.h"
#include "chromeos/ash/services/bluetooth_config/fake_bluetooth_power_controller.h"

namespace ash {

FakeHidPreservingBluetoothStateController::
    FakeHidPreservingBluetoothStateController() {
  CHECK(features::IsBluetoothDisconnectWarningEnabled());
}

FakeHidPreservingBluetoothStateController::
    ~FakeHidPreservingBluetoothStateController() = default;

void FakeHidPreservingBluetoothStateController::TryToSetBluetoothEnabledState(
    bool enabled,
    mojom::HidWarningDialogSource source) {
  if (should_show_warning_dialog_) {
    pending_bluetooth_enabled_request_ = enabled;
    dialog_shown_count_++;
    return;
  }

  SetBluetoothEnabledState(enabled);
}

void FakeHidPreservingBluetoothStateController::SetShouldShowWarningDialog(
    bool should_show_warning_dialog) {
  should_show_warning_dialog_ = should_show_warning_dialog;
}

void FakeHidPreservingBluetoothStateController::CompleteShowDialog(
    bool show_dialog_result) {
  CHECK(should_show_warning_dialog_);
  if (!show_dialog_result) {
    return;
  }

  should_show_warning_dialog_ = false;
  SetBluetoothEnabledState(pending_bluetooth_enabled_request_);
}

void FakeHidPreservingBluetoothStateController::SetScopedBluetoothConfigHelper(
    bluetooth_config::ScopedBluetoothConfigTestHelper* helper) {
  scoped_bluetooth_config_test_helper_ = helper;
}

void FakeHidPreservingBluetoothStateController::SetBluetoothEnabledState(
    bool enabled) {
  CHECK(scoped_bluetooth_config_test_helper_);
  scoped_bluetooth_config_test_helper_->fake_bluetooth_power_controller()
      ->SetBluetoothEnabledState(enabled);
}

}  // namespace ash
