// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/rollback_network_config/rollback_network_config_service.h"

#include "base/check.h"
#include "base/logging.h"
#include "chrome/browser/ash/net/rollback_network_config/rollback_network_config.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/services/rollback_network_config/public/mojom/rollback_network_config.mojom.h"

namespace ash {
namespace rollback_network_config {

namespace {
RollbackNetworkConfig* g_rollback_network_config_instance;
}

RollbackNetworkConfig* GetInstance() {
  if (!g_rollback_network_config_instance) {
    g_rollback_network_config_instance = new RollbackNetworkConfig();
  }
  return g_rollback_network_config_instance;
}

void Shutdown() {
  if (g_rollback_network_config_instance) {
    delete g_rollback_network_config_instance;
    g_rollback_network_config_instance = nullptr;
  }
}

void BindToInProcessInstance(
    mojo::PendingReceiver<mojom::RollbackNetworkConfig> receiver) {
  // This service requires a network handler to fetch configurations or apply
  // configurations.
  if (!NetworkHandler::IsInitialized()) {
    DVLOG(1) << "Ignoring request to bind Rollback Network Config service "
                "because no NetworkHandler has been initialized.";
    return;
  }

  GetInstance()->BindReceiver(std::move(receiver));
}

RollbackNetworkConfig* OverrideInProcessInstanceForTesting(
    std::unique_ptr<RollbackNetworkConfig> instance) {
  g_rollback_network_config_instance = instance.release();
  return g_rollback_network_config_instance;
}

}  // namespace rollback_network_config
}  // namespace ash
