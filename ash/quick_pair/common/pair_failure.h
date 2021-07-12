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
  // Failed to find the Key-based pairing GATT characteristic.
  kKeyBasedPairingCharacteristicDiscovery = 2,
  // Failed to find the Passkey GATT characteristic.
  kPasskeyCharacteristicDiscovery = 3,
  // Failed to start a notify session on the Key-based pairing GATT
  // characteristic.
  kKeyBasedPairingCharacteristicNotifySession = 4,
  // Failed to start a notify session on the Passkey GATT characteristic.
  kPasskeyCharacteristicNotifySession = 5,
  // Failed to write to the Key-based pairing GATT characteristic.
  kKeyBasedPairingCharacteristicWrite = 6,
  // Failed to write to the Passkey GATT characteristic.
  kPasskeyPairingCharacteristicWrite = 7,
  // Timed out while waiting for the Key-based Pairing response.
  kKeyBasedPairingResponseTimeout = 8,
  // Timed out while waiting for the Passkey response.
  kPasskeyResponseTimeout = 9,
  // Incorrect Key-based response message type.
  kIncorrectKeyBasedPairingResponseType = 10,
  // Incorrect Passkey response message type.
  kIncorrectPasskeyResponseType = 11,
  // Passkeys did not match.
  kPasskeyMismatch = 12,
  // Failed to bond to discovered device.
  kPairingConnect = 13,
  // Failed to bond to device via public address.
  kAddressConnect = 14,
};

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
std::ostream& operator<<(std::ostream& stream, PairFailure protocol);

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_PAIR_FAILURE_H_
