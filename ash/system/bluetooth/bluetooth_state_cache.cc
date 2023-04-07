// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_state_cache.h"

#include "ash/public/cpp/bluetooth_config_service.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"

namespace ash {

BluetoothStateCache::BluetoothStateCache() {
  // Asynchronously bind to CrosBluetoothConfig so that we don't attempt to bind
  // to it before it has initialized.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothStateCache::BindToCrosBluetoothConfig,
                                weak_ptr_factory_.GetWeakPtr()));
}

BluetoothStateCache::~BluetoothStateCache() = default;

void BluetoothStateCache::BindToCrosBluetoothConfig() {
  GetBluetoothConfigService(
      remote_cros_bluetooth_config_.BindNewPipeAndPassReceiver());
  // Observing system properties also does a fetch of the initial properties.
  remote_cros_bluetooth_config_->ObserveSystemProperties(
      system_properties_observer_receiver_.BindNewPipeAndPassRemote());
}

void BluetoothStateCache::OnPropertiesUpdated(
    bluetooth_config::mojom::BluetoothSystemPropertiesPtr properties) {
  system_state_ = properties->system_state;
}

}  // namespace ash
