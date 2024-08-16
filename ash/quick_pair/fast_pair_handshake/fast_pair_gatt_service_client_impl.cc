// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "components/cross_device/logging/logging.h"
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
const device::BluetoothUUID kModelIDCharacteristicUuidV1("1233");
const device::BluetoothUUID kModelIDCharacteristicUuidV2(
    "FE2C1233-8366-4814-8EB0-01DE32100BEA");
const device::BluetoothUUID kKeyBasedCharacteristicUuidV1("1234");
const device::BluetoothUUID kKeyBasedCharacteristicUuidV2(
    "FE2C1234-8366-4814-8EB0-01DE32100BEA");
const device::BluetoothUUID kPasskeyCharacteristicUuidV1("1235");
const device::BluetoothUUID kPasskeyCharacteristicUuidV2(
    "FE2C1235-8366-4814-8EB0-01DE32100BEA");
const device::BluetoothUUID kAccountKeyCharacteristicUuidV1("1236");
const device::BluetoothUUID kAccountKeyCharacteristicUuidV2(
    "FE2C1236-8366-4814-8EB0-01DE32100BEA");
const device::BluetoothUUID kAdditionalDataCharacteristicUuidV1("1237");
const device::BluetoothUUID kAdditionalDataCharacteristicUuidV2(
    "FE2C1237-8366-4814-8EB0-01DE32100BEA");

constexpr uint8_t kProviderAddressStartIndex = 2;
constexpr uint8_t kSeekerAddressStartIndex = 8;
constexpr uint8_t kSeekerPasskey = 0x02;
constexpr uint8_t kAccountKeyStartByte = 0x04;

constexpr uint8_t kEmptyFlags = 0x00;

// Action Request constants.
constexpr uint8_t kActionRequestDeviceActionFlags = 0x80;
constexpr uint8_t kActionRequestAdditionalDataFlags = 0x40;
constexpr uint8_t kActionMessage = 0x10;
constexpr size_t kMessageTypeIndex = 0;
constexpr size_t kFlagsIndex = 1;
constexpr size_t kMessageGroupIndex = 8;
constexpr size_t kMessageCodeIndex = 9;
constexpr size_t kDataIdOrSizeIndex = 10;
constexpr size_t kAdditionalDataStartIndex = 11;
constexpr uint8_t kAdditionalDataMaxSizeBytes = 5;

// Personalized Name data request constants.
constexpr uint8_t kPersonalizedNameDataId = 0x10;

constexpr base::TimeDelta kGattOperationTimeout = base::Seconds(15);
constexpr int kMaxNumGattConnectionAttempts = 3;
constexpr base::TimeDelta kCoolOffPeriodBeforeGattConnectionAfterDisconnect =
    base::Seconds(2);
constexpr base::TimeDelta kDisconnectResponseTimeout = base::Seconds(5);

constexpr const char* ErrorCodeToString(
    device::BluetoothGattService::GattErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothGattService::GattErrorCode::kUnknown:
      return "GATT_ERROR_UNKNOWN";
    case device::BluetoothGattService::GattErrorCode::kFailed:
      return "GATT_ERROR_FAILED";
    case device::BluetoothGattService::GattErrorCode::kInProgress:
      return "GATT_ERROR_IN_PROGRESS";
    case device::BluetoothGattService::GattErrorCode::kInvalidLength:
      return "GATT_ERROR_INVALID_LENGTH";
    case device::BluetoothGattService::GattErrorCode::kNotPermitted:
      return "GATT_ERROR_NOT_PERMITTED";
    case device::BluetoothGattService::GattErrorCode::kNotAuthorized:
      return "GATT_ERROR_NOT_AUTHORIZED";
    case device::BluetoothGattService::GattErrorCode::kNotPaired:
      return "GATT_ERROR_NOT_PAIRED";
    case device::BluetoothGattService::GattErrorCode::kNotSupported:
      return "GATT_ERROR_NOT_SUPPORTED";
    default:
      NOTREACHED();
  }
}

constexpr ash::quick_pair::AccountKeyFailure GattErrorCodeToAccountKeyFailure(
    device::BluetoothGattService::GattErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothGattService::GattErrorCode::kUnknown:
      return ash::quick_pair::AccountKeyFailure::kGattErrorUnknown;
    case device::BluetoothGattService::GattErrorCode::kFailed:
      return ash::quick_pair::AccountKeyFailure::kGattErrorFailed;
    case device::BluetoothGattService::GattErrorCode::kInProgress:
      return ash::quick_pair::AccountKeyFailure::kGattInProgress;
    case device::BluetoothGattService::GattErrorCode::kInvalidLength:
      return ash::quick_pair::AccountKeyFailure::kGattErrorInvalidLength;
    case device::BluetoothGattService::GattErrorCode::kNotPermitted:
      return ash::quick_pair::AccountKeyFailure::kGattErrorNotPermitted;
    case device::BluetoothGattService::GattErrorCode::kNotAuthorized:
      return ash::quick_pair::AccountKeyFailure::kGattErrorNotAuthorized;
    case device::BluetoothGattService::GattErrorCode::kNotPaired:
      return ash::quick_pair::AccountKeyFailure::kGattErrorNotPaired;
    case device::BluetoothGattService::GattErrorCode::kNotSupported:
      return ash::quick_pair::AccountKeyFailure::kGattErrorNotSupported;
    default:
      NOTREACHED();
  }
}

