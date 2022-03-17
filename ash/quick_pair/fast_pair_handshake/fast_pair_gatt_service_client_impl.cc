// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
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

constexpr uint8_t kProviderAddressStartIndex = 2;
constexpr uint8_t kSeekerAddressStartIndex = 8;
constexpr uint8_t kSeekerPasskey = 0x02;
constexpr uint8_t kAccountKeyStartByte = 0x04;

constexpr base::TimeDelta kGattOperationTimeout = base::Seconds(5);

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

constexpr const char* ToString(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_CANCELED:
      return "ERROR_AUTH_CANCELED";
    case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_FAILED:
      return "ERROR_AUTH_FAILED";
    case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_REJECTED:
      return "ERROR_AUTH_REJECTED";
    case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_TIMEOUT:
      return "ERROR_AUTH_TIMEOUT";
    case device::BluetoothDevice::ConnectErrorCode::ERROR_FAILED:
      return "ERROR_FAILED";
    case device::BluetoothDevice::ConnectErrorCode::ERROR_INPROGRESS:
      return "ERROR_INPROGRESS";
    case device::BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN:
      return "ERROR_UNKNOWN";
    case device::BluetoothDevice::ConnectErrorCode::ERROR_UNSUPPORTED_DEVICE:
      return "ERROR_UNSUPPORTED_DEVICE";
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
  return base::WrapUnique(new FastPairGattServiceClientImpl(
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

  QP_LOG(INFO) << __func__ << ": Starting the GATT connection to device";
  device->CreateGattConnection(
      base::BindOnce(&FastPairGattServiceClientImpl::OnGattConnection,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()),
      kFastPairBluetoothUuid);
  gatt_service_discovery_timer_.Start(
      FROM_HERE, kGattOperationTimeout,
      base::BindOnce(&FastPairGattServiceClientImpl::NotifyInitializedError,
                     weak_ptr_factory_.GetWeakPtr(),
                     PairFailure::kGattServiceDiscoveryTimeout));
}

FastPairGattServiceClientImpl::~FastPairGattServiceClientImpl() = default;

void FastPairGattServiceClientImpl::OnGattConnection(
    base::TimeTicks gatt_connection_start_time,
    std::unique_ptr<device::BluetoothGattConnection> gatt_connection,
    absl::optional<device::BluetoothDevice::ConnectErrorCode> error_code) {
  RecordGattConnectionResult(/*success=*/!error_code.has_value());

  if (error_code) {
    QP_LOG(WARNING) << "Error creating GATT connection to device: "
                    << ToString(error_code.value());
    RecordGattConnectionErrorCode(error_code.value());
    NotifyInitializedError(PairFailure::kCreateGattConnection);
  } else {
    QP_LOG(INFO) << __func__
                 << ": Successful creation of GATT connection to device";
    RecordTotalGattConnectionTime(base::TimeTicks::Now() -
                                  gatt_connection_start_time);
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
  passkey_write_request_timer_.Stop();
  key_based_write_request_timer_.Stop();
  bluetooth_gatt_notify_sessions_.clear();
}

void FastPairGattServiceClientImpl::NotifyInitializedError(
    PairFailure failure) {
  ClearCurrentState();

  // This function is invoked in several flows and it is possible for it to run
  // twice. In that case, we are ok with the first instance being the one that
  // reports the failure. An example is if we timeout waiting for all notify
  // sessions to start.
  if (on_initialized_callback_)
    std::move(on_initialized_callback_).Run(failure);
}

void FastPairGattServiceClientImpl::NotifyWriteRequestError(
    PairFailure failure) {
  key_based_write_request_timer_.Stop();
  DCHECK(key_based_write_response_callback_);
  std::move(key_based_write_response_callback_)
      .Run(/*response_data=*/{}, failure);
}

void FastPairGattServiceClientImpl::NotifyWritePasskeyError(
    PairFailure failure) {
  passkey_write_request_timer_.Stop();
  DCHECK(passkey_write_response_callback_);
  std::move(passkey_write_response_callback_)
      .Run(/*response_data=*/{}, failure);
}

void FastPairGattServiceClientImpl::NotifyWriteAccountKeyError(
    device::BluetoothGattService::GattErrorCode error) {
  DCHECK(write_account_key_callback_);
  std::move(write_account_key_callback_).Run(error);
}

void FastPairGattServiceClientImpl::GattDiscoveryCompleteForService(
    device::BluetoothAdapter* adapter,
    device::BluetoothRemoteGattService* service) {
  gatt_service_discovery_timer_.Stop();

  // Verify that the discovered service and device are the ones we care about.
  if (service->GetUUID() == kFastPairBluetoothUuid &&
      service->GetDevice()->GetAddress() == device_address_) {
    QP_LOG(INFO) << __func__
                 << ": Completed discovery for Fast Pair GATT service";
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
      FROM_HERE, kGattOperationTimeout,
      base::BindOnce(
          &FastPairGattServiceClientImpl::NotifyInitializedError,
          weak_ptr_factory_.GetWeakPtr(),
          PairFailure::kKeyBasedPairingCharacteristicNotifySessionTimeout));
  passkey_notify_session_timer_.Start(
      FROM_HERE, kGattOperationTimeout,
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
    QP_LOG(INFO) << __func__ << ": Finished initializing GATT service";
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
  QP_LOG(INFO) << __func__ << ": Error: " << ToString(error);
  NotifyInitializedError(failure);
}

device::BluetoothRemoteGattService*
FastPairGattServiceClientImpl::gatt_service() {
  return gatt_service_;
}

const std::array<uint8_t, kBlockByteSize>
FastPairGattServiceClientImpl::CreateRequest(
    uint8_t message_type,
    uint8_t flags,
    const std::string& provider_address,
    const std::string& seekers_address) {
  std::array<uint8_t, kBlockByteSize> data_to_write;
  RAND_bytes(data_to_write.data(), kBlockByteSize);

  data_to_write[0] = message_type;
  data_to_write[1] = flags;

  std::array<uint8_t, 6> provider_address_bytes;
  device::ParseBluetoothAddress(provider_address, provider_address_bytes);
  std::copy(provider_address_bytes.begin(), provider_address_bytes.end(),
            std::begin(data_to_write) + kProviderAddressStartIndex);

  // Seekers address can be empty, in which we would just have the bytes be
  // the salt.
  if (!seekers_address.empty()) {
    std::array<uint8_t, 6> seeker_address_bytes;
    device::ParseBluetoothAddress(seekers_address, seeker_address_bytes);
    std::copy(seeker_address_bytes.begin(), seeker_address_bytes.end(),
              std::begin(data_to_write) + kSeekerAddressStartIndex);
  }

  return data_to_write;
}

const std::array<uint8_t, kBlockByteSize>
FastPairGattServiceClientImpl::CreatePasskeyBlock(uint8_t message_type,
                                                  uint32_t passkey) {
  std::array<uint8_t, kBlockByteSize> data_to_write;
  RAND_bytes(data_to_write.data(), kBlockByteSize);

  data_to_write[0] = message_type;

  // Need to convert the uint_32 to uint_8 to use in our data vector.
  data_to_write[1] = (passkey & 0x00ff0000) >> 16;
  data_to_write[2] = (passkey & 0x0000ff00) >> 8;
  data_to_write[3] = passkey & 0x000000ff;
  return data_to_write;
}

bool FastPairGattServiceClientImpl::IsConnected() {
  return gatt_connection_ && gatt_connection_->IsConnected();
}

void FastPairGattServiceClientImpl::WriteRequestAsync(
    uint8_t message_type,
    uint8_t flags,
    const std::string& provider_address,
    const std::string& seekers_address,
    FastPairDataEncryptor* fast_pair_data_encryptor,
    base::OnceCallback<void(std::vector<uint8_t>, absl::optional<PairFailure>)>
        write_response_callback) {
  DCHECK(is_initialized_);
  DCHECK(!key_based_write_response_callback_);
  DCHECK(fast_pair_data_encryptor);

  key_based_write_response_callback_ = std::move(write_response_callback);

  // We don't need to check that the write response callback exists still before
  // we run the  callback with the timeout PairFailure if the timer fires a call
  // to |NotifyWriteRequestError|. If the callback is used to notify error
  // before the timer expires, |NotifyWriteRequestError| will stop the
  // corresponding timer before it fires here.
  key_based_write_request_timer_.Start(
      FROM_HERE, kGattOperationTimeout,
      base::BindOnce(&FastPairGattServiceClientImpl::NotifyWriteRequestError,
                     weak_ptr_factory_.GetWeakPtr(),
                     PairFailure::kKeyBasedPairingResponseTimeout));

  const std::array<uint8_t, kBlockSizeBytes> data_to_write =
      fast_pair_data_encryptor->EncryptBytes(CreateRequest(
          message_type, flags, provider_address, seekers_address));
  std::vector<uint8_t> data_to_write_vec(data_to_write.begin(),
                                         data_to_write.end());

  // Append the public version of the private key to the message so the device
  // can generate the shared secret to decrypt the message.
  const absl::optional<std::array<uint8_t, 64>> public_key =
      fast_pair_data_encryptor->GetPublicKey();

  if (public_key) {
    const std::vector<uint8_t> public_key_vec = std::vector<uint8_t>(
        public_key.value().begin(), public_key.value().end());
    data_to_write_vec.insert(data_to_write_vec.end(), public_key_vec.begin(),
                             public_key_vec.end());
  }

  notify_keybased_start_time_ = base::TimeTicks::Now();
  key_based_characteristic_->WriteRemoteCharacteristic(
      data_to_write_vec,
      device::BluetoothRemoteGattCharacteristic::WriteType::kWithResponse,
      base::BindOnce(&FastPairGattServiceClientImpl::OnWriteRequest,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FastPairGattServiceClientImpl::OnWriteRequestError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairGattServiceClientImpl::WritePasskeyAsync(
    uint8_t message_type,
    uint32_t passkey,
    FastPairDataEncryptor* fast_pair_data_encryptor,
    base::OnceCallback<void(std::vector<uint8_t>, absl::optional<PairFailure>)>
        write_response_callback) {
  DCHECK(message_type == kSeekerPasskey);
  DCHECK(is_initialized_);
  passkey_write_response_callback_ = std::move(write_response_callback);

  // We don't need to check that the write response callback exists still before
  // we run the  callback with the timeout PairFailure if the timer fires a call
  // to |NotifyWritePasskeyError|. If the callback is used to notify error
  // before the timer expires, |NotifyWritePasskeyError| will stop the
  // corresponding timer before it fires here.
  passkey_write_request_timer_.Start(
      FROM_HERE, kGattOperationTimeout,
      base::BindOnce(&FastPairGattServiceClientImpl::NotifyWritePasskeyError,
                     weak_ptr_factory_.GetWeakPtr(),
                     PairFailure::kPasskeyResponseTimeout));

  const std::array<uint8_t, kBlockSizeBytes> data_to_write =
      fast_pair_data_encryptor->EncryptBytes(
          CreatePasskeyBlock(message_type, passkey));

  notify_passkey_start_time_ = base::TimeTicks::Now();
  passkey_characteristic_->WriteRemoteCharacteristic(
      std::vector<uint8_t>(data_to_write.begin(), data_to_write.end()),
      device::BluetoothRemoteGattCharacteristic::WriteType::kWithResponse,
      base::BindOnce(&FastPairGattServiceClientImpl::OnWritePasskey,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FastPairGattServiceClientImpl::OnWritePasskeyError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairGattServiceClientImpl::WriteAccountKey(
    std::array<uint8_t, 16> account_key,
    FastPairDataEncryptor* fast_pair_data_encryptor,
    base::OnceCallback<
        void(absl::optional<device::BluetoothGattService::GattErrorCode>)>
        write_account_key_callback) {
  DCHECK(account_key[0] == kAccountKeyStartByte);
  DCHECK(is_initialized_);
  write_account_key_callback_ = std::move(write_account_key_callback);

  const std::array<uint8_t, kBlockSizeBytes> data_to_write =
      fast_pair_data_encryptor->EncryptBytes(account_key);

  account_key_characteristic_->WriteRemoteCharacteristic(
      std::vector<uint8_t>(data_to_write.begin(), data_to_write.end()),
      device::BluetoothRemoteGattCharacteristic::WriteType::kWithResponse,
      base::BindOnce(&FastPairGattServiceClientImpl::OnWriteAccountKey,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()),
      base::BindOnce(&FastPairGattServiceClientImpl::OnWriteAccountKeyError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairGattServiceClientImpl::GattCharacteristicValueChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  DCHECK_EQ(adapter, adapter_.get());

  // We check that the callbacks still exists still before we run the
  // it with the response bytes to handle the case where the callback
  // has already been used to notify error. This can happen if the timer for
  // fires with an error, and then the write completes successfully after and
  // we get response bytes here.
  if (characteristic == key_based_characteristic_ &&
      key_based_write_response_callback_) {
    key_based_write_request_timer_.Stop();
    std::move(key_based_write_response_callback_)
        .Run(value, /*failure=*/absl::nullopt);
    RecordNotifyKeyBasedCharacteristicTime(base::TimeTicks::Now() -
                                           notify_keybased_start_time_);
  } else if (characteristic == passkey_characteristic_ &&
             passkey_write_response_callback_) {
    passkey_write_request_timer_.Stop();
    RecordNotifyPasskeyCharacteristicTime(base::TimeTicks::Now() -
                                          notify_passkey_start_time_);
    std::move(passkey_write_response_callback_)
        .Run(value, /*failure=*/absl::nullopt);
  }
}

void FastPairGattServiceClientImpl::OnWriteRequest() {
  QP_LOG(INFO) << __func__;
}

void FastPairGattServiceClientImpl::OnWritePasskey() {
  QP_LOG(INFO) << __func__;
}

void FastPairGattServiceClientImpl::OnWriteRequestError(
    device::BluetoothGattService::GattErrorCode error) {
  QP_LOG(WARNING) << ": Error: " << ToString(error);
  RecordWriteRequestGattError(error);
  NotifyWriteRequestError(PairFailure::kKeyBasedPairingCharacteristicWrite);
}

void FastPairGattServiceClientImpl::OnWritePasskeyError(
    device::BluetoothGattService::GattErrorCode error) {
  QP_LOG(WARNING) << ": Error: " << ToString(error);
  RecordWritePasskeyGattError(error);
  NotifyWritePasskeyError(PairFailure::kPasskeyPairingCharacteristicWrite);
}

void FastPairGattServiceClientImpl::OnWriteAccountKey(
    base::TimeTicks write_account_key_start_time) {
  QP_LOG(INFO) << __func__;
  DCHECK(write_account_key_callback_);
  RecordWriteAccountKeyTime(base::TimeTicks::Now() -
                            write_account_key_start_time);
  std::move(write_account_key_callback_).Run(/*failure=*/absl::nullopt);
}

void FastPairGattServiceClientImpl::OnWriteAccountKeyError(
    device::BluetoothGattService::GattErrorCode error) {
  QP_LOG(WARNING) << __func__ << ": Error: " << ToString(error);
  RecordWriteAccountKeyGattError(error);
  NotifyWriteAccountKeyError(error);
}

}  // namespace quick_pair
}  // namespace ash
