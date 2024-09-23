// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOBSTER_LOBSTER_SYSTEM_STATE_H_
#define ASH_PUBLIC_CPP_LOBSTER_LOBSTER_SYSTEM_STATE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "base/containers/enum_set.h"

namespace ash {

struct ASH_PUBLIC_EXPORT LobsterSystemState {
  using SystemChecks = base::EnumSet<LobsterSystemCheck,
                                     LobsterSystemCheck::kMinValue,
                                     LobsterSystemCheck::kMaxValue>;

  LobsterSystemState(LobsterStatus status, SystemChecks failed_checks);
  ~LobsterSystemState();

  // Specifies the current system status.
  LobsterStatus status;
  // Holds any system checks that failed when deriving the above system status.
  SystemChecks failed_checks;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOBSTER_LOBSTER_SYSTEM_STATE_H_
