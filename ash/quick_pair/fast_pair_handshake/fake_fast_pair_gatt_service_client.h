// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_

#include <optional>

#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "base/functional/callback.h"

namespace device {
class BluetoothDevice;
}  // namespace device

namespace ash {
namespace quick_pair {

class FastPairDataEncryptor;

// This class fakes FastPairGattServiceClient and permits setting which
// PairFailure, if any, is run with the callback.
class FakeFastPairGattServiceClient : public FastPairGattServiceClient {
 public:
  FakeFastPairGattServiceClient(
      device::BluetoothDevice* device,
      scoped_refptr<device::BluetoothAdapter> adapter,
      base::OnceCallback<void(std::optional<PairFailure>)>
          on_initialized_callback);
  ~FakeFastPairGattServiceClient() override;

  device::BluetoothRemoteGattService* gatt_service() override;

  bool IsConnected() override;
  void SetConnected(bool is_connected);

  void ReadModelIdAsync(
      base::OnceCallback<void(
          std::optional<device::BluetoothGattService::GattErrorCode> error_code,
          const std::vector<uint8_t>& value)> callback) override;

  void WriteRequestAsync(
      uint8_t message_type,
      uint8_t flags,
      const std::string& provider_address,
      const std::string& seekers_address,
      FastPairDataEncryptor* fast_pair_data_encryptor,
      base::OnceCallback<void(std::vector<uint8_t>, std::optional<PairFailure>)>
          write_response_callback) override;

  void WritePasskeyAsync(
      uint8_t message_type,
      uint32_t passkey,
      FastPairDataEncryptor* fast_pair_data_encryptor,
      base::OnceCallback<void(std::vector<uint8_t>, std::optional<PairFailure>)>
          write_response_callback) override;

  void WriteAccountKey(std::array<uint8_t, 16> account_key,
                       FastPairDataEncryptor* fast_pair_data_encryptor,
                       base::OnceCallback<void(
                           std::optional<ash::quick_pair::AccountKeyFailure>)>
                           write_account_key_callback) override;

  void WritePersonalizedName(
      const std::string& name,
      const std::string& provider_address,
      FastPairDataEncryptor* fast_pair_data_encryptor,
      base::OnceCallback<void(std::optional<PairFailure>)>
          write_additional_data_callback) override;

  void RunOnGattClientInitializedCallback(
      std::optional<PairFailure> failure = std::nullopt);

  void RunReadModelIdCallback(
      std::optional<device::BluetoothGattService::GattErrorCode> error_code,
      const std::vector<uint8_t>& value);

  void RunWriteResponseCallback(
      std::vector<uint8_t> data,
      std::optional<PairFailure> failure = std::nullopt);

  void RunWritePasskeyCallback(
      std::vector<uint8_t> data,
      std::optional<PairFailure> failure = std::nullopt);

  void RunWriteAccountKeyCallback(
      std::optional<AccountKeyFailure> failure = std::nullopt);

  void RunWritePersonalizedNameCallback(
      std::optional<PairFailure> failure = std::nullopt);

 private:
  bool is_connected_ = false;
  base::OnceCallback<void(std::optional<PairFailure>)> on_initialized_callback_;
  base::OnceCallback<void(
      std::optional<device::BluetoothGattService::GattErrorCode> error_code,
      const std::vector<uint8_t>& value)>
      read_model_id_callback_;
  base::OnceCallback<void(std::vector<uint8_t>, std::optional<PairFailure>)>
      key_based_write_response_callback_;
  base::OnceCallback<void(std::vector<uint8_t>, std::optional<PairFailure>)>
      passkey_write_response_callback_;
  base::OnceCallback<void(std::optional<ash::quick_pair::AccountKeyFailure>)>
      write_account_key_callback_;
  base::OnceCallback<void(std::optional<PairFailure>)>
      write_personalized_name_callback_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_
