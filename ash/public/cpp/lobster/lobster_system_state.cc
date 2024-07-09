// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/lobster/lobster_system_state.h"

#include "ash/public/cpp/lobster/lobster_enums.h"
#include "base/containers/enum_set.h"

namespace ash {

LobsterSystemState::LobsterSystemState(LobsterStatus status,
                                       SystemChecks failed_checks)
    : status(status), failed_checks(failed_checks) {}

LobsterSystemState::~LobsterSystemState() = default;

}  // namespace ash
