// Copyright 2021 The Chromium Authors. All rights reserved.
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

    case AccountKeyFailure::kAccountKeyCharacteristicWrite:
      stream << "[Failed to write to the Account Key GATT characteristic]";
      break;
  }

  return stream;
}

}  // namespace quick_pair
}  // namespace ash
