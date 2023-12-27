// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_IMPL_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_IMPL_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"

namespace device {

class BluetoothDevice;
class BluetoothGattConnection;
class BluetoothRemoteGattService;
class BluetoothGattNotifySession;
class BluetoothRemoteGattService;

}  // namespace device

namespace ash {
namespace quick_pair {

class FastPairDataEncryptor;

// This class is responsible for connecting to the Fast Pair GATT service for a
// device and invoking a callback when ready, or when an error is discovered
// during initialization.
class FastPairGattServiceClientImpl : public FastPairGattServiceClient {
 public:
  class Factory {
   public:
    static std::unique_ptr<FastPairGattServiceClient> Create(
        device::BluetoothDevice* device,
        scoped_refptr<device::BluetoothAdapter> adapter,
        base::OnceCallback<void(std::optional<PairFailure>)>
            on_initialized_callback);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<FastPairGattServiceClient> CreateInstance(
        device::BluetoothDevice* device,
        scoped_refptr<device::BluetoothAdapter> adapter,
        base::OnceCallback<void(std::optional<PairFailure>)>
            on_initialized_callback) = 0;

   private:
    static Factory* g_test_factory_;
  };

  ~FastPairGattServiceClientImpl() override;

  device::BluetoothRemoteGattService* gatt_service() override;

  bool IsConnected() override;

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

 private:
  FastPairGattServiceClientImpl(
      device::BluetoothDevice* device,
      scoped_refptr<device::BluetoothAdapter> adapter,
      base::OnceCallback<void(std::optional<PairFailure>)>
          on_initialized_callback);
  FastPairGattServiceClientImpl(const FastPairGattServiceClientImpl&) = delete;
  FastPairGattServiceClientImpl& operator=(
      const FastPairGattServiceClientImpl&) = delete;

  // Creates a data vector based on parameter information.
  const std::array<uint8_t, kBlockByteSize> CreateRequest(
      uint8_t message_type,
      uint8_t flags,
      const std::string& provider_address,
      const std::string& seekers_address);
  const std::array<uint8_t, kBlockByteSize> CreatePasskeyBlock(
      uint8_t message_type,
      uint32_t passkey);

  // Attempt to create a GATT connection with the device. This method may be
  // called multiple times.
  void AttemptGattConnection();
  void CreateGattConnection();
  void CoolOffBeforeCreateGattConnection();
  void OnDisconnectTimeout();
  void OnGattServiceDiscoveryTimeout();

  // Callback from the adapter's call to create GATT connection.
  void OnGattConnection(
      base::TimeTicks gatt_connection_start_time,
      std::unique_ptr<device::BluetoothGattConnection> gatt_connection,
      std::optional<device::BluetoothDevice::ConnectErrorCode> error_code);

  // Invokes the initialized callback with the proper PairFailure and clears
  // local state.
  void NotifyInitializedError(PairFailure failure);

  // Invokes the write response callback with the proper PairFailure on a
  // write error.
  void NotifyWriteRequestError(PairFailure failure);
  void NotifyWritePasskeyError(PairFailure failure);
  void NotifyWriteAccountKeyError(ash::quick_pair::AccountKeyFailure failure);

  void ClearCurrentState();

  // BluetoothAdapter::Observer
  void GattDiscoveryCompleteForService(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattService* service) override;
  void GattCharacteristicValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;

  std::optional<PairFailure> SetGattCharacteristics();

  std::vector<device::BluetoothRemoteGattCharacteristic*>
  GetCharacteristicsByUUIDs(const device::BluetoothUUID& uuidV1,
                            const device::BluetoothUUID& uuidV2);

  // Writes the encrypted personalized name packet to the
  // `additional_data_characteristic_`.
  void OnWritePersonalizedNameRequest(
      const std::string& name,
      const std::string& provider_address,
      FastPairDataEncryptor* fast_pair_data_encryptor);

  // Writes `encrypted_request` to `characteristic`.
  void WriteGattCharacteristicWithTimeout(
      device::BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& encrypted_request,
      device::BluetoothRemoteGattCharacteristic::WriteType write_type,
      base::OnceClosure on_timeout,
      base::OnceClosure on_sucess,
      base::OnceCallback<void(device::BluetoothGattService::GattErrorCode)>
          on_failure);

  // Helper functions to WriteGattCharacteristicTimeout.
  // Both stop `characteristic`'s corresponding timer in
  // `characteristic_write_request_timers_` and run `on_success`/`on_failure`
  // callbacks. `on_failure` must take parameter `error`.
  void StopTimerRunSuccess(
      device::BluetoothRemoteGattCharacteristic* characteristic,
      base::OnceClosure on_success);
  void StopTimerRunFailure(
      device::BluetoothRemoteGattCharacteristic* characteristic,
      base::OnceCallback<void(device::BluetoothGattService::GattErrorCode)>
          on_failure,
      device::BluetoothGattService::GattErrorCode error);

