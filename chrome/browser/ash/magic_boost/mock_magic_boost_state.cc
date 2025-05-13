// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/magic_boost/mock_magic_boost_state.h"

#include "base/functional/bind.h"
#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"

namespace ash {

MockMagicBoostState::MockMagicBoostState()
    : MagicBoostStateAsh(base::BindRepeating(
          []() { return static_cast<Profile*>(nullptr); })) {}

MockMagicBoostState::~MockMagicBoostState() = default;

}  // namespace ash
