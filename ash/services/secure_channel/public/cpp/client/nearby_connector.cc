// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/secure_channel/public/cpp/client/nearby_connector.h"

namespace ash::secure_channel {

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace mojom = ::chromeos::secure_channel::mojom;

NearbyConnector::NearbyConnector() = default;

NearbyConnector::~NearbyConnector() = default;

mojo::PendingRemote<mojom::NearbyConnector>
NearbyConnector::GeneratePendingRemote() {
  mojo::PendingRemote<mojom::NearbyConnector> pending_remote;
  receiver_set_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

}  // namespace ash::secure_channel
