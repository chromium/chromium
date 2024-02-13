// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/hid_preserving_controller/hid_preserving_bluetooth_state_controller_test_helper.h"

#include "ash/constants/ash_features.h"
#include "ash/system/bluetooth/hid_preserving_controller/in_process_instance.h"
#include "base/check.h"

namespace ash {

HidPreservingBluetoothStateControllerTestHelper::
    HidPreservingBluetoothStateControllerTestHelper() {
  CHECK(features::IsBluetoothDisconnectWarningEnabled());
  bluetooth::OverrideInProcessInstanceForTesting(&hid_preserving_bluetooth_);
}

HidPreservingBluetoothStateControllerTestHelper::
    ~HidPreservingBluetoothStateControllerTestHelper() {
  bluetooth::OverrideInProcessInstanceForTesting(nullptr);
}

}  // namespace ash
