// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_state_controller.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"

namespace ash {

AssistantStateController::AssistantStateController()
    : session_observer_(this) {}

AssistantStateController::~AssistantStateController() = default;

void AssistantStateController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  // For non-primary prefs, calling the method with nullptr will reset the
  // current registry.
  PrefService* primary_user_prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  RegisterPrefChanges(primary_user_prefs == pref_service ? primary_user_prefs
                                                         : nullptr);
}

}  // namespace ash
