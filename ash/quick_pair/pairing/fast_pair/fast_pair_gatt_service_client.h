// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_GATT_SERVICE_CLIENT_H_
#define ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_GATT_SERVICE_CLIENT_H_

#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {
namespace quick_pair {

// This class is responsible for connecting to the Fast Pair GATT service for a
// device and invoking a callback when ready, or when an error is discovered
// during initialization.
class FastPairGattServiceClient : public device::BluetoothAdapter::Observer {
 public:
  ~FastPairGattServiceClient() override = default;
  virtual device::BluetoothRemoteGattService* gatt_service() = 0;

  // Constructs a data vector based on the message type, flags, provider
  // address, and seekers address. Writes data to the key based characteristic
  // and calls the callback with response data on success, or with a PairFailure
  // on failure.
  virtual void WriteRequestAsync(
      uint8_t message_type,
      uint8_t flags,
      const std::string& provider_address,
      const std::string& seekers_address,
      base::OnceCallback<void(std::vector<uint8_t>,
                              absl::optional<PairFailure>)>
          write_response_callback) = 0;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_GATT_SERVICE_CLIENT_H_
