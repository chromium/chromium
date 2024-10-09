// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/policy_user_manager_controller.h"

#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_broker.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"

namespace ash {
namespace {

user_manager::UserManager::EphemeralModeConfig CreateEphemeralModeConfig(
    ash::CrosSettings* cros_settings) {
  DCHECK(cros_settings);

  bool ephemeral_users_enabled = false;
  // Only `UserManagerImpl` is allowed to directly use this setting. All
  // other clients have to use `UserManager::IsEphemeralAccountId()` function to
  // get ephemeral mode for account ID. Such rule is needed because there are
  // new policies(e.g.kiosk ephemeral mode) that overrides behaviour of
  // the current setting for some accounts.
  cros_settings->GetBoolean(ash::kAccountsPrefEphemeralUsersEnabled,
                            &ephemeral_users_enabled);

  std::vector<AccountId> ephemeral_accounts, non_ephemeral_accounts;

  const auto accounts = policy::GetDeviceLocalAccounts(cros_settings);
  for (const auto& account : accounts) {
    switch (account.ephemeral_mode) {
      case policy::DeviceLocalAccount::EphemeralMode::kEnable:
        ephemeral_accounts.push_back(AccountId::FromUserEmail(account.user_id));
        break;
      case policy::DeviceLocalAccount::EphemeralMode::kDisable:
        non_ephemeral_accounts.push_back(
            AccountId::FromUserEmail(account.user_id));
        break;
      case policy::DeviceLocalAccount::EphemeralMode::kUnset:
      case policy::DeviceLocalAccount::EphemeralMode::kFollowDeviceWidePolicy:
        // Do nothing.
        break;
    }
  }

  return user_manager::UserManager::EphemeralModeConfig(
      ephemeral_users_enabled, std::move(ephemeral_accounts),
      std::move(non_ephemeral_accounts));
}

}  // namespace

PolicyUserManagerController::PolicyUserManagerController(
    user_manager::UserManager* user_manager,
    CrosSettings* cros_settings,
    DeviceSettingsService* device_settings_service,
    policy::MinimumVersionPolicyHandler* minimum_version_policy_handler)
    : user_manager_(user_manager), cros_settings_(cros_settings) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // UserManager outlives PolicyUserManagerController, and subscriptions are
  // destroyed on destroying PolicyUserManagerController. So, base::Unretained
  // works for the case.
  cros_settings_subscriptions_.push_back(cros_settings_->AddSettingsObserver(
      kAccountsPrefAllowGuest,
      base::BindRepeating(
          &user_manager::UserManager::NotifyUsersSignInConstraintsChanged,
          base::Unretained(user_manager_.get()))));
  // For user allowlist.
  cros_settings_subscriptions_.push_back(cros_settings_->AddSettingsObserver(
      kAccountsPrefUsers,
      base::BindRepeating(
          &user_manager::UserManager::NotifyUsersSignInConstraintsChanged,
          base::Unretained(user_manager_.get()))));
  cros_settings_subscriptions_.push_back(cros_settings_->AddSettingsObserver(
      kAccountsPrefFamilyLinkAccountsAllowed,
      base::BindRepeating(
          &user_manager::UserManager::NotifyUsersSignInConstraintsChanged,
          base::Unretained(user_manager_.get()))));

  cros_settings_subscriptions_.push_back(cros_settings_->AddSettingsObserver(
      kAccountsPrefEphemeralUsersEnabled,
      base::BindRepeating(
          &PolicyUserManagerController::RetrieveTrustedDevicePolicies,
          weak_factory_.GetWeakPtr())));
  cros_settings_subscriptions_.push_back(cros_settings_->AddSettingsObserver(
      kAccountsPrefDeviceLocalAccounts,
      base::BindRepeating(
          &PolicyUserManagerController::RetrieveTrustedDevicePolicies,
          weak_factory_.GetWeakPtr())));

  // |this| is sometimes initialized before owner is ready in CrosSettings for
  // the consoldiated consent screen flow. Listen for changes to owner setting
  // to ensure that owner changes are reflected in |this|.
  // TODO(crbug.com/1307359): Investigate using RetrieveTrustedDevicePolicies
  // instead of UpdateOwnerId.
  cros_settings_subscriptions_.push_back(cros_settings_->AddSettingsObserver(
      kDeviceOwner,
      base::BindRepeating(&PolicyUserManagerController::UpdateOwnerId,
                          weak_factory_.GetWeakPtr())));

