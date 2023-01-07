// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_

#include "device/bluetooth/bluetooth_adapter.h"

inline constexpr int kBlockByteSize = 16;

namespace ash {
namespace quick_pair {

class FastPairDataEncryptor;

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
      FastPairDataEncryptor* fast_pair_data_encryptor,
      base::OnceCallback<void(std::vector<uint8_t>,
                              absl::optional<PairFailure>)>
          write_response_callback) = 0;

  // Constructs a data vector based on the message type and passkey. Writes
  // data to the passkey characteristic and calls the callback with response
  // data on success, or with a PairFailure on failure.
  virtual void WritePasskeyAsync(
      uint8_t message_type,
      uint32_t passkey,
      FastPairDataEncryptor* fast_pair_data_encryptor,
      base::OnceCallback<void(std::vector<uint8_t>,
                              absl::optional<PairFailure>)>
          write_response_callback) = 0;

  // Writes the account key to the account key characteristic.
  virtual void WriteAccountKey(
      std::array<uint8_t, 16> account_key,
      FastPairDataEncryptor* fast_pair_data_encryptor,
      base::OnceCallback<
          void(absl::optional<device::BluetoothGattService::GattErrorCode>)>
          write_account_key_callback) = 0;

  // Returns whether or not this client has an active GATT connection.
  virtual bool IsConnected() = 0;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_
