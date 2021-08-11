// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_gatt_service_client_impl.h"

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/logging.h"
#include "base/time/time.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

namespace {

// We have two UUID possibilities for each characteristic because they changed
// across different Fast Pair versions.
const device::BluetoothUUID kKeyBasedCharacteristicUuidV1("1234");
const device::BluetoothUUID kKeyBasedCharacteristicUuidV2(
    "FE2C1234-8366-4814-8EB0-01DE32100BEA");
const device::BluetoothUUID kPasskeyCharacteristicUuidV1("1235");
const device::BluetoothUUID kPasskeyCharacteristicUuidV2(
    "FE2C1235-8366-4814-8EB0-01DE32100BEA");
const device::BluetoothUUID kAccountKeyCharacteristicUuidV1("1236");
const device::BluetoothUUID kAccountKeyCharacteristicUuidV2(
    "FE2C1236-8366-4814-8EB0-01DE32100BEA");

constexpr uint8_t kRequestByteSize = 16;
constexpr uint8_t kSaltByteSize = 3;
constexpr uint8_t kProviderAddressStartIndex = 2;
constexpr uint8_t kSeekerAddressStartIndex = 8;
constexpr uint8_t kSaltStartIndex = 13;

constexpr base::TimeDelta kConnectingTimeout = base::TimeDelta::FromSeconds(5);

constexpr const char* ToString(
    device::BluetoothGattService::GattErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothGattService::GATT_ERROR_UNKNOWN:
      return "GATT_ERROR_UNKNOWN";
    case device::BluetoothGattService::GATT_ERROR_FAILED:
      return "GATT_ERROR_FAILED";
    case device::BluetoothGattService::GATT_ERROR_IN_PROGRESS:
      return "GATT_ERROR_IN_PROGRESS";
    case device::BluetoothGattService::GATT_ERROR_INVALID_LENGTH:
      return "GATT_ERROR_INVALID_LENGTH";
    case device::BluetoothGattService::GATT_ERROR_NOT_PERMITTED:
      return "GATT_ERROR_NOT_PERMITTED";
    case device::BluetoothGattService::GATT_ERROR_NOT_AUTHORIZED:
      return "GATT_ERROR_NOT_AUTHORIZED";
    case device::BluetoothGattService::GATT_ERROR_NOT_PAIRED:
      return "GATT_ERROR_NOT_PAIRED";
    case device::BluetoothGattService::GATT_ERROR_NOT_SUPPORTED:
      return "GATT_ERROR_NOT_SUPPORTED";
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace

namespace ash {
namespace quick_pair {

// static
FastPairGattServiceClientImpl::Factory*
    FastPairGattServiceClientImpl::Factory::g_test_factory_ = nullptr;

// static
std::unique_ptr<FastPairGattServiceClient>
FastPairGattServiceClientImpl::Factory::Create(
    device::BluetoothDevice* device,
    scoped_refptr<device::BluetoothAdapter> adapter,
    base::OnceCallback<void(absl::optional<PairFailure>)>
        on_initialized_callback) {
  if (g_test_factory_) {
    return g_test_factory_->CreateInstance(device, adapter,
                                           std::move(on_initialized_callback));
  }
  return absl::WrapUnique(new FastPairGattServiceClientImpl(
      device, adapter, std::move(on_initialized_callback)));
}

// static
void FastPairGattServiceClientImpl::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairGattServiceClientImpl::Factory::~Factory() = default;

FastPairGattServiceClientImpl::FastPairGattServiceClientImpl(
    device::BluetoothDevice* device,
    scoped_refptr<device::BluetoothAdapter> adapter,
    base::OnceCallback<void(absl::optional<PairFailure>)>
        on_initialized_callback)
    : on_initialized_callback_(std::move(on_initialized_callback)),
      device_address_(device->GetAddress()),
      adapter_(std::move(adapter)) {
  adapter_observation_.Observe(adapter_.get());

  QP_LOG(VERBOSE) << "Starting the GATT connection to device at address:["
                  << device_address_ << "].";
  device->CreateGattConnection(
      base::BindOnce(&FastPairGattServiceClientImpl::OnGattConnection,
                     weak_ptr_factory_.GetWeakPtr()),
      kFastPairBluetoothUuid);
  gatt_service_discovery_timer_.Start(
      FROM_HERE, kConnectingTimeout,
      base::BindOnce(&FastPairGattServiceClientImpl::NotifyInitializedError,
                     weak_ptr_factory_.GetWeakPtr(),
                     PairFailure::kGattServiceDiscoveryTimeout));
}

FastPairGattServiceClientImpl::~FastPairGattServiceClientImpl() = default;

void FastPairGattServiceClientImpl::OnGattConnection(
    std::unique_ptr<device::BluetoothGattConnection> gatt_connection,
    absl::optional<device::BluetoothDevice::ConnectErrorCode> error_code) {
  if (error_code) {
    QP_LOG(WARNING) << "Error creating GATT connection to device at address:["
                    << device_address_ << "].";
    NotifyInitializedError(PairFailure::kCreateGattConnection);
  } else {
    QP_LOG(VERBOSE)
        << "Successful creation of GATT connection to device at address:["
        << device_address_ << "].";
    gatt_connection_ = std::move(gatt_connection);
  }
}

void FastPairGattServiceClientImpl::ClearCurrentState() {
  adapter_.reset();
  adapter_observation_.Reset();
  gatt_connection_.reset();
  gatt_service_ = nullptr;
  account_key_characteristic_ = nullptr;
  key_based_characteristic_ = nullptr;
  passkey_characteristic_ = nullptr;
  gatt_service_discovery_timer_.Stop();
  passkey_notify_session_timer_.Stop();
  keybased_notify_session_timer_.Stop();
  bluetooth_gatt_notify_sessions_.clear();
}

void FastPairGattServiceClientImpl::NotifyInitializedError(
    PairFailure failure) {
  ClearCurrentState();
  DCHECK(on_initialized_callback_);
  std::move(on_initialized_callback_).Run(failure);
}

void FastPairGattServiceClientImpl::NotifyWriteError(PairFailure failure) {
  DCHECK(key_based_write_response_callback_);
  std::move(key_based_write_response_callback_)
      .Run(/*response_data=*/{}, failure);
}

void FastPairGattServiceClientImpl::GattDiscoveryCompleteForService(
    device::BluetoothAdapter* adapter,
    device::BluetoothRemoteGattService* service) {
  gatt_service_discovery_timer_.Stop();

  // Verify that the discovered service and device are the ones we care about.
  if (service->GetUUID() == kFastPairBluetoothUuid &&
      service->GetDevice()->GetAddress() == device_address_) {
    QP_LOG(VERBOSE)
        << "GATT discovery complete for service related to device at address:["
        << device_address_ << "].";
    gatt_service_ = service;
    FindGattCharacteristicsAndStartNotifySessions();
  }
}

std::vector<device::BluetoothRemoteGattCharacteristic*>
FastPairGattServiceClientImpl::GetCharacteristicsByUUIDs(
    const device::BluetoothUUID& uuidV1,
    const device::BluetoothUUID& uuidV2) {
  if (!gatt_service_)
    return {};

  std::vector<device::BluetoothRemoteGattCharacteristic*> characteristics =
      gatt_service_->GetCharacteristicsByUUID(uuidV1);
  characteristics = characteristics.size()
                        ? characteristics
                        : gatt_service_->GetCharacteristicsByUUID(uuidV2);
  return characteristics;
}

void FastPairGattServiceClientImpl::
    FindGattCharacteristicsAndStartNotifySessions() {
  std::vector<device::BluetoothRemoteGattCharacteristic*>
      key_based_characteristics = GetCharacteristicsByUUIDs(
          kKeyBasedCharacteristicUuidV1, kKeyBasedCharacteristicUuidV2);
  if (key_based_characteristics.empty()) {
    NotifyInitializedError(
        PairFailure::kKeyBasedPairingCharacteristicDiscovery);
    return;
  }

  std::vector<device::BluetoothRemoteGattCharacteristic*>
      passkey_characteristics = GetCharacteristicsByUUIDs(
          kPasskeyCharacteristicUuidV1, kPasskeyCharacteristicUuidV2);
  if (passkey_characteristics.empty()) {
    NotifyInitializedError(PairFailure::kPasskeyCharacteristicDiscovery);
    return;
  }

  std::vector<device::BluetoothRemoteGattCharacteristic*>
      account_key_characteristics = GetCharacteristicsByUUIDs(
          kAccountKeyCharacteristicUuidV1, kAccountKeyCharacteristicUuidV2);
  if (account_key_characteristics.empty()) {
    NotifyInitializedError(PairFailure::kAccountKeyCharacteristicDiscovery);
    return;
  }
  account_key_characteristic_ = account_key_characteristics[0];

  keybased_notify_session_timer_.Start(
      FROM_HERE, kConnectingTimeout,
      base::BindOnce(
          &FastPairGattServiceClientImpl::NotifyInitializedError,
          weak_ptr_factory_.GetWeakPtr(),
          PairFailure::kKeyBasedPairingCharacteristicNotifySessionTimeout));
  passkey_notify_session_timer_.Start(
      FROM_HERE, kConnectingTimeout,
      base::BindOnce(&FastPairGattServiceClientImpl::NotifyInitializedError,
                     weak_ptr_factory_.GetWeakPtr(),
                     PairFailure::kPasskeyCharacteristicNotifySessionTimeout));

  key_based_characteristic_ = key_based_characteristics[0];
  key_based_characteristic_->StartNotifySession(
      base::BindOnce(&FastPairGattServiceClientImpl::OnNotifySession,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FastPairGattServiceClientImpl::OnGattError,
                     weak_ptr_factory_.GetWeakPtr(),
                     PairFailure::kKeyBasedPairingCharacteristicNotifySession));

  passkey_characteristic_ = passkey_characteristics[0];
  passkey_characteristic_->StartNotifySession(
      base::BindOnce(&FastPairGattServiceClientImpl::OnNotifySession,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FastPairGattServiceClientImpl::OnGattError,
                     weak_ptr_factory_.GetWeakPtr(),
                     PairFailure::kPasskeyCharacteristicNotifySession));
}

void FastPairGattServiceClientImpl::OnNotifySession(
    std::unique_ptr<device::BluetoothGattNotifySession> session) {
  // Check which characteristic the session corresponds to and stop the relevant
  // timer.
  if (key_based_characteristic_ &&
      session->GetCharacteristic() == key_based_characteristic_) {
    keybased_notify_session_timer_.Stop();
  } else if (passkey_characteristic_ &&
             session->GetCharacteristic() == passkey_characteristic_) {
    passkey_notify_session_timer_.Stop();
  }

  bluetooth_gatt_notify_sessions_.push_back(std::move(session));

  // Once expected characteristics are notifying, Run the callback with no
  // error. Here, we are waiting for both the key based characteristics and the
  // pass key characteristics to notify, thus size "2";
  if (bluetooth_gatt_notify_sessions_.size() == 2) {
    QP_LOG(VERBOSE) << "GATT service is ready for device at address:["
                    << device_address_ << "].";
    is_initialized_ = true;

    // This check handles the case where a timer for the characteristic's notify
    // session fires and the |on_initialized_callback_| callback has been used
    // to send a PairFailure, but the notify session is received here
    // afterwards.
    if (on_initialized_callback_)
      std::move(on_initialized_callback_).Run(absl::nullopt);
  }
}

void FastPairGattServiceClientImpl::OnGattError(
    PairFailure failure,
    device::BluetoothGattService::GattErrorCode error) {
  QP_LOG(VERBOSE) << "StartNotifySession failed due to GATT error: "
                  << ToString(error);
  NotifyInitializedError(failure);
}

device::BluetoothRemoteGattService*
FastPairGattServiceClientImpl::gatt_service() {
  return gatt_service_;
}

std::vector<uint8_t> FastPairGattServiceClientImpl::CreateRequest(
    uint8_t message_type,
    uint8_t flags,
    const std::string& provider_address,
    const std::string& seekers_address) {
  std::vector<uint8_t> data_to_write(kRequestByteSize);

  data_to_write[0] = message_type;
  data_to_write[1] = flags;

  std::copy(provider_address.begin(), provider_address.end(),
            data_to_write.begin() + kProviderAddressStartIndex);
  std::copy(seekers_address.begin(), seekers_address.end(),
            data_to_write.begin() + kSeekerAddressStartIndex);

  uint8_t salt[kSaltByteSize];
  RAND_bytes(salt, kSaltByteSize);
  std::copy(std::begin(salt), std::end(salt),
            data_to_write.begin() + kSaltStartIndex);

  return data_to_write;
}

void FastPairGattServiceClientImpl::WriteRequestAsync(
    uint8_t message_type,
    uint8_t flags,
    const std::string& provider_address,
    const std::string& seekers_address,
    base::OnceCallback<void(std::vector<uint8_t>, absl::optional<PairFailure>)>
        write_response_callback) {
  DCHECK(is_initialized_);
  DCHECK(!key_based_write_response_callback_);

  key_based_write_response_callback_ = std::move(write_response_callback);
  key_based_characteristic_->WriteRemoteCharacteristic(
      CreateRequest(message_type, flags, provider_address, seekers_address),
      device::BluetoothRemoteGattCharacteristic::WriteType::kWithResponse,
      base::BindOnce(&FastPairGattServiceClientImpl::OnWriteRequest,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FastPairGattServiceClientImpl::OnWriteRequestError,
                     weak_ptr_factory_.GetWeakPtr(),
                     PairFailure::kKeyBasedPairingCharacteristicWrite));
}

void FastPairGattServiceClientImpl::GattCharacteristicValueChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  DCHECK_EQ(adapter, adapter_.get());

  if (characteristic == key_based_characteristic_ &&
      key_based_write_response_callback_) {
    std::move(key_based_write_response_callback_)
        .Run(value, /*failure=*/absl::nullopt);
  }
}

void FastPairGattServiceClientImpl::OnWriteRequest() {
  QP_LOG(VERBOSE) << "WriteRemoteCharacteristic to key-based pairing "
                     "characteristic successful.";
}

void FastPairGattServiceClientImpl::OnWriteRequestError(
    PairFailure failure,
    device::BluetoothGattService::GattErrorCode error) {
  QP_LOG(WARNING) << "WriteRemoteCharacteristic to key-based pairing "
                     "characteristic failed due to GATT error: "
                  << ToString(error);

  NotifyWriteError(failure);
}

}  // namespace quick_pair
}  // namespace ash
