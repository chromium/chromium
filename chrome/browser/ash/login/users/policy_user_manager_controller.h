// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_POLICY_USER_MANAGER_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_POLICY_USER_MANAGER_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"

namespace user_manager {
class UserManager;
}  // namespace user_manager

namespace ash {

// Observes policy related events for user manager updates, and triggers methods
// defined in UserManager.
class PolicyUserManagerController
    : public policy::MinimumVersionPolicyHandler::Observer {
 public:
  PolicyUserManagerController(
      user_manager::UserManager* user_manager,
      policy::MinimumVersionPolicyHandler* minimum_version_policy_handler);
  PolicyUserManagerController(const PolicyUserManagerController&) = delete;
  PolicyUserManagerController& operator=(const PolicyUserManagerController&) =
      delete;
  ~PolicyUserManagerController() override;

  // policy::MinimumVersionPolicyHandler::Observer:
  void OnMinimumVersionStateChanged() override;

 private:
  const raw_ptr<user_manager::UserManager> user_manager_;
  base::ScopedObservation<policy::MinimumVersionPolicyHandler,
                          policy::MinimumVersionPolicyHandler::Observer>
      minimum_version_policy_handler_observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_POLICY_USER_MANAGER_CONTROLLER_H_