constexpr const char* ErrorCodeToString(
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
  }
}

// *** Prefer to use CreateActionRequestBeforeAdditionalData or create analogous
// function for requesting device action using `kActionRequestDeviceActionFlag`.
// ***
//
// Creates Action Request data array based on Table 2.2 of the Fast Pair spec:
// https://developers.google.com/nearby/fast-pair/specifications/characteristics#table1.2.2.
// Note: if the intended `additional_data` vector will have size 0, it is
// sufficient to pass a `nullopt` instead of an empty vector.
const std::array<uint8_t, kBlockByteSize> CreateActionRequest(
    const uint8_t flags,
    const std::string& provider_address,
    std::optional<const uint8_t> message_group,
    std::optional<const uint8_t> message_code,
    std::optional<const uint8_t> data_id_or_size,
    std::optional<const std::vector<uint8_t>> additional_data) {
  std::array<uint8_t, kBlockByteSize> request;

  request[kMessageTypeIndex] = kActionMessage;
  request[kFlagsIndex] = flags;

  // Copy provider address bytes.
  std::array<uint8_t, 6> provider_address_bytes;
  device::ParseBluetoothAddress(provider_address, provider_address_bytes);
  base::ranges::copy(
      provider_address_bytes,
      std::next(std::begin(request), kProviderAddressStartIndex));

  // Construct `request` based on `flags`. Optional values CHECKed for are
  // required based on the flag case.
  switch (flags) {
    case kEmptyFlags:
      break;
    case kActionRequestDeviceActionFlags:
      CHECK(message_group.has_value());
      CHECK(message_code.has_value());
      CHECK(data_id_or_size.has_value());

      request[kMessageGroupIndex] = message_group.value();
      request[kMessageCodeIndex] = message_code.value();

      if (additional_data.has_value()) {
        CHECK(additional_data.value().size() <= kAdditionalDataMaxSizeBytes);
        CHECK(data_id_or_size.value() == additional_data.value().size());
        base::ranges::copy(
            additional_data.value(),
            std::next(std::begin(request), kAdditionalDataStartIndex));
      } else {
        CHECK(data_id_or_size.value() == 0);
      }
      ABSL_FALLTHROUGH_INTENDED;
    case kActionRequestAdditionalDataFlags:
      CHECK(data_id_or_size.has_value());

      request[kDataIdOrSizeIndex] = data_id_or_size.value();
      break;
    default:
      NOTREACHED();
  }

  // Fill unused trailing bytes with random (salt) values.
  if (flags == kActionRequestDeviceActionFlags) {
    // `data_id_or_size` will contain the additional data size in this case.
    if (data_id_or_size.value() < kAdditionalDataMaxSizeBytes) {
      RAND_bytes(request.data() + kAdditionalDataStartIndex +
                     additional_data.value().size(),
                 kAdditionalDataMaxSizeBytes - data_id_or_size.value());
    }
  } else {
    RAND_bytes(request.data() + kAdditionalDataStartIndex,
               kAdditionalDataMaxSizeBytes);
  }

  return request;
}

// Creates Action Request data array with flag 0x40 based on Table 2.2 of the
// Fast Pair spec:
// https://developers.google.com/nearby/fast-pair/specifications/characteristics#table1.2.2.
// The flag indicates that an Additional Data packet write will follow this
// request.
const std::array<uint8_t, kBlockByteSize>
CreateActionRequestBeforeAdditionalData(const std::string& provider_address) {
  return CreateActionRequest(kActionRequestAdditionalDataFlags,
                             provider_address, std::nullopt, std::nullopt,
                             kPersonalizedNameDataId, std::nullopt);
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
    base::OnceCallback<void(std::optional<PairFailure>)>
        on_initialized_callback) {
  if (g_test_factory_) {
    return g_test_factory_->CreateInstance(device, adapter,
                                           std::move(on_initialized_callback));
  }
  auto gatt_service = base::WrapUnique(new FastPairGattServiceClientImpl(
      device, adapter, std::move(on_initialized_callback)));
  if (ash::features::IsFastPairHandshakeLongTermRefactorEnabled()) {
    RecordGattInitializationStep(
        FastPairGattConnectionSteps::kConnectionStarted);
    gatt_service->AttemptGattConnection();
  }
  return gatt_service;
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
    base::OnceCallback<void(std::optional<PairFailure>)>
        on_initialized_callback)
    : on_initialized_callback_(std::move(on_initialized_callback)),
      device_address_(device->GetAddress()),
      adapter_(std::move(adapter)) {
  adapter_observation_.Observe(adapter_.get());

  if (!ash::features::IsFastPairHandshakeLongTermRefactorEnabled()) {
    CD_LOG(INFO, Feature::FP)
        << __func__ << ": Starting the GATT connection to device";
    RecordGattInitializationStep(
        FastPairGattConnectionSteps::kConnectionStarted);
    AttemptGattConnection();
  }
}

FastPairGattServiceClientImpl::~FastPairGattServiceClientImpl() = default;

