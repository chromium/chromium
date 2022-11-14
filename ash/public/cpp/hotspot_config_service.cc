// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/hotspot_config_service.h"

#include "chromeos/ash/services/hotspot_config/in_process_instance.h"

namespace ash {

void GetHotspotConfigService(
    mojo::PendingReceiver<hotspot_config::mojom::CrosHotspotConfig> receiver) {
  hotspot_config::BindToInProcessInstance(std::move(receiver));
}

}  // namespace ash