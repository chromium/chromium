// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_METRICS_H_
#define ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_METRICS_H_

#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_http_result.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "base/component_export.h"
#include "base/time/time.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"
#include "device/bluetooth/bluetooth_socket.h"

namespace ash {
namespace quick_pair {

struct Device;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. The numbers here correspond to the
// ordering of the flow. This enum should be kept in sync with the
// FastPairEngagementFlowEvent enum in src/tools/metrics/histograms/enums.xml.
enum COMPONENT_EXPORT(QUICK_PAIR_COMMON) FastPairEngagementFlowEvent {
  kDiscoveryUiShown = 1,
  kDiscoveryUiDismissed = 11,
  kDiscoveryUiConnectPressed = 12,
  kDiscoveryUiDismissedByUser = 13,
  kDiscoveryUiLearnMorePressed = 14,
  kPairingFailed = 121,
  kPairingSucceeded = 122,
  kDiscoveryUiConnectPressedAfterLearnMorePressed = 141,
  kDiscoveryUiDismissedByUserAfterLearnMorePressed = 142,
  kDiscoveryUiDismissedAfterLearnMorePressed = 143,
  kErrorUiDismissed = 1211,
  kErrorUiSettingsPressed = 1212,
  kErrorUiDismissedByUser = 1213,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. The numbers here correspond to the
// ordering of the flow. This enum should be kept in sync with the
// FastPairRetroactiveEngagementFlowEvent enum in
// src/tools/metrics/histograms/enums.xml.
enum COMPONENT_EXPORT(QUICK_PAIR_COMMON)
    FastPairRetroactiveEngagementFlowEvent {
      kAssociateAccountUiShown = 1,
      kAssociateAccountUiDismissedByUser = 11,
      kAssociateAccountUiDismissed = 12,
      kAssociateAccountLearnMorePressed = 13,
      kAssociateAccountSavePressed = 14,
      kAssociateAccountSavePressedAfterLearnMorePressed = 131,
      kAssociateAccountDismissedByUserAfterLearnMorePressed = 132,
      kAssociateAccountDismissedAfterLearnMorePressed = 133,
    };

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This enum should be kept in sync
// with the FastPairPairingMethod enum in
// src/tools/metrics/histograms/enums.xml.
enum class COMPONENT_EXPORT(QUICK_PAIR_COMMON) PairingMethod {
  kFastPair = 0,
  kSystemPairingUi = 1,
  kMaxValue = kSystemPairingUi,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This enum should be kept in sync
// with the FastPairHandshakeFailureReason enum in
// src/tools/metrics/histograms/enums.xml.
enum class COMPONENT_EXPORT(QUICK_PAIR_COMMON) HandshakeFailureReason {
  kFailedGattInit = 0,
  kFailedCreateEncryptor = 1,
  kFailedWriteResponse = 2,
  kFailedDecryptResponse = 3,
  kFailedIncorrectResponseType = 4,
  kMaxValue = kFailedIncorrectResponseType,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This enum should be kept in sync
// with the FastPairVersion enum in src/tools/metrics/histograms/enums.xml.
enum class COMPONENT_EXPORT(QUICK_PAIR_COMMON) FastPairVersion {
  kVersion1 = 0,
  kVersion2 = 1,
  kMaxValue = kVersion2,
};

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void AttemptRecordingFastPairEngagementFlow(const Device& device,
                                            FastPairEngagementFlowEvent event);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void AttemptRecordingTotalUxPairTime(const Device& device,
                                     base::TimeDelta total_pair_time);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void AttemptRecordingFastPairRetroactiveEngagementFlow(
    const Device& device,
    FastPairRetroactiveEngagementFlowEvent event);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordPairingMethod(PairingMethod method);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordRetroactivePairingResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordTotalGattConnectionTime(base::TimeDelta total_gatt_connection_time);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordGattConnectionResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordGattConnectionErrorCode(
    device::BluetoothDevice::ConnectErrorCode error_code);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordPairingFailureReason(const Device& device, PairFailure failure);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordPairingResult(const Device& device, bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordAccountKeyFailureReason(const Device& device,
                                   AccountKeyFailure failure);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordAccountKeyResult(const Device& device, bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordKeyPairGenerationResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordDataEncryptorCreateResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordWriteKeyBasedCharacteristicResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordWriteKeyBasedCharacteristicPairFailure(PairFailure failure);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordWriteRequestGattError(
    device::BluetoothGattService::GattErrorCode error);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordNotifyKeyBasedCharacteristicTime(base::TimeDelta total_notify_time);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordKeyBasedCharacteristicDecryptTime(base::TimeDelta decrypt_time);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordKeyBasedCharacteristicDecryptResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordWritePasskeyCharacteristicResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordWritePasskeyCharacteristicPairFailure(PairFailure failure);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordWritePasskeyGattError(
    device::BluetoothGattService::GattErrorCode error);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordNotifyPasskeyCharacteristicTime(base::TimeDelta total_notify_time);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordPasskeyCharacteristicDecryptTime(base::TimeDelta decrypt_time);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordPasskeyCharacteristicDecryptResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordWriteAccountKeyCharacteristicResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordWriteAccountKeyGattError(
    device::BluetoothGattService::GattErrorCode error);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordWriteAccountKeyTime(base::TimeDelta write_time);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordTotalDataEncryptorCreateTime(base::TimeDelta total_create_time);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordMessageStreamReceiveResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordMessageStreamReceiveError(
    device::BluetoothSocket::ErrorReason error);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordMessageStreamConnectToServiceResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordMessageStreamConnectToServiceError(const std::string& error);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordMessageStreamConnectToServiceTime(
    base::TimeDelta total_connect_time);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordDeviceMetadataFetchResult(const FastPairHttpResult& result);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordFootprintsFetcherDeleteResult(const FastPairHttpResult& result);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordFootprintsFetcherPostResult(const FastPairHttpResult& result);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordFootprintsFetcherGetResult(const FastPairHttpResult& result);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordFastPairRepositoryCacheResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordHandshakeResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordHandshakeFailureReason(HandshakeFailureReason failure_reason);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordBluetoothLowEnergyScannerStartSessionResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordBluetoothLowEnergyScannerStartSessionErrorReason(
    device::BluetoothLowEnergyScanSession::ErrorCode error_code);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordBluetoothLowEnergyScanFilterResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordFastPairDiscoveredVersion(FastPairVersion version);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordNavigateToSettingsResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordConnectDeviceResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordPairDeviceResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordPairDeviceErrorReason(
    device::BluetoothDevice::ConnectErrorCode error_code);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordConfirmPasskeyConfirmTime(base::TimeDelta total_confirm_time);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordConfirmPasskeyAskTime(base::TimeDelta total_ask_time);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordPairFailureRetry(int num_retries);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordSavedDevicesRemoveResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordSavedDevicesUpdatedOptInStatusResult(const Device& device,
                                                bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordGetSavedDevicesResult(bool success);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordSavedDevicesTotalUxLoadTime(base::TimeDelta total_load_time);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
void RecordSavedDevicesCount(int num_devices);

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_METRICS_H_