void FastPairGattServiceClientImpl::AttemptGattConnection() {
  CD_LOG(INFO, Feature::FP) << __func__;

  if (num_gatt_connection_attempts_ == kMaxNumGattConnectionAttempts) {
    NotifyInitializedError(PairFailure::kCreateGattConnection);
    RecordEffectiveGattConnectionSuccess(/*success=*/false);
    return;
  }

  num_gatt_connection_attempts_++;

  CD_LOG(INFO, Feature::FP)
      << __func__ << ": Starting GATT connection attempt #"
      << num_gatt_connection_attempts_ << " to device";

  // Attempt creating a GATT connection with the device.
  auto* device = adapter_->GetDevice(device_address_);
  if (!device) {
    // The device must have been lost between connection attempts.
    NotifyInitializedError(
        PairFailure::kPairingDeviceLostBetweenGattConnectionAttempts);
    return;
  }

  // If we are already bonded and connected, potentially due to attempting to
  // retroactive pair, don't disconnect the device since the device is already
  // in a good working state.
  if (device->IsBonded() && device->IsConnected()) {
    CD_LOG(INFO, Feature::FP)
        << __func__ << ": Device already bonded and connected";
    CreateGattConnection();
    return;
  }

  // Remove any pre-existing GATT connection on the device before we make a
  // new one. We cannot determine if there is a GATT connection already
  // established, and because its not very expensive and has no impact if there
  // is no connection established, we call `Disconnect` regardless.
  CD_LOG(INFO, Feature::FP)
      << __func__ << ": Disconnecting any previous connections before attempt";

  // Start a timer so if we don't get a response from the disconnect call, we
  // still proceed with GATT connection attempts.
  gatt_disconnect_timer_.Start(
      FROM_HERE, kDisconnectResponseTimeout,
      base::BindOnce(&FastPairGattServiceClientImpl::OnDisconnectTimeout,
                     weak_ptr_factory_.GetWeakPtr()));

  device->Disconnect(
      base::BindOnce(
          &FastPairGattServiceClientImpl::CoolOffBeforeCreateGattConnection,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FastPairGattServiceClientImpl::NotifyInitializedError,
                     weak_ptr_factory_.GetWeakPtr(),
                     PairFailure::kFailureToDisconnectGattBetweenRetries));
}

