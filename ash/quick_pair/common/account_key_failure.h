// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_ACCOUNT_KEY_FAILURE_H_
#define ASH_QUICK_PAIR_COMMON_ACCOUNT_KEY_FAILURE_H_

#include <ostream>
#include "base/component_export.h"

namespace ash {
namespace quick_pair {

enum class AccountKeyFailure {
  // Failed to find the Account Key GATT characteristic.
  kAccountKeyCharacteristicDiscovery = 0,
  // Failed to write to the Account Key GATT characteristic.
  kAccountKeyCharacteristicWrite = 1,
};

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
std::ostream& operator<<(std::ostream& stream, AccountKeyFailure protocol);

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_ACCOUNT_KEY_FAILURE_H_
