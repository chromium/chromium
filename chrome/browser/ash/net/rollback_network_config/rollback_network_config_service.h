// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_ROLLBACK_NETWORK_CONFIG_ROLLBACK_NETWORK_CONFIG_SERVICE_H_
#define CHROME_BROWSER_ASH_NET_ROLLBACK_NETWORK_CONFIG_ROLLBACK_NETWORK_CONFIG_SERVICE_H_

#include "chrome/browser/ash/net/rollback_network_config/rollback_network_config.h"
#include "chromeos/ash/services/rollback_network_config/public/mojom/rollback_network_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {
namespace rollback_network_config {

// Deletes the `RollbackNetworkConfig` in-process instance which removes
// observers and cancels any pending request.
void Shutdown();

// Binds a receiver to the in-process instance of `RollbackNetworkConfig`. Will
// create the in-process instance if it has not been created yet.
void BindToInProcessInstance(
    mojo::PendingReceiver<mojom::RollbackNetworkConfig> receiver);

RollbackNetworkConfig* OverrideInProcessInstanceForTesting(
    std::unique_ptr<RollbackNetworkConfig> instance);

}  // namespace rollback_network_config
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_ROLLBACK_NETWORK_CONFIG_ROLLBACK_NETWORK_CONFIG_SERVICE_H_
