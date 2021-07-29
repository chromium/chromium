// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_PAIR_FAILURE_H_
#define ASH_QUICK_PAIR_COMMON_PAIR_FAILURE_H_

#include <ostream>
#include "base/component_export.h"

namespace ash {
namespace quick_pair {

enum class PairFailure {
  // Failed to create a GATT connection to the device.
  kCreateGattConnection = 0,
  // Failed to find the expected GATT service.
  kGattServiceDiscovery = 1,
  // Timed out while starting discovery of GATT service.
  kGattServiceDiscoveryTimeout = 2,
  // Failed to find the Key-based pairing GATT characteristic.
  kKeyBasedPairingCharacteristicDiscovery = 3,
  // Failed to find the Passkey GATT characteristic.
  kPasskeyCharacteristicDiscovery = 4,
  // Failed to find the Account Key GATT characteristic.
  kAccountKeyCharacteristicDiscovery = 5,
  // Failed to start a notify session on the Key-based pairing GATT
  // characteristic.
  kKeyBasedPairingCharacteristicNotifySession = 6,
  // Failed to start a notify session on the Passkey GATT characteristic.
  kPasskeyCharacteristicNotifySession = 7,
  // Timed out while waiting to start a notify session on the Key-based pairing
  // GATT characteristic.
  kKeyBasedPairingCharacteristicNotifySessionTimeout = 8,
  // / Timed out while waiting to start a notify session on the Passkey GATT
  // characteristic.
  kPasskeyCharacteristicNotifySessionTimeout = 9,
  // Failed to write to the Key-based pairing GATT characteristic.
  kKeyBasedPairingCharacteristicWrite = 10,
  // Failed to write to the Passkey GATT characteristic.
  kPasskeyPairingCharacteristicWrite = 11,
  // Timed out while waiting for the Key-based Pairing response.
  kKeyBasedPairingResponseTimeout = 12,
  // Timed out while waiting for the Passkey response.
  kPasskeyResponseTimeout = 13,
  // Incorrect Key-based response message type.
  kIncorrectKeyBasedPairingResponseType = 14,
  // Incorrect Passkey response message type.
  kIncorrectPasskeyResponseType = 15,
  // Passkeys did not match.
  kPasskeyMismatch = 16,
  // Failed to bond to discovered device.
  kPairingConnect = 17,
  // Failed to bond to device via public address.
  kAddressConnect = 18,
};

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
std::ostream& operator<<(std::ostream& stream, PairFailure protocol);

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_PAIR_FAILURE_H_