void FastPairGattServiceClientImpl::CoolOffBeforeCreateGattConnection() {
  CD_LOG(INFO, Feature::FP) << __func__;

  if (!gatt_disconnect_timer_.IsRunning()) {
    // The disconnect has already timed out so return early here.
    CD_LOG(INFO, Feature::FP)
        << __func__ << ": Returning early due to prior disconnect timeout.";
    return;
  }

  // A response to the disconnect call was received, stop the associated timer.
  gatt_disconnect_timer_.Stop();

  // In order for the Disconnect to propagate into the platform code, we need
  // a cool off period before we attempt to create a GATT connection in the case
  // we did disconnect an active GATT connection.
  gatt_connect_after_disconnect_cool_off_timer_.Start(
      FROM_HERE, kCoolOffPeriodBeforeGattConnectionAfterDisconnect,
      base::BindOnce(&FastPairGattServiceClientImpl::CreateGattConnection,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairGattServiceClientImpl::OnDisconnectTimeout() {
  // This PairFailure is not surfaced to consumers on the
  // `on_initialized_callback_` because we retry the GATT connection. We don't
  // want the consumers to retry a FastPairHandshake before retries are
  // complete so we log the failure here and continue with the retry if we
  // haven't maxed out yet.
  CD_LOG(INFO, Feature::FP)
      << __func__ << ": reattempting after GATT disconnect timeout: "
      << PairFailure::kDisconnectResponseTimeout;
  RecordGattRetryFailureReason(PairFailure::kDisconnectResponseTimeout);
  AttemptGattConnection();
}

void FastPairGattServiceClientImpl::CreateGattConnection() {
  CD_LOG(INFO, Feature::FP) << __func__;

  // Attempt creating a GATT connection with the device.
  auto* device = adapter_->GetDevice(device_address_);
  if (!device) {
    // The device must have been lost between connection attempts.
    NotifyInitializedError(
        PairFailure::kPairingDeviceLostBetweenGattConnectionAttempts);
    return;
  }
  gatt_service_discovery_start_time_ = base::TimeTicks::Now();
  gatt_service_discovery_timer_.Start(
      FROM_HERE, kGattOperationTimeout,
      base::BindOnce(
          &FastPairGattServiceClientImpl::OnGattServiceDiscoveryTimeout,
          weak_ptr_factory_.GetWeakPtr()));

  device->CreateGattConnection(
      base::BindOnce(&FastPairGattServiceClientImpl::OnGattConnection,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()),
      kFastPairBluetoothUuid);
}

void FastPairGattServiceClientImpl::OnGattServiceDiscoveryTimeout() {
  // This PairFailure is not surfaced to consumers on the
  // `on_initialized_callback_` because we retry the GATT connection. We don't
  // want the consumers to retry a FastPairHandshake before retries are
  // complete so we log the failure here and continue with the retry if we
  // haven't maxed out yet.
  CD_LOG(INFO, Feature::FP)
      << __func__ << ": reattempting from previous GATT connection failure: "
      << PairFailure::kGattServiceDiscoveryTimeout;
  RecordGattRetryFailureReason(PairFailure::kGattServiceDiscoveryTimeout);
  AttemptGattConnection();
}

void FastPairGattServiceClientImpl::OnGattConnection(
    base::TimeTicks gatt_connection_start_time,
    std::unique_ptr<device::BluetoothGattConnection> gatt_connection,
    std::optional<device::BluetoothDevice::ConnectErrorCode> error_code) {
  if (error_code) {
    CD_LOG(WARNING, Feature::FP) << "Error creating GATT connection to device: "
                                 << ErrorCodeToString(error_code.value());
    RecordGattConnectionErrorCode(error_code.value());
    RecordGattConnectionResult(/*success=*/false);

    // This PairFailure is not surfaced to consumers on the
    // `on_initialized_callback_` because we retry the GATT connection. We don't
    // want the consumers to retry a FastPairHandshake before retries are
    // complete so we log the failure here and continue with the retry if we
    // haven't maxed out yet.
    CD_LOG(INFO, Feature::FP)
        << __func__ << ": reattempting from previous GATT connection failure: "
        << PairFailure::kBluetoothDeviceFailureCreatingGattConnection;
    RecordGattRetryFailureReason(
        PairFailure::kBluetoothDeviceFailureCreatingGattConnection);
    AttemptGattConnection();
  } else {
    CD_LOG(INFO, Feature::FP)
        << __func__ << ": Successful creation of GATT connection to device";
    RecordGattConnectionResult(/*success=*/true);
    RecordEffectiveGattConnectionSuccess(/*success=*/true);
    RecordGattConnectionAttemptCount(num_gatt_connection_attempts_);
    RecordTotalGattConnectionTime(base::TimeTicks::Now() -
                                  gatt_connection_start_time);
    gatt_connection_ = std::move(gatt_connection);
  }
}

void FastPairGattServiceClientImpl::ClearCurrentState() {
  adapter_.reset();
  adapter_observation_.Reset();
  gatt_connection_.reset();
  StopAllWriteRequestTimers();
  gatt_service_ = nullptr;
  account_key_characteristic_ = nullptr;
  key_based_characteristic_ = nullptr;
  passkey_characteristic_ = nullptr;
  gatt_service_discovery_timer_.Stop();
  gatt_disconnect_timer_.Stop();
  passkey_notify_session_timer_.Stop();
  keybased_notify_session_timer_.Stop();
  key_based_notify_session_.reset();
  passkey_notify_session_.reset();
}

void FastPairGattServiceClientImpl::NotifyInitializedError(
    PairFailure failure) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << failure;
  ClearCurrentState();

  // This function is invoked in several flows and it is possible for it to run
  // twice. In that case, we are ok with the first instance being the one that
  // reports the failure. An example is if we timeout waiting for all notify
  // sessions to start.
  if (on_initialized_callback_) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << "Executing initialized callback";
    std::move(on_initialized_callback_).Run(failure);
  }
}

void FastPairGattServiceClientImpl::NotifyWriteRequestError(
    PairFailure failure) {
  StopWriteRequestTimer(key_based_characteristic_);

  // |key_based_write_response_callback_| should always exist here.
  // If |OnWriteRequestError| is used to notify error before the timer
  // expires, the timer will be stopped in the line above. If the timer fires
  // after |key_based_write_response_callback_| is called below, |this| will be
  // torn down before |key_based_write_response_callback_| can be called again.
  DCHECK(key_based_write_response_callback_);
  std::move(key_based_write_response_callback_)
      .Run(/*response_data=*/{}, failure);
}

void FastPairGattServiceClientImpl::NotifyWritePasskeyError(
    PairFailure failure) {
  StopWriteRequestTimer(passkey_characteristic_);

  // |passkey_write_response_callback_| should always exist here.
  // If |OnWritePasskeyError| is used to notify error before the timer
  // expires, the timer will be stopped in the line above. If the timer fires
  // after |passkey_write_response_callback_| is called below, |this| will be
  // torn down before |passkey_write_response_callback_| can be called again.
  DCHECK(passkey_write_response_callback_);
  std::move(passkey_write_response_callback_)
      .Run(/*response_data=*/{}, failure);
}

void FastPairGattServiceClientImpl::NotifyWriteAccountKeyError(
    ash::quick_pair::AccountKeyFailure failure) {
  StopWriteRequestTimer(account_key_characteristic_);
  DCHECK(write_account_key_callback_);
  std::move(write_account_key_callback_).Run(failure);
}

void FastPairGattServiceClientImpl::GattDiscoveryCompleteForService(
    device::BluetoothAdapter* adapter,
    device::BluetoothRemoteGattService* service) {
  // Verify that the discovered service and device are the ones we care about.
  if (service->GetUUID() == kFastPairBluetoothUuid &&
      service->GetDevice()->GetAddress() == device_address_) {
    RecordGattServiceDiscoveryTime(base::TimeTicks::Now() -
                                   gatt_service_discovery_start_time_);
    gatt_service_discovery_timer_.Stop();
    CD_LOG(INFO, Feature::FP)
        << __func__ << ": Completed discovery for Fast Pair GATT service";
    RecordGattInitializationStep(FastPairGattConnectionSteps::kConnectionReady);
    gatt_service_ = service;
    auto pair_failure = SetGattCharacteristics();

    if (pair_failure.has_value()) {
      NotifyInitializedError(pair_failure.value());
    } else if (on_initialized_callback_) {
      is_initialized_ = true;
      RecordGattInitializationStep(
          FastPairGattConnectionSteps::kConnectionEstablished);
      std::move(on_initialized_callback_).Run(/*failure=*/std::nullopt);
    }
  }
}

std::vector<device::BluetoothRemoteGattCharacteristic*>
FastPairGattServiceClientImpl::GetCharacteristicsByUUIDs(
    const device::BluetoothUUID& uuidV1,
    const device::BluetoothUUID& uuidV2) {
  if (!gatt_service_)
    return {};

  // Default to V2 device to match Android implementation.
  std::vector<device::BluetoothRemoteGattCharacteristic*> characteristics =
      gatt_service_->GetCharacteristicsByUUID(uuidV2);

  characteristics = characteristics.size()
                        ? characteristics
                        : gatt_service_->GetCharacteristicsByUUID(uuidV1);
  return characteristics;
}

std::optional<PairFailure>
FastPairGattServiceClientImpl::SetGattCharacteristics() {
  auto key_based_characteristics = GetCharacteristicsByUUIDs(
      kKeyBasedCharacteristicUuidV1, kKeyBasedCharacteristicUuidV2);
  if (key_based_characteristics.empty()) {
    return PairFailure::kKeyBasedPairingCharacteristicDiscovery;
  }
  key_based_characteristic_ = key_based_characteristics[0];

  RecordGattInitializationStep(
      FastPairGattConnectionSteps::kFoundKeybasedPairingCharacteristic);

  auto passkey_characteristics = GetCharacteristicsByUUIDs(
      kPasskeyCharacteristicUuidV1, kPasskeyCharacteristicUuidV2);
  if (passkey_characteristics.empty()) {
    return PairFailure::kPasskeyCharacteristicDiscovery;
  }
  passkey_characteristic_ = passkey_characteristics[0];

  auto account_key_characteristics = GetCharacteristicsByUUIDs(
      kAccountKeyCharacteristicUuidV1, kAccountKeyCharacteristicUuidV2);
  if (account_key_characteristics.empty()) {
    return PairFailure::kAccountKeyCharacteristicDiscovery;
  }

  // The account key characteristic does not notify so unlike the other
  // characteristics set in this function, we will not need to create a notify
  // session for it later.
  account_key_characteristic_ = account_key_characteristics[0];

  // The model ID characteristic is required for retroactive pairing for BLE HID
  // devices
  auto model_id_characteristics = GetCharacteristicsByUUIDs(
      kModelIDCharacteristicUuidV1, kModelIDCharacteristicUuidV2);
  if (model_id_characteristics.empty()) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Failed to discover Model ID characteristic.";
  } else {
    model_id_characteristic_ = model_id_characteristics[0];
  }

  auto additional_data_characteristics = GetCharacteristicsByUUIDs(
      kAdditionalDataCharacteristicUuidV1, kAdditionalDataCharacteristicUuidV2);

  // Failure not returned on failure to discover Additional Data characteristic
  // because it shouldn't interrupt the pairing flow. This achieves parity with
  // Android.
  if (additional_data_characteristics.empty()) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Failed to discover Additional Data Characteristic.";
    return std::nullopt;
  }
  additional_data_characteristic_ = additional_data_characteristics[0];

  // No failure.
  return std::nullopt;
}

