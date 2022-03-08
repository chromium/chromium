// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_NEARBY_CONNECTOR_H_
#define ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_NEARBY_CONNECTOR_H_

#include "ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::secure_channel {

// Provides Nearby Connections functionality to the SecureChannel service.
class NearbyConnector : public mojom::NearbyConnector {
 public:
  NearbyConnector();
  ~NearbyConnector() override;

  mojo::PendingRemote<mojom::NearbyConnector> GeneratePendingRemote();

 private:
  mojo::ReceiverSet<mojom::NearbyConnector> receiver_set_;
};

}  // namespace ash::secure_channel

#endif  // ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_NEARBY_CONNECTOR_H_
