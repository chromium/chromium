// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/policy_user_manager_controller.h"

#include "components/user_manager/user_manager.h"

namespace ash {

PolicyUserManagerController::PolicyUserManagerController(
    user_manager::UserManager* user_manager,
    policy::MinimumVersionPolicyHandler* minimum_version_policy_handler)
    : user_manager_(user_manager) {
  minimum_version_policy_handler_observation_.Observe(
      minimum_version_policy_handler);
}

PolicyUserManagerController::~PolicyUserManagerController() = default;

void PolicyUserManagerController::OnMinimumVersionStateChanged() {
  user_manager_->NotifyUsersSignInConstraintsChanged();
}

}  // namespace ash
