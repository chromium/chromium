// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/bluetooth_config_service.h"

#include "chromeos/ash/services/bluetooth_config/in_process_instance.h"

namespace ash {

void GetBluetoothConfigService(
    mojo::PendingReceiver<bluetooth_config::mojom::CrosBluetoothConfig>
        receiver) {
  bluetooth_config::BindToInProcessInstance(std::move(receiver));
}

}  // namespace ash
