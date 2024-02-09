// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/hid_preserving_controller/in_process_instance.h"

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/hid_preserving_bluetooth_state_controller.mojom.h"
#include "ash/system/bluetooth/hid_preserving_controller/hid_preserving_bluetooth_state_controller.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/device_event_log/device_event_log.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::bluetooth {

namespace {

mojom::HidPreservingBluetoothStateController* g_instance_override = nullptr;

mojo::ReceiverSet<mojom::HidPreservingBluetoothStateController>&
GetOverrideReceivers() {
  static base::NoDestructor<
      mojo::ReceiverSet<mojom::HidPreservingBluetoothStateController>>
      receivers;
  return *receivers;
}

}  // namespace

void BindToInProcessInstance(
    mojo::PendingReceiver<ash::mojom::HidPreservingBluetoothStateController>
        pending_receiver) {
  BLUETOOTH_LOG(DEBUG) << "Binding to HidPreservingBluetoothStateController";
  CHECK(ash::features::IsBluetoothDisconnectWarningEnabled());
  if (g_instance_override) {
    GetOverrideReceivers().Add(g_instance_override,
                               std::move(pending_receiver));
    return;
  }

  static base::NoDestructor<HidPreservingBluetoothStateController> instance;
  instance->BindPendingReceiver(std::move(pending_receiver));
}

void OverrideInProcessInstanceForTesting(  // IN-TEST
    mojom::HidPreservingBluetoothStateController* hid_preserving_bluetooth) {
  g_instance_override = hid_preserving_bluetooth;

  // Wipe out the set of override receivers any time a new override is set.
  GetOverrideReceivers().Clear();
}
}  // namespace ash::bluetooth
