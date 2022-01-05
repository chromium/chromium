// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"

#include "ash/quick_pair/common/device.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"

namespace {

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
    "Bluetooth.ChromeOS.FastPair.AccountKeyWrite.Result.InitialPairingProtocol";
const char kFastPairAccountKeyWriteResultSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKeyWrite.Result."
    "SubsequentPairingProtocol";
const char kFastPairAccountKeyWriteResultRetroactiveMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKeyWrite.Result."
    "RetroactivePairingProtocol";
const char kFastPairAccountKeyWriteFailureInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKeyFailure.InitialPairingProtocol";
const char kFastPairAccountKeyWriteFailureSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKeyFailure.SubsequentPairingProtocol";
const char kFastPairAccountKeyWriteFailureRetroactiveMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKeyFailure.RetroactivePairingProtocol";
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
      base::UmaHistogramEnumeration(
          kFastPairAccountKeyWriteFailureSubsequentMetric, failure);
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

}  // namespace quick_pair
}  // namespace ash
