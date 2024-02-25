// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_IN_PROCESS_INSTANCE_H_
#define ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_IN_PROCESS_INSTANCE_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/hid_preserving_bluetooth_state_controller.mojom.h"
#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash::bluetooth {

// Binds to an instance of HidPreservingBluetoothStateController from within the
// browser process.
ASH_EXPORT void BindToInProcessInstance(
    mojo::PendingReceiver<mojom::HidPreservingBluetoothStateController>
        pending_receiver);

// Overrides the in-process instance for testing purposes.
ASH_EXPORT void OverrideInProcessInstanceForTesting(
    mojom::HidPreservingBluetoothStateController* hid_preserving_bluetooth);

}  // namespace ash::bluetooth

#endif  // ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_IN_PROCESS_INSTANCE_H_
