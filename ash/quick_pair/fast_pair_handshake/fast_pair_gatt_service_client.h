// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_

#include "ash/quick_pair/common/account_key_failure.h"
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

  // Gets ModelID from model ID characteristic. Upon successful completion
  // |error_code| will not have a value and |value| may be used. When
  // unsuccessful |error_code| will have a value and |value| must be ignored.
  virtual void ReadModelIdAsync(
      base::OnceCallback<void(
          std::optional<device::BluetoothGattService::GattErrorCode> error_code,
          const std::vector<uint8_t>& value)> callback) = 0;

  // Constructs a data vector based on the message type, flags, provider
  // address, and seekers address. Starts a notify session for key based
  // Pairing. Once the notify session has been started, the message data will be
  // written to the key based characteristic.
  virtual void WriteRequestAsync(
      uint8_t message_type,
      uint8_t flags,
      const std::string& provider_address,
      const std::string& seekers_address,
      FastPairDataEncryptor* fast_pair_data_encryptor,
      base::OnceCallback<void(std::vector<uint8_t>, std::optional<PairFailure>)>
          write_response_callback) = 0;

  // Constructs a data vector based on the message type and passkey. Starts a
  // notify session for the passkey. Once the notify session has been started,
  // the passkey data will be written to the passkey characteristic.
  virtual void WritePasskeyAsync(
      uint8_t message_type,
      uint32_t passkey,
      FastPairDataEncryptor* fast_pair_data_encryptor,
      base::OnceCallback<void(std::vector<uint8_t>, std::optional<PairFailure>)>
          write_response_callback) = 0;

  // Writes the account key to the account key characteristic.
  virtual void WriteAccountKey(
      std::array<uint8_t, 16> account_key,
      FastPairDataEncryptor* fast_pair_data_encryptor,
      base::OnceCallback<
          void(std::optional<ash::quick_pair::AccountKeyFailure>)>
          write_account_key_callback) = 0;

  // Writes `name` to the Additional Data characteristic as a personalized name.
  virtual void WritePersonalizedName(
      const std::string& name,
      const std::string& provider_address,
      FastPairDataEncryptor* fast_pair_data_encryptor,
      base::OnceCallback<void(std::optional<PairFailure>)>
          write_additional_data_callback) = 0;

  // Returns whether or not this client has an active GATT connection.
  virtual bool IsConnected() = 0;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_
