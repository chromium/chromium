// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_FAKE_HID_PRESERVING_BLUETOOTH_STATE_CONTROLLER_H_
#define ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_FAKE_HID_PRESERVING_BLUETOOTH_STATE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/hid_preserving_bluetooth_state_controller.mojom.h"
#include "ash/system/bluetooth/hid_preserving_controller/disable_bluetooth_dialog_controller_impl.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"

namespace ash {

// Fake implementation of HidPreservingBluetoothStateController API.
class ASH_EXPORT FakeHidPreservingBluetoothStateController
    : public mojom::HidPreservingBluetoothStateController {
 public:
  FakeHidPreservingBluetoothStateController();

  FakeHidPreservingBluetoothStateController(
      const FakeHidPreservingBluetoothStateController&) = delete;
  FakeHidPreservingBluetoothStateController& operator=(
      const FakeHidPreservingBluetoothStateController&) = delete;

  ~FakeHidPreservingBluetoothStateController() override;

  // mojom::HidPreservingBluetoothStateController:
  void TryToSetBluetoothEnabledState(
      bool enabled,
      mojom::HidWarningDialogSource source) override;

  void SetShouldShowWarningDialog(bool should_show_warning_dialog);

  // Should only be called if |should_show_warning_dialog| is true.
  void CompleteShowDialog(bool show_dialog_result);

  size_t dialog_shown_count() { return dialog_shown_count_; }

  void SetScopedBluetoothConfigHelper(
      bluetooth_config::ScopedBluetoothConfigTestHelper* helper);

 private:
  void SetBluetoothEnabledState(bool enabled);

  size_t dialog_shown_count_ = 0u;
  bool pending_bluetooth_enabled_request_ = false;
  bool should_show_warning_dialog_ = false;
  raw_ptr<bluetooth_config::ScopedBluetoothConfigTestHelper, DanglingUntriaged>
      scoped_bluetooth_config_test_helper_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_FAKE_HID_PRESERVING_BLUETOOTH_STATE_CONTROLLER_H_
