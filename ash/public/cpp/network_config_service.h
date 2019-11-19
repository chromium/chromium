// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_NETWORK_CONFIG_SERVICE_H_
#define ASH_PUBLIC_CPP_NETWORK_CONFIG_SERVICE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

ASH_PUBLIC_EXPORT void GetNetworkConfigService(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_NETWORK_CONFIG_SERVICE_H_
