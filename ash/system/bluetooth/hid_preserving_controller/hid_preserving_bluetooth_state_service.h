// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_HID_PRESERVING_BLUETOOTH_STATE_SERVICE_H_
#define ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_HID_PRESERVING_BLUETOOTH_STATE_SERVICE_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/hid_preserving_bluetooth_state_controller.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

// Binds |pending_receiver| to HidPreservingBluetoothStateController. Clients
// should use this function as the main entrypoint to the Mojo API.
//
// Internally, this function delegates to an implementation in the browser
// process. We declare this function in //ash to ensure that clients do not have
// any direct dependencies on the implementation.
ASH_EXPORT void GetHidPreservingBluetoothStateControllerService(
    mojo::PendingReceiver<mojom::HidPreservingBluetoothStateController>
        pending_receiver);

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_HID_PRESERVING_BLUETOOTH_STATE_SERVICE_H_
