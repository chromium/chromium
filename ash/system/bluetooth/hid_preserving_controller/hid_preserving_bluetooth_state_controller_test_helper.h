// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_HID_PRESERVING_BLUETOOTH_STATE_CONTROLLER_TEST_HELPER_H_
#define ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_HID_PRESERVING_BLUETOOTH_STATE_CONTROLLER_TEST_HELPER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/hid_preserving_bluetooth_state_controller.mojom.h"
#include "ash/system/bluetooth/hid_preserving_controller/fake_hid_preserving_bluetooth_state_controller.h"
#include "base/memory/raw_ptr.h"

namespace ash {

// Helper for tests which need a HidPreservingBluetoothStateController service
// interface.
class ASH_EXPORT HidPreservingBluetoothStateControllerTestHelper {
 public:
  HidPreservingBluetoothStateControllerTestHelper();

  HidPreservingBluetoothStateControllerTestHelper(
      const HidPreservingBluetoothStateControllerTestHelper&) = delete;
  HidPreservingBluetoothStateControllerTestHelper& operator=(
      const HidPreservingBluetoothStateControllerTestHelper&) = delete;

  ~HidPreservingBluetoothStateControllerTestHelper();

  FakeHidPreservingBluetoothStateController* fake_hid_preserving_bluetooth() {
    return &hid_preserving_bluetooth_;
  }

 protected:
  FakeHidPreservingBluetoothStateController hid_preserving_bluetooth_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_HID_PRESERVING_BLUETOOTH_STATE_CONTROLLER_TEST_HELPER_H_
