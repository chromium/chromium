// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_ACCOUNT_KEY_FAILURE_H_
#define ASH_QUICK_PAIR_COMMON_ACCOUNT_KEY_FAILURE_H_

#include <ostream>
#include "base/component_export.h"

namespace ash {
namespace quick_pair {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This enum should be kept in sync with
// the FastPairAccountKeyFailure enum in
// //tools/metrics/histograms/metadata/bluetooth/enums.xml.
enum class AccountKeyFailure {
  // Failed to find the Account Key GATT characteristic.
  kAccountKeyCharacteristicDiscovery = 0,
  // Deprecated
  kDeprecated_AccountKeyCharacteristicWrite = 1,
  // Timed out while writing to the Account Key GATT characteristic.
  kAccountKeyCharacteristicWriteTimeout = 2,
  // The remaining error codes correspond to the GATT errors in
  // device/bluetooth/bluetooth_gatt_service.h
  kGattErrorUnknown = 3,
  kGattErrorFailed = 4,
  kGattInProgress = 5,
  kGattErrorInvalidLength = 6,
  kGattErrorNotPermitted = 7,
  kGattErrorNotAuthorized = 8,
  kGattErrorNotPaired = 9,
  kGattErrorNotSupported = 10,
  kMaxValue = kGattErrorNotSupported,
};

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
std::ostream& operator<<(std::ostream& stream, AccountKeyFailure protocol);

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_ACCOUNT_KEY_FAILURE_H_
