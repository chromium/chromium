// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_REMOTE_ATTRIBUTE_H_
#define ASH_SERVICES_SECURE_CHANNEL_REMOTE_ATTRIBUTE_H_

#include <string>

#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace chromeos {

namespace secure_channel {

// Represents an attribute in the peripheral (service or characteristic).
struct RemoteAttribute {
  device::BluetoothUUID uuid;
  std::string id;
};

}  // namespace secure_channel

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash::secure_channel {
using ::chromeos::secure_channel::RemoteAttribute;
}

#endif  // ASH_SERVICES_SECURE_CHANNEL_REMOTE_ATTRIBUTE_H_
