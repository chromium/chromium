// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/smartlock_state.h"

namespace ash {

std::ostream& operator<<(std::ostream& stream, const SmartLockState& state) {
  switch (state) {
    case SmartLockState::kInactive:
      stream << "[inactive]";
      break;
    case SmartLockState::kDisabled:
      stream << "[disabled]";
      break;
    case SmartLockState::kBluetoothDisabled:
      stream << "[bluetooth disabled]";
      break;
    case SmartLockState::kConnectingToPhone:
      stream << "[connecting to phone]";
      break;
    case SmartLockState::kPhoneNotFound:
      stream << "[phone not found]";
      break;
    case SmartLockState::kPhoneNotAuthenticated:
      stream << "[phone not authenticated]";
      break;
    case SmartLockState::kPhoneFoundLockedAndProximate:
      stream << "[phone locked and proximate]";
      break;
    case SmartLockState::kPhoneNotLockable:
      stream << "[phone not lockable]";
      break;
    case SmartLockState::kPhoneFoundUnlockedAndDistant:
      stream << "[phone unlocked and distant]";
      break;
    case SmartLockState::kPhoneFoundLockedAndDistant:
      stream << "[phone locked and distant]";
      break;
    case SmartLockState::kPhoneAuthenticated:
      stream << "[phone authenticated]";
      break;
    case SmartLockState::kPrimaryUserAbsent:
      stream << "[primary user absent]";
      break;
  }
  return stream;
}

}  // namespace ash