void FastPairGattServiceClientImpl::OnKeyBasedRequestNotifySession(
    const std::vector<uint8_t>& request_data,
    std::unique_ptr<device::BluetoothGattNotifySession> session) {
  RecordKeyBasedNotifyTime(base::TimeTicks::Now() -
                           keybased_notify_session_start_time_);
  keybased_notify_session_timer_.Stop();
  notify_keybased_start_time_ = base::TimeTicks::Now();

  // The session member variable is set to keep the session from going out of
  // scope and being destroyed.
  key_based_notify_session_ = std::move(session);

  key_based_write_request_start_time_ = base::TimeTicks::Now();

  WriteGattCharacteristicWithTimeout(
      key_based_characteristic_, request_data,
      device::BluetoothRemoteGattCharacteristic::WriteType::kWithResponse,
      base::BindOnce(&FastPairGattServiceClientImpl::NotifyWriteRequestError,
                     weak_ptr_factory_.GetWeakPtr(),
                     PairFailure::kKeyBasedPairingResponseTimeout),
      base::BindOnce(&FastPairGattServiceClientImpl::OnWriteRequest,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FastPairGattServiceClientImpl::OnWriteRequestError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairGattServiceClientImpl::OnPasskeyNotifySession(
    const std::vector<uint8_t>& passkey_data,
    std::unique_ptr<device::BluetoothGattNotifySession> session) {
  RecordPasskeyNotifyTime(base::TimeTicks::Now() -
                          passkey_notify_session_start_time_);
  passkey_notify_session_timer_.Stop();
  notify_passkey_start_time_ = base::TimeTicks::Now();

  // The session member variable is set to keep the session from going out of
  // scope and being destroyed.
  passkey_notify_session_ = std::move(session);

  RecordGattInitializationStep(
      FastPairGattConnectionSteps::kNotifiationsEnabledForKeybasedPairing);

  passkey_write_request_start_time_ = base::TimeTicks::Now();

  WriteGattCharacteristicWithTimeout(
      passkey_characteristic_, passkey_data,
      device::BluetoothRemoteGattCharacteristic::WriteType::kWithResponse,
      base::BindOnce(&FastPairGattServiceClientImpl::NotifyWritePasskeyError,
                     weak_ptr_factory_.GetWeakPtr(),
                     PairFailure::kPasskeyResponseTimeout),
      base::BindOnce(&FastPairGattServiceClientImpl::OnWritePasskey,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FastPairGattServiceClientImpl::OnWritePasskeyError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairGattServiceClientImpl::OnNotifySessionError(
    PairFailure failure,
    device::BluetoothGattService::GattErrorCode error) {
  if (failure == PairFailure::kKeyBasedPairingCharacteristicNotifySession) {
    CD_LOG(INFO, Feature::FP)
        << __func__
        << ": for key based characteristic: " << ErrorCodeToString(error);
    NotifyWriteRequestError(failure);
  } else if (failure == PairFailure::kPasskeyCharacteristicNotifySession) {
    CD_LOG(INFO, Feature::FP)
        << __func__
        << ": for passkey characteristic: " << ErrorCodeToString(error);
    NotifyWritePasskeyError(failure);
  } else {
    NOTREACHED();
  }
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
  base::ranges::copy(provider_address_bytes,
                     std::begin(data_to_write) + kProviderAddressStartIndex);

  // Seekers address can be empty, in which we would just have the bytes be
  // the salt.
  if (!seekers_address.empty()) {
    std::array<uint8_t, 6> seeker_address_bytes;
    device::ParseBluetoothAddress(seekers_address, seeker_address_bytes);
    base::ranges::copy(seeker_address_bytes,
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

void FastPairGattServiceClientImpl::ReadModelIdAsync(
    base::OnceCallback<void(
        std::optional<device::BluetoothGattService::GattErrorCode> error_code,
        const std::vector<uint8_t>& value)> callback) {
  DCHECK(is_initialized_);

  if (!model_id_characteristic_) {
    std::move(callback).Run(
        device::BluetoothGattService::GattErrorCode::kNotSupported,
        std::vector<uint8_t>{});
    return;
  }

  model_id_characteristic_->ReadRemoteCharacteristic(std::move(callback));
}

void FastPairGattServiceClientImpl::WriteRequestAsync(
    uint8_t message_type,
    uint8_t flags,
    const std::string& provider_address,
    const std::string& seekers_address,
    FastPairDataEncryptor* fast_pair_data_encryptor,
    base::OnceCallback<void(std::vector<uint8_t>, std::optional<PairFailure>)>
        write_response_callback) {
  DCHECK(is_initialized_);
  DCHECK(!key_based_write_response_callback_);
  DCHECK(fast_pair_data_encryptor);

  // The key based request should only ever be written once; if the notify
  // session has already been set, something has gone wrong.
  DCHECK(!key_based_notify_session_);

  key_based_write_response_callback_ = std::move(write_response_callback);

  const std::array<uint8_t, kBlockSizeBytes> data_to_write =
      fast_pair_data_encryptor->EncryptBytes(CreateRequest(
          message_type, flags, provider_address, seekers_address));
  std::vector<uint8_t> data_to_write_vec(data_to_write.begin(),
                                         data_to_write.end());

  // Append the public version of the private key to the message so the device
  // can generate the shared secret to decrypt the message.
  const std::optional<std::array<uint8_t, 64>> public_key =
      fast_pair_data_encryptor->GetPublicKey();

  if (public_key) {
    const std::vector<uint8_t> public_key_vec = std::vector<uint8_t>(
        public_key.value().begin(), public_key.value().end());
    data_to_write_vec.insert(data_to_write_vec.end(), public_key_vec.begin(),
                             public_key_vec.end());
  }

  keybased_notify_session_start_time_ = base::TimeTicks::Now();
  keybased_notify_session_timer_.Start(
      FROM_HERE, kGattOperationTimeout,
      base::BindOnce(
          &FastPairGattServiceClientImpl::NotifyWriteRequestError,
          weak_ptr_factory_.GetWeakPtr(),
          PairFailure::kKeyBasedPairingCharacteristicNotifySessionTimeout));

  key_based_characteristic_->StartNotifySession(
      base::BindOnce(
          &FastPairGattServiceClientImpl::OnKeyBasedRequestNotifySession,
          weak_ptr_factory_.GetWeakPtr(), data_to_write_vec),
      base::BindOnce(&FastPairGattServiceClientImpl::OnNotifySessionError,
                     weak_ptr_factory_.GetWeakPtr(),
                     PairFailure::kKeyBasedPairingCharacteristicNotifySession));
}

void FastPairGattServiceClientImpl::WritePasskeyAsync(
    uint8_t message_type,
    uint32_t passkey,
    FastPairDataEncryptor* fast_pair_data_encryptor,
    base::OnceCallback<void(std::vector<uint8_t>, std::optional<PairFailure>)>
        write_response_callback) {
  DCHECK(is_initialized_);
  DCHECK(message_type == kSeekerPasskey);

  // |passkey_notify_session| might already exist since it happens after the
  // handshake completes, meaning a reused handshake may already have a
  // |passkey_notify_session|. Therefore, do not DCHECK that it doesn't exist.

  passkey_write_response_callback_ = std::move(write_response_callback);

  const std::array<uint8_t, kBlockSizeBytes> data_to_write =
      fast_pair_data_encryptor->EncryptBytes(
          CreatePasskeyBlock(message_type, passkey));
  std::vector<uint8_t> data_to_write_vec(data_to_write.begin(),
                                         data_to_write.end());

  passkey_notify_session_start_time_ = base::TimeTicks::Now();
  passkey_notify_session_timer_.Start(
      FROM_HERE, kGattOperationTimeout,
      base::BindOnce(&FastPairGattServiceClientImpl::NotifyWritePasskeyError,
                     weak_ptr_factory_.GetWeakPtr(),
                     PairFailure::kPasskeyCharacteristicNotifySessionTimeout));

  passkey_characteristic_->StartNotifySession(
      base::BindOnce(&FastPairGattServiceClientImpl::OnPasskeyNotifySession,
                     weak_ptr_factory_.GetWeakPtr(), data_to_write_vec),
      base::BindOnce(&FastPairGattServiceClientImpl::OnNotifySessionError,
                     weak_ptr_factory_.GetWeakPtr(),
                     PairFailure::kPasskeyCharacteristicNotifySession));
}

void FastPairGattServiceClientImpl::WriteAccountKey(
    std::array<uint8_t, 16> account_key,
    FastPairDataEncryptor* fast_pair_data_encryptor,
    base::OnceCallback<void(std::optional<ash::quick_pair::AccountKeyFailure>)>
        write_account_key_callback) {
  DCHECK(account_key[0] == kAccountKeyStartByte);
  DCHECK(is_initialized_);
  write_account_key_callback_ = std::move(write_account_key_callback);

  const std::array<uint8_t, kBlockSizeBytes> data_to_write =
      fast_pair_data_encryptor->EncryptBytes(account_key);

  WriteGattCharacteristicWithTimeout(
      account_key_characteristic_,
      std::vector<uint8_t>(data_to_write.begin(), data_to_write.end()),
      device::BluetoothRemoteGattCharacteristic::WriteType::kWithResponse,
      base::BindOnce(&FastPairGattServiceClientImpl::NotifyWriteAccountKeyError,
                     weak_ptr_factory_.GetWeakPtr(),
                     ash::quick_pair::AccountKeyFailure::
                         kAccountKeyCharacteristicWriteTimeout),
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
    RecordKeyBasedWriteRequestTime(base::TimeTicks::Now() -
                                   key_based_write_request_start_time_);
    StopWriteRequestTimer(key_based_characteristic_);
    std::move(key_based_write_response_callback_)
        .Run(value, /*failure=*/std::nullopt);
    RecordNotifyKeyBasedCharacteristicTime(base::TimeTicks::Now() -
                                           notify_keybased_start_time_);
  } else if (characteristic == passkey_characteristic_ &&
             passkey_write_response_callback_) {
    RecordPasskeyWriteRequestTime(base::TimeTicks::Now() -
                                  passkey_write_request_start_time_);
    StopWriteRequestTimer(passkey_characteristic_);
    RecordNotifyPasskeyCharacteristicTime(base::TimeTicks::Now() -
                                          notify_passkey_start_time_);
    std::move(passkey_write_response_callback_)
        .Run(value, /*failure=*/std::nullopt);
  }
}

void FastPairGattServiceClientImpl::OnWriteRequest() {
  CD_LOG(INFO, Feature::FP) << __func__;
}

void FastPairGattServiceClientImpl::OnWritePasskey() {
  CD_LOG(INFO, Feature::FP) << __func__;
}

void FastPairGattServiceClientImpl::OnWriteRequestError(
    device::BluetoothGattService::GattErrorCode error) {
  CD_LOG(WARNING, Feature::FP) << ": Error: " << ErrorCodeToString(error);
  RecordWriteRequestGattError(error);
  NotifyWriteRequestError(PairFailure::kKeyBasedPairingCharacteristicWrite);
}

void FastPairGattServiceClientImpl::OnWritePasskeyError(
    device::BluetoothGattService::GattErrorCode error) {
  CD_LOG(WARNING, Feature::FP) << ": Error: " << ErrorCodeToString(error);
  RecordWritePasskeyGattError(error);
  NotifyWritePasskeyError(PairFailure::kPasskeyPairingCharacteristicWrite);
}

void FastPairGattServiceClientImpl::OnWriteAccountKey(
    base::TimeTicks write_account_key_start_time) {
  StopWriteRequestTimer(account_key_characteristic_);
  CD_LOG(INFO, Feature::FP) << __func__;
  RecordWriteAccountKeyTime(base::TimeTicks::Now() -
                            write_account_key_start_time);
  std::move(write_account_key_callback_).Run(/*failure=*/std::nullopt);
}

void FastPairGattServiceClientImpl::OnWriteAccountKeyError(
    device::BluetoothGattService::GattErrorCode error) {
  CD_LOG(WARNING, Feature::FP)
      << __func__ << ": Error: " << ErrorCodeToString(error);
  RecordWriteAccountKeyGattError(error);
  NotifyWriteAccountKeyError(GattErrorCodeToAccountKeyFailure(error));
  // |this| may be destroyed after this line.
}

// TODO(b/297104920): ensure this is not called if
// `additional_data_characteristic_` is not discovered.
void FastPairGattServiceClientImpl::WritePersonalizedName(
    const std::string& name,
    const std::string& provider_address,
    FastPairDataEncryptor* fast_pair_data_encryptor,
    base::OnceCallback<void(std::optional<PairFailure>)>
        write_additional_data_callback) {
  DCHECK(write_additional_data_callback_.is_null());
  write_additional_data_callback_ = std::move(write_additional_data_callback);

  // Write Action Request to inform the Additional Data characteristic that the
  // next message is the personalized name.
  const std::array<uint8_t, kBlockSizeBytes> encrypted_data =
      fast_pair_data_encryptor->EncryptBytes(
          CreateActionRequestBeforeAdditionalData(provider_address));

  std::vector<uint8_t> encrypted_request;
  encrypted_request.reserve(kBlockSizeBytes);

  encrypted_request.insert(
      encrypted_request.end(), std::begin(encrypted_data),
      std::next(std::begin(encrypted_data), kBlockSizeBytes));

  WriteGattCharacteristicWithTimeout(
      additional_data_characteristic_, encrypted_request,
      device::BluetoothRemoteGattCharacteristic::WriteType::kWithResponse,
      base::BindOnce(
          &FastPairGattServiceClientImpl::OnWriteAdditionalDataTimeout,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &FastPairGattServiceClientImpl::OnWritePersonalizedNameRequest,
          weak_ptr_factory_.GetWeakPtr(), name, provider_address,
          fast_pair_data_encryptor),
      base::BindOnce(&FastPairGattServiceClientImpl::OnWriteAdditionalDataError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairGattServiceClientImpl::WriteGattCharacteristicWithTimeout(
    device::BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& encrypted_request,
    device::BluetoothRemoteGattCharacteristic::WriteType write_type,
    base::OnceClosure on_timeout,
    base::OnceClosure on_success,
    base::OnceCallback<void(device::BluetoothGattService::GattErrorCode)>
        on_failure) {
  CHECK(!characteristic_write_request_timers_.contains(characteristic) ||
        !characteristic_write_request_timers_[characteristic]->IsRunning());
  std::unique_ptr<base::OneShotTimer> timer =
      std::make_unique<base::OneShotTimer>();
  timer->Start(FROM_HERE, kGattOperationTimeout, std::move(on_timeout));
  characteristic_write_request_timers_.insert(
      {characteristic, std::move(timer)});
  characteristic->WriteRemoteCharacteristic(
      encrypted_request, write_type,
      base::BindOnce(&FastPairGattServiceClientImpl::StopTimerRunSuccess,
                     weak_ptr_factory_.GetWeakPtr(), characteristic,
                     std::move(on_success)),
      base::BindOnce(&FastPairGattServiceClientImpl::StopTimerRunFailure,
                     weak_ptr_factory_.GetWeakPtr(), characteristic,
                     std::move(on_failure)));
}

void FastPairGattServiceClientImpl::StopTimerRunSuccess(
    device::BluetoothRemoteGattCharacteristic* characteristic,
    base::OnceClosure on_success) {
  StopWriteRequestTimer(characteristic);
  std::move(on_success).Run();
}

void FastPairGattServiceClientImpl::StopTimerRunFailure(
    device::BluetoothRemoteGattCharacteristic* characteristic,
    base::OnceCallback<void(device::BluetoothGattService::GattErrorCode)>
        on_failure,
    device::BluetoothGattService::GattErrorCode error) {
  StopWriteRequestTimer(characteristic);
  std::move(on_failure).Run(/*error=*/error);
}

void FastPairGattServiceClientImpl::StopAllWriteRequestTimers() {
  for (auto& [characteristic, timer] : characteristic_write_request_timers_) {
    if (timer->IsRunning()) {
      timer->Stop();
    }
  }
}

void FastPairGattServiceClientImpl::StopWriteRequestTimer(
    device::BluetoothRemoteGattCharacteristic* characteristic) {
  if (characteristic_write_request_timers_.contains(characteristic)) {
    characteristic_write_request_timers_[characteristic]->Stop();
    characteristic_write_request_timers_.erase(characteristic);
  }
}

void FastPairGattServiceClientImpl::OnWriteAdditionalData() {
  CD_LOG(VERBOSE, Feature::FP) << __func__;
  std::move(write_additional_data_callback_).Run(/*error=*/std::nullopt);
}

void FastPairGattServiceClientImpl::OnWriteAdditionalDataError(
    device::BluetoothGattService::GattErrorCode error) {
  CD_LOG(WARNING, Feature::FP) << ": Error: " << ErrorCodeToString(error);
  std::move(write_additional_data_callback_)
      .Run(
          /*error=*/PairFailure::kAdditionalDataCharacteristicWrite);
}

void FastPairGattServiceClientImpl::OnWriteAdditionalDataTimeout() {
  CD_LOG(WARNING, Feature::FP) << __func__;
  std::move(write_additional_data_callback_)
      .Run(
          /*error=*/PairFailure::kAdditionalDataCharacteristicWriteTimeout);
}

void FastPairGattServiceClientImpl::OnWritePersonalizedNameRequest(
    const std::string& name,
    const std::string& provider_address,
    FastPairDataEncryptor* fast_pair_data_encryptor) {
  CD_LOG(VERBOSE, Feature::FP) << __func__;
  std::array<uint8_t, kNonceSizeBytes> nonce;
  RAND_bytes(nonce.data(), kNonceSizeBytes);
  const std::vector<uint8_t> name_bytes(name.begin(), name.end());

  const std::vector<uint8_t> additional_data_packet =
      fast_pair_data_encryptor->CreateAdditionalDataPacket(nonce, name_bytes);

  WriteGattCharacteristicWithTimeout(
      additional_data_characteristic_, additional_data_packet,
      device::BluetoothRemoteGattCharacteristic::WriteType::kWithResponse,
      base::BindOnce(
          &FastPairGattServiceClientImpl::OnWriteAdditionalDataTimeout,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FastPairGattServiceClientImpl::OnWriteAdditionalData,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FastPairGattServiceClientImpl::OnWriteAdditionalDataError,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace quick_pair
}  // namespace ash
