// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"

#include "ash/quick_pair/common/device.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"

namespace {

// Error strings should be kept in sync with the strings reflected in
// device/bluetooth/bluez/bluetooth_socket_bluez.cc.
const char kAcceptFailedString[] = "Failed to accept connection.";
const char kInvalidUUIDString[] = "Invalid UUID";
const char kSocketNotListeningString[] = "Socket is not listening.";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This enum should be kept in sync
// with the BluetoothConnectToServiceError enum in
// src/tools/metrics/histograms/enums.xml.
enum class ConnectToServiceError {
  kUnknownError = 0,
  kAcceptFailed = 1,
  kInvalidUUID = 2,
  kSocketNotListening = 3,
  kMaxValue = kSocketNotListening,
};

ConnectToServiceError GetConnectToServiceError(const std::string& error) {
  if (error == kAcceptFailedString)
    return ConnectToServiceError::kAcceptFailed;

  if (error == kInvalidUUIDString)
    return ConnectToServiceError::kInvalidUUID;

  if (error == kSocketNotListeningString)
    return ConnectToServiceError::kSocketNotListening;

  DCHECK(error != kSocketNotListeningString && error != kInvalidUUIDString &&
         error != kAcceptFailedString);
  return ConnectToServiceError::kUnknownError;
}

const char kEngagementFlowInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.EngagementFunnel.Steps.InitialPairingProtocol";
const char kEngagementFlowSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.EngagementFunnel.Steps."
    "SubsequentPairingProtocol";
const char kTotalUxPairTimeInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.TotalUxPairTime.InitialPairingProtocol";
const char kTotalUxPairTimeSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.TotalUxPairTime.SubsequentPairingProtocol";
const char kRetroactiveEngagementFlowMetric[] =
    "Bluetooth.ChromeOS.FastPair.RetroactiveEngagementFunnel.Steps";
const char kPairingMethodMetric[] = "Bluetooth.ChromeOS.FastPair.PairingMethod";
const char kRetroactivePairingResultMetric[] =
    "Bluetooth.ChromeOS.FastPair.RetroactivePairing.Result";
const char kTotalGattConnectionTimeMetric[] =
    "Bluetooth.ChromeOS.FastPair.TotalGattConnectionTime";
const char kGattConnectionResult[] =
    "Bluetooth.ChromeOS.FastPair.GattConnection.Result";
const char kGattConnectionErrorMetric[] =
    "Bluetooth.ChromeOS.FastPair.GattConnection.ErrorReason";
const char kFastPairPairFailureInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.PairFailure.InitialPairingProtocol";
const char kFastPairPairFailureSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.PairFailure.SubsequentPairingProtocol";
const char kFastPairPairFailureRetroactiveMetric[] =
    "Bluetooth.ChromeOS.FastPair.PairFailure.RetroactivePairingProtocol";
const char kFastPairPairResultInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.Pairing.Result.InitialPairingProtocol";
const char kFastPairPairResultSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.Pairing.Result.SubsequentPairingProtocol";
const char kFastPairPairResultRetroactiveMetric[] =
    "Bluetooth.ChromeOS.FastPair.Pairing.Result.RetroactivePairingProtocol";
const char kFastPairAccountKeyWriteResultInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.Result."
    "InitialPairingProtocol";
const char kFastPairAccountKeyWriteResultSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.Result."
    "SubsequentPairingProtocol";
const char kFastPairAccountKeyWriteResultRetroactiveMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.Result."
    "RetroactivePairingProtocol";
const char kFastPairAccountKeyWriteFailureInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Failure.InitialPairingProtocol";
const char kFastPairAccountKeyWriteFailureRetroactiveMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Failure.RetroactivePairingProtocol";
const char kKeyGenerationResultMetric[] =
    "Bluetooth.ChromeOS.FastPair.KeyGenerationResult";
const char kDataEncryptorCreateResultMetric[] =
    "Bluetooth.ChromeOS.FastPair.FastPairDataEncryptor.CreateResult";
const char kWriteKeyBasedCharacteristicResult[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.Write.Result";
const char kWriteKeyBasedCharacteristicPairFailure[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.Write.PairFailure";
const char kWriteKeyBasedCharacteristicGattError[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.Write.GattErrorReason";
const char kNotifyKeyBasedCharacteristicTime[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.NotifyTime";
const char kKeyBasedCharacteristicDecryptTime[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.DecryptTime";
const char kKeyBasedCharacteristicDecryptResult[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.DecryptResult";
const char kWritePasskeyCharacteristicResult[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Write.Result";
const char kWritePasskeyCharacteristicPairFailure[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Write.PairFailure";
const char kWritePasskeyCharacteristicGattError[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Write.GattErrorReason";
const char kNotifyPasskeyCharacteristicTime[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.NotifyTime";
const char kPasskeyCharacteristicDecryptTime[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Decrypt.Time";
const char kPasskeyCharacteristicDecryptResult[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Decrypt.Result";
const char kWriteAccountKeyCharacteristicResult[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.Result";
const char kWriteAccountKeyCharacteristicGattError[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.GattErrorReason";
const char kWriteAccountKeyTime[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.TotalTime";
const char kTotalDataEncryptorCreateTime[] =
    "Bluetooth.ChromeOS.FastPair.FastPairDataEncryptor.CreateTime";
const char kMessageStreamReceiveResult[] =
    "Bluetooth.ChromeOS.FastPair.MessageStream.Receive.Result";
const char kMessageStreamReceiveError[] =
    "Bluetooth.ChromeOS.FastPair.MessageStream.Receive.ErrorReason";
const char kMessageStreamConnectToServiceError[] =
    "Bluetooth.ChromeOS.FastPair.MessageStream.ConnectToService.ErrorReason";
const char kMessageStreamConnectToServiceResult[] =
    "Bluetooth.ChromeOS.FastPair.MessageStream.ConnectToService.Result";
const char kMessageStreamConnectToServiceTime[] =
    "Bluetooth.ChromeOS.FastPair.MessageStream.ConnectToService."
    "TotalConnectTime";
const char kDeviceMetadataFetchResult[] =
    "Bluetooth.ChromeOS.FastPair.DeviceMetadataFetcher.Result";
const char kDeviceMetadataFetchNetError[] =
    "Bluetooth.ChromeOS.FastPair.DeviceMetadataFetcher.Get.NetError";
const char kDeviceMetadataFetchHttpResponseError[] =
    "Bluetooth.ChromeOS.FastPair.DeviceMetadataFetcher.Get.HttpResponseError";
const char kFootprintsFetcherDeleteResult[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Delete.Result";
const char kFootprintsFetcherDeleteNetError[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Delete.NetError";
const char kFootprintsFetcherDeleteHttpResponseError[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Delete.HttpResponseError";
const char kFootprintsFetcherPostResult[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Post.Result";
const char kFootprintsFetcherPostNetError[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Post.NetError";
const char kFootprintsFetcherPostHttpResponseError[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Post.HttpResponseError";
const char kFootprintsFetcherGetResult[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Get.Result";
const char kFootprintsFetcherGetNetError[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Get.NetError";
const char kFootprintsFetcherGetHttpResponseError[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Get.HttpResponseError";
const char kFastPairRepositoryCacheResult[] =
    "Bluetooth.ChromeOS.FastPair.FastPairRepository.Cache.Result";
const char kHandshakeResult[] = "Bluetooth.ChromeOS.FastPair.Handshake.Result";
const char kHandshakeFailureReason[] =
    "Bluetooth.ChromeOS.FastPair.Handshake.FailureReason";
const char kBleScanSessionResult[] =
    "Bluetooth.ChromeOS.FastPair.Scanner.StartSession.Result";
const char kBleScanFilterResult[] =
    "Bluetooth.ChromeOS.FastPair.CreateScanFilter.Result";
const char kFastPairVersion[] =
    "Bluetooth.ChromeOS.FastPair.Discovered.Version";
const char kNavigateToSettings[] =
    "Bluetooth.ChromeOS.FastPair.NavigateToSettings.Result";
const char kConnectDeviceResult[] =
    "Bluetooth.ChromeOS.FastPair.ConnectDevice.Result";
const char kPairDeviceResult[] =
    "Bluetooth.ChromeOS.FastPair.PairDevice.Result";
const char kPairDeviceErrorReason[] =
    "Bluetooth.ChromeOS.FastPair.PairDevice.ErrorReason";
const char kConfirmPasskeyAskTime[] =
    "Bluetooth.ChromeOS.FastPair.RequestPasskey.Latency";
const char kConfirmPasskeyConfirmTime[] =
    "Bluetooth.ChromeOS.FastPair.ConfirmPasskey.Latency";
const char kFastPairRetryCount[] =
    "Bluetooth.ChromeOS.FastPair.PairRetry.Count";

}  // namespace

namespace ash {
namespace quick_pair {

void AttemptRecordingFastPairEngagementFlow(const Device& device,
                                            FastPairEngagementFlowEvent event) {
  switch (device.protocol) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramSparse(kEngagementFlowInitialMetric,
                               static_cast<int>(event));
      break;
    case Protocol::kFastPairRetroactive:
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramSparse(kEngagementFlowSubsequentMetric,
                               static_cast<int>(event));
      break;
  }
}

void AttemptRecordingTotalUxPairTime(const Device& device,
                                     base::TimeDelta total_pair_time) {
  switch (device.protocol) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramTimes(kTotalUxPairTimeInitialMetric, total_pair_time);
      break;
    case Protocol::kFastPairRetroactive:
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramTimes(kTotalUxPairTimeSubsequentMetric,
                              total_pair_time);
      break;
  }
}

void AttemptRecordingFastPairRetroactiveEngagementFlow(
    const Device& device,
    FastPairRetroactiveEngagementFlowEvent event) {
  switch (device.protocol) {
    case Protocol::kFastPairInitial:
    case Protocol::kFastPairSubsequent:
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramSparse(kRetroactiveEngagementFlowMetric,
                               static_cast<int>(event));
      break;
  }
}

void RecordPairingMethod(PairingMethod method) {
  base::UmaHistogramEnumeration(kPairingMethodMetric, method);
}

void RecordRetroactivePairingResult(bool success) {
  base::UmaHistogramBoolean(kRetroactivePairingResultMetric, success);
}

void RecordTotalGattConnectionTime(base::TimeDelta total_gatt_connection_time) {
  base::UmaHistogramTimes(kTotalGattConnectionTimeMetric,
                          total_gatt_connection_time);
}

void RecordGattConnectionResult(bool success) {
  base::UmaHistogramBoolean(kGattConnectionResult, success);
}

void RecordGattConnectionErrorCode(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  base::UmaHistogramEnumeration(
      kGattConnectionErrorMetric, error_code,
      device::BluetoothDevice::ConnectErrorCode::NUM_CONNECT_ERROR_CODES);
}

void RecordPairingResult(const Device& device, bool success) {
  switch (device.protocol) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramBoolean(kFastPairPairResultInitialMetric, success);
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramBoolean(kFastPairPairResultRetroactiveMetric, success);
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramBoolean(kFastPairPairResultSubsequentMetric, success);
      break;
  }
}

void RecordPairingFailureReason(const Device& device, PairFailure failure) {
  switch (device.protocol) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramEnumeration(kFastPairPairFailureInitialMetric, failure);
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramEnumeration(kFastPairPairFailureRetroactiveMetric,
                                    failure);
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramEnumeration(kFastPairPairFailureSubsequentMetric,
                                    failure);
      break;
  }
}

void RecordAccountKeyFailureReason(const Device& device,
                                   AccountKeyFailure failure) {
  switch (device.protocol) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramEnumeration(
          kFastPairAccountKeyWriteFailureInitialMetric, failure);
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramEnumeration(
          kFastPairAccountKeyWriteFailureRetroactiveMetric, failure);
      break;
    case Protocol::kFastPairSubsequent:
      break;
  }
}

void RecordAccountKeyResult(const Device& device, bool success) {
  switch (device.protocol) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramBoolean(kFastPairAccountKeyWriteResultInitialMetric,
                                success);
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramBoolean(kFastPairAccountKeyWriteResultRetroactiveMetric,
                                success);
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramBoolean(kFastPairAccountKeyWriteResultSubsequentMetric,
                                success);
      break;
  }
}

void RecordKeyPairGenerationResult(bool success) {
  base::UmaHistogramBoolean(kKeyGenerationResultMetric, success);
}

void RecordDataEncryptorCreateResult(bool success) {
  base::UmaHistogramBoolean(kDataEncryptorCreateResultMetric, success);
}

void RecordWriteKeyBasedCharacteristicResult(bool success) {
  base::UmaHistogramBoolean(kWriteKeyBasedCharacteristicResult, success);
}

void RecordWriteKeyBasedCharacteristicPairFailure(PairFailure failure) {
  base::UmaHistogramEnumeration(kWriteKeyBasedCharacteristicPairFailure,
                                failure);
}

void RecordWriteRequestGattError(
    device::BluetoothGattService::GattErrorCode error) {
  base::UmaHistogramEnumeration(kWriteKeyBasedCharacteristicGattError, error);
}

void RecordNotifyKeyBasedCharacteristicTime(base::TimeDelta total_notify_time) {
  base::UmaHistogramTimes(kNotifyKeyBasedCharacteristicTime, total_notify_time);
}

void RecordKeyBasedCharacteristicDecryptTime(base::TimeDelta decrypt_time) {
  base::UmaHistogramTimes(kKeyBasedCharacteristicDecryptTime, decrypt_time);
}

void RecordKeyBasedCharacteristicDecryptResult(bool success) {
  base::UmaHistogramBoolean(kKeyBasedCharacteristicDecryptResult, success);
}

void RecordWritePasskeyCharacteristicResult(bool success) {
  base::UmaHistogramBoolean(kWritePasskeyCharacteristicResult, success);
}

void RecordWritePasskeyCharacteristicPairFailure(PairFailure failure) {
  base::UmaHistogramEnumeration(kWritePasskeyCharacteristicPairFailure,
                                failure);
}

void RecordWritePasskeyGattError(
    device::BluetoothGattService::GattErrorCode error) {
  base::UmaHistogramEnumeration(kWritePasskeyCharacteristicGattError, error);
}

void RecordNotifyPasskeyCharacteristicTime(base::TimeDelta total_notify_time) {
  base::UmaHistogramTimes(kNotifyPasskeyCharacteristicTime, total_notify_time);
}

void RecordPasskeyCharacteristicDecryptTime(base::TimeDelta decrypt_time) {
  base::UmaHistogramTimes(kPasskeyCharacteristicDecryptTime, decrypt_time);
}

void RecordPasskeyCharacteristicDecryptResult(bool success) {
  base::UmaHistogramBoolean(kPasskeyCharacteristicDecryptResult, success);
}

void RecordWriteAccountKeyCharacteristicResult(bool success) {
  base::UmaHistogramBoolean(kWriteAccountKeyCharacteristicResult, success);
}

void RecordWriteAccountKeyGattError(
    device::BluetoothGattService::GattErrorCode error) {
  base::UmaHistogramEnumeration(kWriteAccountKeyCharacteristicGattError, error);
}

void RecordWriteAccountKeyTime(base::TimeDelta write_time) {
  base::UmaHistogramTimes(kWriteAccountKeyTime, write_time);
}

void RecordTotalDataEncryptorCreateTime(base::TimeDelta total_create_time) {
  base::UmaHistogramTimes(kTotalDataEncryptorCreateTime, total_create_time);
}

void RecordMessageStreamReceiveResult(bool success) {
  base::UmaHistogramBoolean(kMessageStreamReceiveResult, success);
}

void RecordMessageStreamReceiveError(
    device::BluetoothSocket::ErrorReason error) {
  base::UmaHistogramEnumeration(kMessageStreamReceiveError, error);
}

void RecordMessageStreamConnectToServiceResult(bool success) {
  base::UmaHistogramBoolean(kMessageStreamConnectToServiceResult, success);
}

void RecordMessageStreamConnectToServiceError(const std::string& error) {
  base::UmaHistogramEnumeration(kMessageStreamConnectToServiceError,
                                GetConnectToServiceError(error));
}

void RecordMessageStreamConnectToServiceTime(
    base::TimeDelta total_connect_time) {
  base::UmaHistogramTimes(kMessageStreamConnectToServiceTime,
                          total_connect_time);
}

void RecordDeviceMetadataFetchResult(const FastPairHttpResult& result) {
  base::UmaHistogramBoolean(kDeviceMetadataFetchResult, result.IsSuccess());

  if (result.net_error()) {
    base::UmaHistogramSparse(kDeviceMetadataFetchNetError,
                             -*result.net_error());
  }

  if (result.http_response_error()) {
    base::UmaHistogramSparse(kDeviceMetadataFetchHttpResponseError,
                             *result.http_response_error());
  }
}

void RecordFootprintsFetcherDeleteResult(const FastPairHttpResult& result) {
  base::UmaHistogramBoolean(kFootprintsFetcherDeleteResult, result.IsSuccess());

  if (result.net_error()) {
    base::UmaHistogramSparse(kFootprintsFetcherDeleteNetError,
                             -*result.net_error());
  }

  if (result.http_response_error()) {
    base::UmaHistogramSparse(kFootprintsFetcherDeleteHttpResponseError,
                             *result.http_response_error());
  }
}

void RecordFootprintsFetcherPostResult(const FastPairHttpResult& result) {
  base::UmaHistogramBoolean(kFootprintsFetcherPostResult, result.IsSuccess());

  if (result.net_error()) {
    base::UmaHistogramSparse(kFootprintsFetcherPostNetError,
                             -*result.net_error());
  }

  if (result.http_response_error()) {
    base::UmaHistogramSparse(kFootprintsFetcherPostHttpResponseError,
                             *result.http_response_error());
  }
}

void RecordFootprintsFetcherGetResult(const FastPairHttpResult& result) {
  base::UmaHistogramBoolean(kFootprintsFetcherGetResult, result.IsSuccess());

  if (result.net_error()) {
    base::UmaHistogramSparse(kFootprintsFetcherGetNetError,
                             -*result.net_error());
  }

  if (result.http_response_error()) {
    base::UmaHistogramSparse(kFootprintsFetcherGetHttpResponseError,
                             *result.http_response_error());
  }
}

void RecordFastPairRepositoryCacheResult(bool success) {
  base::UmaHistogramBoolean(kFastPairRepositoryCacheResult, success);
}

void RecordHandshakeResult(bool success) {
  base::UmaHistogramBoolean(kHandshakeResult, success);
}
void RecordHandshakeFailureReason(HandshakeFailureReason failure_reason) {
  base::UmaHistogramEnumeration(kHandshakeFailureReason, failure_reason);
}

void RecordBluetoothLowEnergyScannerStartSessionResult(bool success) {
  base::UmaHistogramBoolean(kBleScanSessionResult, success);
}

void RecordBluetoothLowEnergyScanFilterResult(bool success) {
  base::UmaHistogramBoolean(kBleScanFilterResult, success);
}

void RecordFastPairDiscoveredVersion(FastPairVersion version) {
  base::UmaHistogramEnumeration(kFastPairVersion, version);
}

void RecordNavigateToSettingsResult(bool success) {
  base::UmaHistogramBoolean(kNavigateToSettings, success);
}

void RecordConnectDeviceResult(bool success) {
  base::UmaHistogramBoolean(kConnectDeviceResult, success);
}

void RecordPairDeviceResult(bool success) {
  base::UmaHistogramBoolean(kPairDeviceResult, success);
}

void RecordPairDeviceErrorReason(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  base::UmaHistogramEnumeration(
      kPairDeviceErrorReason, error_code,
      device::BluetoothDevice::NUM_CONNECT_ERROR_CODES);
}

void RecordConfirmPasskeyConfirmTime(base::TimeDelta total_confirm_time) {
  base::UmaHistogramTimes(kConfirmPasskeyConfirmTime, total_confirm_time);
}

void RecordConfirmPasskeyAskTime(base::TimeDelta total_ask_time) {
  base::UmaHistogramTimes(kConfirmPasskeyAskTime, total_ask_time);
}

void RecordPairFailureRetry(int num_retries) {
  base::UmaHistogramExactLinear(kFastPairRetryCount, num_retries,
                                /*exclusive_max=*/10);
}

}  // namespace quick_pair
}  // namespace ash
