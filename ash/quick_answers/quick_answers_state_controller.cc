// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/quick_answers_state_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"

namespace ash {

QuickAnswersStateController::QuickAnswersStateController()
    : session_observer_(this) {}

QuickAnswersStateController::~QuickAnswersStateController() = default;

void QuickAnswersStateController::OnFirstSessionStarted() {
  if (!features::IsQuickAnswersV2Enabled())
    return;

  state_.RegisterPrefChanges(
      Shell::Get()->session_controller()->GetPrimaryUserPrefService());
}

}  // namespace ash
