// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_SHARED_CONNECTION_MEDIUM_H_
#define ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_SHARED_CONNECTION_MEDIUM_H_

#include <ostream>

namespace chromeos {

namespace secure_channel {

// Enumeration of all mediums through which connections can be created.
enum class ConnectionMedium {
  // Uses BLE to create a GATT connection
  kBluetoothLowEnergy,

  // Uses the Nearby Connections library to connect over Bluetooth Classic, then
  // upgrade the connection to WebRTC for higher bandwidth.
  kNearbyConnections
};

std::ostream& operator<<(std::ostream& stream, const ConnectionMedium& medium);

}  // namespace secure_channel

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash::secure_channel {
using ::chromeos::secure_channel::ConnectionMedium;
}

#endif  // ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_SHARED_CONNECTION_MEDIUM_H_