  // Stops `characteristic`'s corresponding timer in
  // `characteristic_write_request_timers_` if it exists.
  void StopWriteRequestTimer(
      device::BluetoothRemoteGattCharacteristic* characteristic);

  // Stops all timers in `characteristic_write_request_timers_`.
  void StopAllWriteRequestTimers();

  // BluetoothRemoteGattCharacteristic StartNotifySession callbacks
  void OnKeyBasedRequestNotifySession(
      const std::vector<uint8_t>& request_data,
      std::unique_ptr<device::BluetoothGattNotifySession> session);
  void OnPasskeyNotifySession(
      const std::vector<uint8_t>& passkey_data,
      std::unique_ptr<device::BluetoothGattNotifySession> session);
  void OnNotifySessionError(PairFailure failure,
                            device::BluetoothGattService::GattErrorCode error);

  void OnWriteAdditionalDataTimeout();

  // BluetoothRemoteGattCharacteristic WriteRemoteCharacteristic callbacks
  void OnWriteRequest();
  void OnWriteRequestError(device::BluetoothGattService::GattErrorCode error);
  void OnWritePasskey();
  void OnWritePasskeyError(device::BluetoothGattService::GattErrorCode error);
  void OnWriteAccountKey(base::TimeTicks write_account_key_start_time);
  void OnWriteAccountKeyError(
      device::BluetoothGattService::GattErrorCode error);
  void OnWriteAdditionalData();
  void OnWriteAdditionalDataError(
      device::BluetoothGattService::GattErrorCode error);

  base::OneShotTimer gatt_connect_after_disconnect_cool_off_timer_;
  base::OneShotTimer gatt_disconnect_timer_;
  base::OneShotTimer gatt_service_discovery_timer_;
  base::OneShotTimer passkey_notify_session_timer_;
  base::OneShotTimer keybased_notify_session_timer_;
  base::OneShotTimer key_based_write_request_timer_;

  std::map<device::BluetoothRemoteGattCharacteristic*,
           std::unique_ptr<base::OneShotTimer>>
      characteristic_write_request_timers_;

  base::TimeTicks gatt_service_discovery_start_time_;
  base::TimeTicks passkey_notify_session_start_time_;
  base::TimeTicks keybased_notify_session_start_time_;
  base::TimeTicks passkey_write_request_start_time_;
  base::TimeTicks key_based_write_request_start_time_;

  base::OnceCallback<void(std::optional<PairFailure>)> on_initialized_callback_;
  base::OnceCallback<void(std::vector<uint8_t>, std::optional<PairFailure>)>
      key_based_write_response_callback_;
  base::OnceCallback<void(std::vector<uint8_t>, std::optional<PairFailure>)>
      passkey_write_response_callback_;
  base::OnceCallback<void(std::optional<ash::quick_pair::AccountKeyFailure>)>
      write_account_key_callback_;
  base::OnceCallback<void(std::optional<PairFailure>)>
      write_additional_data_callback_;

  std::string device_address_;
  bool is_initialized_ = false;

  // Initial timestamps used to calculate duration to log to metrics.
  base::TimeTicks notify_keybased_start_time_;
  base::TimeTicks notify_passkey_start_time_;

  raw_ptr<device::BluetoothRemoteGattCharacteristic, DanglingUntriaged>
      model_id_characteristic_ = nullptr;
  raw_ptr<device::BluetoothRemoteGattCharacteristic, DanglingUntriaged>
      key_based_characteristic_ = nullptr;
  raw_ptr<device::BluetoothRemoteGattCharacteristic, DanglingUntriaged>
      passkey_characteristic_ = nullptr;
  raw_ptr<device::BluetoothRemoteGattCharacteristic, DanglingUntriaged>
      account_key_characteristic_ = nullptr;
  raw_ptr<device::BluetoothRemoteGattCharacteristic, DanglingUntriaged>
      additional_data_characteristic_ = nullptr;

  // Initialize with zero failures.
  int num_gatt_connection_attempts_ = 0;

  std::unique_ptr<device::BluetoothGattNotifySession> key_based_notify_session_;
  std::unique_ptr<device::BluetoothGattNotifySession> passkey_notify_session_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  std::unique_ptr<device::BluetoothGattConnection> gatt_connection_;
  raw_ptr<device::BluetoothRemoteGattService, DanglingUntriaged> gatt_service_ =
      nullptr;

  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};
  base::WeakPtrFactory<FastPairGattServiceClientImpl> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_IMPL_H_
