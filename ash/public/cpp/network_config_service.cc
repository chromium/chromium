// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/network_config_service.h"

#include "chromeos/ash/services/network_config/in_process_instance.h"

namespace ash {

void GetNetworkConfigService(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  network_config::BindToInProcessInstance(std::move(receiver));
}

}  // namespace ash
