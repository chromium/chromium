// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_POLICY_USER_MANAGER_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_POLICY_USER_MANAGER_CONTROLLER_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"
#include "chrome/browser/ash/settings/device_settings_service.h"

namespace user_manager {
class UserManager;
}  // namespace user_manager

namespace ash {

class CrosSettings;

// Observes policy related events for user manager updates, and triggers methods
// defined in UserManager.
class PolicyUserManagerController
    : public DeviceSettingsService::Observer,
      public policy::MinimumVersionPolicyHandler::Observer,
      public policy::DeviceLocalAccountPolicyService::Observer {
 public:
  PolicyUserManagerController(
      user_manager::UserManager* user_manager,
      CrosSettings* cros_settings,
      DeviceSettingsService* device_settings_service,
      policy::MinimumVersionPolicyHandler* minimum_version_policy_handler);
  PolicyUserManagerController(const PolicyUserManagerController&) = delete;
  PolicyUserManagerController& operator=(const PolicyUserManagerController&) =
      delete;
  ~PolicyUserManagerController() override;

  // DeviceSettingsService::Observer:
  void OwnershipStatusChanged() override;

  // policy::MinimumVersionPolicyHandler::Observer:
  void OnMinimumVersionStateChanged() override;

  // policy::DeviceLocalAccountPolicyService::Observer:
  void OnPolicyUpdated(const std::string& user_id) override;
  void OnDeviceLocalAccountsChanged() override;

 private:
  friend class UserManagerTest;

  // Retrieves trusted device policies and removes users from the persistent
  // list if ephemeral users are enabled. Schedules a callback to itself if
  // trusted device policies are not yet available.
  void RetrieveTrustedDevicePolicies();

  void UpdateOwnerId();

  // Returns the display name taken from policy, expected to be used for
  // public accounts.
  std::optional<std::u16string> GetDisplayName(std::string_view user_id);

  const raw_ptr<user_manager::UserManager> user_manager_;
  const raw_ptr<CrosSettings> cros_settings_;

  std::vector<base::CallbackListSubscription> cros_settings_subscriptions_;
  base::ScopedObservation<DeviceSettingsService,
                          DeviceSettingsService::Observer>
      device_settings_service_observation_{this};
  base::ScopedObservation<policy::MinimumVersionPolicyHandler,
                          policy::MinimumVersionPolicyHandler::Observer>
      minimum_version_policy_handler_observation_{this};
  base::ScopedObservation<policy::DeviceLocalAccountPolicyService,
                          policy::DeviceLocalAccountPolicyService::Observer>
      device_local_account_policy_service_observation_{this};

  base::WeakPtrFactory<PolicyUserManagerController> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_POLICY_USER_MANAGER_CONTROLLER_H_
