// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/nearby/bluetooth_adapter_util.h"

#include <memory>
#include <utility>

#include "chromeos/services/nearby/public/cpp/nearby_client_uuids.h"
#include "device/bluetooth/adapter.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace chromeos {
namespace nearby {

void MakeSelfOwnedBluetoothAdapter(
    mojo::PendingReceiver<bluetooth::mojom::Adapter> pending_receiver,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  auto adapter =
      std::make_unique<bluetooth::Adapter>(std::move(bluetooth_adapter));

  // Nearby Connections clients will be initiating and listening for connections
  // from the untrusted Nearby utility process. Pre-emptively allowlist their
  // UUIDs here in the browser process.
  for (const auto& uuid : GetNearbyClientUuids()) {
    adapter->AllowConnectionsForUuid(uuid);
  }

  mojo::MakeSelfOwnedReceiver(std::move(adapter), std::move(pending_receiver));
}

}  // namespace nearby
}  // namespace chromeos