  device_settings_service_observation_.Observe(device_settings_service);
  if (minimum_version_policy_handler) {
    minimum_version_policy_handler_observation_.Observe(
        minimum_version_policy_handler);
  } else {
    CHECK_IS_TEST();
  }

  // Since we're in ctor postpone any actions till this is fully created.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PolicyUserManagerController::RetrieveTrustedDevicePolicies,
          weak_factory_.GetWeakPtr()));
}

PolicyUserManagerController::~PolicyUserManagerController() = default;

void PolicyUserManagerController::OwnershipStatusChanged() {
  if (!device_local_account_policy_service_observation_.IsObserving()) {
    device_local_account_policy_service_observation_.Observe(
        g_browser_process->platform_part()
            ->browser_policy_connector_ash()
            ->GetDeviceLocalAccountPolicyService());
  }
  RetrieveTrustedDevicePolicies();
}

void PolicyUserManagerController::OnMinimumVersionStateChanged() {
  user_manager_->NotifyUsersSignInConstraintsChanged();
}

void PolicyUserManagerController::OnPolicyUpdated(const std::string& user_id) {
  user_manager::KnownUser known_user(user_manager_->GetLocalState());
  const AccountId account_id = known_user.GetAccountId(
      user_id, std::string() /* id */, AccountType::UNKNOWN);
  const user_manager::User* user = user_manager_->FindUser(account_id);
  if (!user || user->GetType() != user_manager::UserType::kPublicAccount) {
    return;
  }
  auto display_name = GetDisplayName(user_id);
  if (display_name) {
    user_manager_->SaveUserDisplayName(user->GetAccountId(), *display_name);
  }
}

void PolicyUserManagerController::OnDeviceLocalAccountsChanged() {
  // No action needed here, changes to the list of device-local accounts get
  // handled via the kAccountsPrefDeviceLocalAccounts device setting observer.
}

void PolicyUserManagerController::RetrieveTrustedDevicePolicies() {
  // Local state may not be initialized in unit_tests.
  if (!user_manager_->GetLocalState()) {
    CHECK_IS_TEST();
    return;
  }

  user_manager_->SetEphemeralModeConfig(
      user_manager::UserManager::EphemeralModeConfig());

  // Schedule a callback if device policy has not yet been verified.
  if (CrosSettingsProvider::TRUSTED !=
      cros_settings_->PrepareTrustedValues(base::BindOnce(
          &PolicyUserManagerController::RetrieveTrustedDevicePolicies,
          weak_factory_.GetWeakPtr()))) {
    return;
  }

  user_manager_->SetEphemeralModeConfig(
      CreateEphemeralModeConfig(cros_settings_));
  UpdateOwnerId();

  auto device_local_accounts = policy::GetDeviceLocalAccounts(cros_settings_);
  std::vector<user_manager::UserManager::DeviceLocalAccountInfo>
      device_local_account_info_list;
  for (const auto& account : device_local_accounts) {
    user_manager::UserManager::DeviceLocalAccountInfo info(
        account.user_id,
        *chrome_user_manager_util::DeviceLocalAccountTypeToUserType(
            account.type));
    if (info.type == user_manager::UserType::kPublicAccount) {
      info.display_name = GetDisplayName(info.user_id);
    }
    device_local_account_info_list.push_back(std::move(info));
  }
  bool changed = user_manager_->UpdateDeviceLocalAccountUser(
      device_local_account_info_list);

  // Remove ephemeral regular users (except the owner) when on the login screen.
  if (!user_manager_->IsUserLoggedIn()) {
    changed |= user_manager_->RemoveStaleEphemeralUsers();
  }

  if (changed) {
    user_manager_->NotifyLocalStateChanged();
  }
}

void PolicyUserManagerController::UpdateOwnerId() {
  std::string owner_email;
  cros_settings_->GetString(kDeviceOwner, &owner_email);

  user_manager::KnownUser known_user(user_manager_->GetLocalState());
  const AccountId owner_account_id = known_user.GetAccountId(
      owner_email, std::string() /* id */, AccountType::UNKNOWN);
  user_manager_->SetOwnerId(owner_account_id);
}

std::optional<std::u16string> PolicyUserManagerController::GetDisplayName(
    std::string_view user_id) {
  auto* service = device_local_account_policy_service_observation_.GetSource();
  if (!service) {
    return std::nullopt;
  }

  policy::DeviceLocalAccountPolicyBroker* broker =
      service->GetBrokerForUser(user_id);
  if (!broker) {
    return std::nullopt;
  }

  return base::UTF8ToUTF16(broker->GetDisplayName());
}

}  // namespace ash
