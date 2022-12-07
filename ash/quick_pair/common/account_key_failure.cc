// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/account_key_failure.h"

namespace ash {
namespace quick_pair {

std::ostream& operator<<(std::ostream& stream, AccountKeyFailure failure) {
  switch (failure) {
    case AccountKeyFailure::kAccountKeyCharacteristicDiscovery:
      stream << "[Failed to find the Account Key GATT characteristic]";
      break;
    case AccountKeyFailure::kDeprecated_AccountKeyCharacteristicWrite:
      stream << "[Failed to write to the Account Key GATT characteristic]";
      break;
    case AccountKeyFailure::kAccountKeyCharacteristicWriteTimeout:
      stream << "[Timed out attempting to write to the Account Key GATT "
                "characteristic]";
      break;
    case AccountKeyFailure::kGattErrorUnknown:
      stream << "[GATT_ERROR_UNKNOWN]";
      break;
    case AccountKeyFailure::kGattErrorFailed:
      stream << "[GATT_ERROR_FAILED]";
      break;
    case AccountKeyFailure::kGattInProgress:
      stream << "[GATT_ERROR_IN_PROGRESS]";
      break;
    case AccountKeyFailure::kGattErrorInvalidLength:
      stream << "[GATT_ERROR_INVALID_LENGTH]";
      break;
    case AccountKeyFailure::kGattErrorNotPermitted:
      stream << "[GATT_ERROR_NOT_PERMITTED]";
      break;
    case AccountKeyFailure::kGattErrorNotAuthorized:
      stream << "[GATT_ERROR_NOT_AUTHORIZED]";
      break;
    case AccountKeyFailure::kGattErrorNotPaired:
      stream << "[GATT_ERROR_NOT_PAIRED]";
      break;
    case AccountKeyFailure::kGattErrorNotSupported:
      stream << "[GATT_ERROR_NOT_SUPPORTED]";
      break;
  }

  return stream;
}

}  // namespace quick_pair
}  // namespace ash
