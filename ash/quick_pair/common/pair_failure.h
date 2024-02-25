// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_PAIR_FAILURE_H_
#define ASH_QUICK_PAIR_COMMON_PAIR_FAILURE_H_

#include <ostream>
#include "base/component_export.h"

namespace ash {
namespace quick_pair {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This enum should be kept in sync with
// the FastPairPairFailure enum in //tools/metrics/histograms/enums.xml.
enum class PairFailure {
  // Failed to create a GATT connection to the device.
  kCreateGattConnection = 0,
  // Failed to find the expected GATT service.
  kGattServiceDiscovery = 1,
  // Timed out while starting discovery of GATT service.
  kGattServiceDiscoveryTimeout = 2,
  // Failed to retrieve the data encryptor.
  kDataEncryptorRetrieval = 3,
  // Failed to find the Key-based pairing GATT characteristic.
  kKeyBasedPairingCharacteristicDiscovery = 4,
  // Failed to find the Passkey GATT characteristic.
  kPasskeyCharacteristicDiscovery = 5,
  // Failed to find the Account Key GATT characteristic.
  kAccountKeyCharacteristicDiscovery = 6,
  // Failed to start a notify session on the Key-based pairing GATT
  // characteristic.
  kKeyBasedPairingCharacteristicNotifySession = 7,
  // Failed to start a notify session on the Passkey GATT characteristic.
  kPasskeyCharacteristicNotifySession = 8,
  // Timed out while waiting to start a notify session on the Key-based pairing
  // GATT characteristic.
  kKeyBasedPairingCharacteristicNotifySessionTimeout = 9,
  // / Timed out while waiting to start a notify session on the Passkey GATT
  // characteristic.
  kPasskeyCharacteristicNotifySessionTimeout = 10,
  // Failed to write to the Key-based pairing GATT characteristic.
  kKeyBasedPairingCharacteristicWrite = 11,
  // Failed to write to the Passkey GATT characteristic.
  kPasskeyPairingCharacteristicWrite = 12,
  // Timed out while waiting for the Key-based Pairing response.
  kKeyBasedPairingResponseTimeout = 13,
  // Timed out while waiting for the Passkey response.
  kPasskeyResponseTimeout = 14,
  // Failed to decrypt Key-based response message.
  kKeybasedPairingResponseDecryptFailure = 15,
  // Incorrect Key-based response message type.
  kIncorrectKeyBasedPairingResponseType = 16,
  // Failed to decrypt Passkey response message.
  kPasskeyDecryptFailure = 17,
  // Incorrect Passkey response message type.
  kIncorrectPasskeyResponseType = 18,
  // Passkeys did not match.
  kPasskeyMismatch = 19,
  // Potential pairing device lost during Passkey exchange.
  kPairingDeviceLost = 20,
  // Failed to bond to discovered device.
  kPairingConnect = 21,
  // Failed to bond to device via public address.
  kAddressConnect = 22,
  // BLE device instance lost mid pair with classic instance
  kBleDeviceLostMidPair = 23,
  // Timed out attempting to bond to device.
  kCreateBondTimeout = 24,
  // Potential pairing device lost between GATT connection attempts.
  kPairingDeviceLostBetweenGattConnectionAttempts = 25,
  // Timed out while waiting for the Bluetooth adapter event to confirm the
  // passkey after pairing begins.
  kConfirmPasskeyTimeout = 26,
  // Failed to disconnect GATT connections from device between retries.
  kFailureToDisconnectGattBetweenRetries = 27,
  // Failed to create a GATT connection in the platform layer due to a Bluetooth
  // device error.
  kBluetoothDeviceFailureCreatingGattConnection = 28,
  // Timed out while waiting for a response after attempt to disconnect.
  kDisconnectResponseTimeout = 29,
  // Failed to connect to discovered device after pairing when the device is
  // known to the adapter.
  kFailedToConnectAfterPairing = 30,
  // Failed to write to Additional Data characteristic.
  kAdditionalDataCharacteristicWrite = 31,
  // Failed to discover Additional Data Characteristic.
  // Currently this failure is not emitted anywhere.
  // TODO(b/279654454) measure this pair failure in some metric.
  kAdditionalDataCharacteristicDiscovery = 32,
  // Timed out while writing to Additional Data characteristic.
  kAdditionalDataCharacteristicWriteTimeout = 33,
  kMaxValue = kAdditionalDataCharacteristicWriteTimeout,
};

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
std::ostream& operator<<(std::ostream& stream, PairFailure protocol);

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_PAIR_FAILURE_H_
