// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/chrome_user_manager_impl.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/users/affiliation.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater_ash.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/network/proxy/proxy_config_service_impl.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/timezone/timezone_resolver.h"
#include "chromeos/components/onc/certificate_scope.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "components/account_id/account_id.h"
#include "components/crash/core/common/crash_key.h"
#include "components/policy/core/common/cloud/affiliation.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/proxy_config/proxy_prefs.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_pref_names.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/wm/core/wm_core_switches.h"

namespace ash {

// TODO(b/278643115) Remove the using when moved.
namespace prefs {
using user_manager::prefs::kDeviceLocalAccountPendingDataRemoval;
using user_manager::prefs::kDeviceLocalAccountsWithSavedData;
using user_manager::prefs::kRegularUsersPref;
}  // namespace prefs

namespace {

using ::content::BrowserThread;

policy::MinimumVersionPolicyHandler* GetMinimumVersionPolicyHandler() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetMinimumVersionPolicyHandler();
}

user_manager::UserManager::EphemeralModeConfig CreateEphemeralModeConfig(
    ash::CrosSettings* cros_settings) {
  DCHECK(cros_settings);

  bool ephemeral_users_enabled = false;
  // Only `ChromeUserManagerImpl` is allowed to directly use this setting. All
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

// static
std::unique_ptr<ChromeUserManagerImpl>
ChromeUserManagerImpl::CreateChromeUserManager() {
  return base::WrapUnique(new ChromeUserManagerImpl());
}

ChromeUserManagerImpl::ChromeUserManagerImpl()
    : UserManagerBase(
          std::make_unique<UserManagerDelegateImpl>(),
          base::SingleThreadTaskRunner::HasCurrentDefault()
              ? base::SingleThreadTaskRunner::GetCurrentDefault()
              : nullptr,
          g_browser_process ? g_browser_process->local_state() : nullptr,
          CrosSettings::Get()),
      device_local_account_policy_service_(nullptr) {
  // UserManager instance should be used only on UI thread.
  // (or in unit tests)
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  DeviceSettingsService::Get()->AddObserver(this);

  // Since we're in ctor postpone any actions till this is fully created.
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                       weak_factory_.GetWeakPtr()));
  }

  allow_guest_subscription_ = cros_settings()->AddSettingsObserver(
      kAccountsPrefAllowGuest,
      base::BindRepeating(&UserManager::NotifyUsersSignInConstraintsChanged,
                          weak_factory_.GetWeakPtr()));
  // For user allowlist.
  users_subscription_ = cros_settings()->AddSettingsObserver(
      kAccountsPrefUsers,
      base::BindRepeating(&UserManager::NotifyUsersSignInConstraintsChanged,
                          weak_factory_.GetWeakPtr()));
  family_link_accounts_subscription_ = cros_settings()->AddSettingsObserver(
      kAccountsPrefFamilyLinkAccountsAllowed,
      base::BindRepeating(&UserManager::NotifyUsersSignInConstraintsChanged,
                          weak_factory_.GetWeakPtr()));

  ephemeral_users_enabled_subscription_ = cros_settings()->AddSettingsObserver(
      kAccountsPrefEphemeralUsersEnabled,
      base::BindRepeating(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                          weak_factory_.GetWeakPtr()));
  local_accounts_subscription_ = cros_settings()->AddSettingsObserver(
      kAccountsPrefDeviceLocalAccounts,
      base::BindRepeating(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                          weak_factory_.GetWeakPtr()));

  // |this| is sometimes initialized before owner is ready in CrosSettings for
  // the consoldiated consent screen flow. Listen for changes to owner setting
  // to ensure that owner changes are reflected in |this|.
  // TODO(crbug.com/1307359): Investigate using RetrieveTrustedDevicePolicies
  // instead of UpdateOwnerId.
  owner_subscription_ = cros_settings()->AddSettingsObserver(
      kDeviceOwner, base::BindRepeating(&ChromeUserManagerImpl::UpdateOwnerId,
                                        weak_factory_.GetWeakPtr()));

  if (GetMinimumVersionPolicyHandler()) {
    GetMinimumVersionPolicyHandler()->AddObserver(this);
  }
}

void ChromeUserManagerImpl::UpdateOwnerId() {
  std::string owner_email;
  cros_settings()->GetString(kDeviceOwner, &owner_email);

  user_manager::KnownUser known_user(GetLocalState());
  const AccountId owner_account_id = known_user.GetAccountId(
      owner_email, std::string() /* id */, AccountType::UNKNOWN);
  SetOwnerId(owner_account_id);
}

ChromeUserManagerImpl::~ChromeUserManagerImpl() {
  if (DeviceSettingsService::IsInitialized()) {
    DeviceSettingsService::Get()->RemoveObserver(this);
  }
}

void ChromeUserManagerImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  UserManagerBase::Shutdown();

  if (GetMinimumVersionPolicyHandler()) {
    GetMinimumVersionPolicyHandler()->RemoveObserver(this);
  }

  ephemeral_users_enabled_subscription_ = {};
  local_accounts_subscription_ = {};

  if (device_local_account_policy_service_) {
    device_local_account_policy_service_->RemoveObserver(this);
  }
}

void ChromeUserManagerImpl::OwnershipStatusChanged() {
  if (!device_local_account_policy_service_) {
    policy::BrowserPolicyConnectorAsh* connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();
    device_local_account_policy_service_ =
        connector->GetDeviceLocalAccountPolicyService();
    if (device_local_account_policy_service_) {
      device_local_account_policy_service_->AddObserver(this);
    }
  }
  RetrieveTrustedDevicePolicies();
}

void ChromeUserManagerImpl::OnPolicyUpdated(const std::string& user_id) {
  user_manager::KnownUser known_user(GetLocalState());
  const AccountId account_id = known_user.GetAccountId(
      user_id, std::string() /* id */, AccountType::UNKNOWN);
  const user_manager::User* user = FindUser(account_id);
  if (!user || user->GetType() != user_manager::UserType::kPublicAccount) {
    return;
  }
  auto display_name = GetDisplayName(user_id);
  if (display_name) {
    SaveUserDisplayName(user->GetAccountId(), *display_name);
  }
}

void ChromeUserManagerImpl::OnDeviceLocalAccountsChanged() {
  // No action needed here, changes to the list of device-local accounts get
  // handled via the kAccountsPrefDeviceLocalAccounts device setting observer.
}

void ChromeUserManagerImpl::RetrieveTrustedDevicePolicies() {
  // Local state may not be initialized in unit_tests.
  if (!GetLocalState()) {
    return;
  }

  SetEphemeralModeConfig(EphemeralModeConfig());

  // Schedule a callback if device policy has not yet been verified.
  if (CrosSettingsProvider::TRUSTED !=
      cros_settings()->PrepareTrustedValues(
          base::BindOnce(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                         weak_factory_.GetWeakPtr()))) {
    return;
  }

  SetEphemeralModeConfig(CreateEphemeralModeConfig(cros_settings()));

  std::string owner_email;
  cros_settings()->GetString(kDeviceOwner, &owner_email);
  user_manager::KnownUser known_user(GetLocalState());
  const AccountId owner_account_id = known_user.GetAccountId(
      owner_email, std::string() /* id */, AccountType::UNKNOWN);
  SetOwnerId(owner_account_id);

  auto device_local_accounts = policy::GetDeviceLocalAccounts(cros_settings());
  std::vector<DeviceLocalAccountInfo> device_local_account_info_list;
  for (const auto& account : device_local_accounts) {
    DeviceLocalAccountInfo info(
        account.user_id,
        *chrome_user_manager_util::DeviceLocalAccountTypeToUserType(
            account.type));
    if (info.type == user_manager::UserType::kPublicAccount) {
      info.display_name = GetDisplayName(info.user_id);
    }
    device_local_account_info_list.push_back(std::move(info));
  }
  bool changed =
      UpdateAndCleanUpDeviceLocalAccounts(device_local_account_info_list);

  // Remove ephemeral regular users (except the owner) when on the login screen.
  if (!IsUserLoggedIn()) {
    // Take snapshot because DeleteUser called in the loop will update it.
    std::vector<raw_ptr<user_manager::User, VectorExperimental>> users = users_;
    for (user_manager::User* user : users) {
      const AccountId account_id = user->GetAccountId();
      if (user->HasGaiaAccount() && account_id != GetOwnerAccountId() &&
          IsEphemeralAccountId(account_id)) {
        RemoveUserFromListImpl(
            account_id,
            /*reason=*/
            user_manager::UserRemovalReason::DEVICE_EPHEMERAL_USERS_ENABLED,
            /*trigger_cryptohome_removal=*/false);
        changed = true;
      }
    }
  }

  if (changed) {
    NotifyLocalStateChanged();
  }
}

void ChromeUserManagerImpl::RemoveNonCryptohomeData(
    const AccountId& account_id) {
  // TODO(tbarzic): Forward data removal request to HammerDeviceHandler,
  // instead of removing the prefs value here.
  if (GetLocalState()->FindPreference(prefs::kDetachableBaseDevices)) {
    ScopedDictPrefUpdate update(GetLocalState(), prefs::kDetachableBaseDevices);
    update->Remove(account_id.HasAccountIdKey() ? account_id.GetAccountIdKey()
                                                : account_id.GetUserEmail());
  }

  UserManagerBase::RemoveNonCryptohomeData(account_id);
}

void ChromeUserManagerImpl::RemovePendingDeviceLocalAccount() {
  PrefService* local_state = GetLocalState();
  const std::string device_local_account_pending_data_removal =
      local_state->GetString(prefs::kDeviceLocalAccountPendingDataRemoval);
  if (device_local_account_pending_data_removal.empty() ||
      (IsUserLoggedIn() &&
       device_local_account_pending_data_removal ==
           GetActiveUser()->GetAccountId().GetUserEmail())) {
    return;
  }

  RemoveUserFromListImpl(
      AccountId::FromUserEmail(device_local_account_pending_data_removal),
      user_manager::UserRemovalReason::DEVICE_LOCAL_ACCOUNT_UPDATED,
      /*trigger_cryptohome_removal=*/false);
  local_state->ClearPref(prefs::kDeviceLocalAccountPendingDataRemoval);
}

bool ChromeUserManagerImpl::UpdateAndCleanUpDeviceLocalAccounts(
    const std::vector<DeviceLocalAccountInfo>& device_local_accounts) {
  // Try to remove any device local account data marked as pending removal.
  RemovePendingDeviceLocalAccount();

  // Persist the new list of device local accounts in a pref. These accounts
  // will be loaded in LoadDeviceLocalAccounts() on the next reboot regardless
  // of whether they still exist in kAccountsPrefDeviceLocalAccounts, allowing
  // us to clean up associated data if they disappear from policy.
  ScopedListPrefUpdate prefs_device_local_accounts_update(
      GetLocalState(), prefs::kDeviceLocalAccountsWithSavedData);
  prefs_device_local_accounts_update->clear();
  for (const auto& account : device_local_accounts) {
    prefs_device_local_accounts_update->Append(account.user_id);
  }

  // If the list of device local accounts has not changed, return.
  {
    bool changed = false;
    size_t i = 0;
    for (const user_manager::User* user : users_) {
      if (!user->IsDeviceLocalAccount()) {
        continue;
      }
      if (i >= device_local_accounts.size()) {
        changed = true;
        break;
      }
      if (user->GetAccountId().GetUserEmail() !=
              device_local_accounts[i].user_id ||
          user->GetType() != device_local_accounts[i].type) {
        changed = true;
        break;
      }
      ++i;
    }
    if (i == device_local_accounts.size() && !changed) {
      return false;
    }
  }

  // Remove the old device local accounts from the user list.
  // Take snapshot because RemoveUserFromListImpl will update |user_|.
  std::vector<user_manager::User*> users(users_.begin(), users_.end());
  for (user_manager::User* user : users) {
    if (!user->IsDeviceLocalAccount()) {
      // Non device local account is not a target to be removed.
      continue;
    }
    if (base::ranges::any_of(
            device_local_accounts, [user](const DeviceLocalAccountInfo& info) {
              return info.user_id == user->GetAccountId().GetUserEmail() &&
                     info.type == user->GetType();
            })) {
      // The account exists in new device local accounts. Do not remove.
      continue;
    }
    if (user == GetActiveUser()) {
      // This user is active, so keep the instance. Instead, mark it as
      // pending removal, so it will be removed in the next turn.
      GetLocalState()->SetString(prefs::kDeviceLocalAccountPendingDataRemoval,
                                 user->GetAccountId().GetUserEmail());
      std::erase(users_, user);
      continue;
    }

    // Remove the instance.
    RemoveUserFromListImpl(
        user->GetAccountId(),
        user_manager::UserRemovalReason::DEVICE_LOCAL_ACCOUNT_UPDATED,
        /*trigger_cryptohome_removal=*/false);
  }

  // Add the new device local accounts to the front of the user list.
  for (size_t i = 0; i < device_local_accounts.size(); ++i) {
    const DeviceLocalAccountInfo& account = device_local_accounts[i];
    auto iter = std::find_if(users_.begin() + i, users_.end(),
                             [&account](const user_manager::User* user) {
                               return user->GetAccountId().GetUserEmail() ==
                                          account.user_id &&
                                      user->GetType() == account.type;
                             });
    if (iter != users_.end()) {
      // Found the instance. Rotate the `users_` to place the found user at
      // the i-th position.
      std::rotate(users_.begin() + i, iter, iter + 1);
    } else {
      // Not found so create an instance.
      // Using `new` to access a non-public constructor.
      user_storage_.push_back(base::WrapUnique(new user_manager::User(
          AccountId::FromUserEmail(account.user_id), account.type)));
      users_.insert(users_.begin() + i, user_storage_.back().get());
    }
    if (account.display_name) {
      SaveUserDisplayName(AccountId::FromUserEmail(account.user_id),
                          *account.display_name);
    }
  }

  for (auto& observer : observer_list_) {
    observer.OnDeviceLocalUserListUpdated();
  }

  return true;
}

std::optional<std::u16string> ChromeUserManagerImpl::GetDisplayName(
    std::string_view user_id) {
  if (!device_local_account_policy_service_) {
    return std::nullopt;
  }

  policy::DeviceLocalAccountPolicyBroker* broker =
      device_local_account_policy_service_->GetBrokerForUser(user_id);
  if (!broker) {
    return std::nullopt;
  }

  return base::UTF8ToUTF16(broker->GetDisplayName());
}

void ChromeUserManagerImpl::OnMinimumVersionStateChanged() {
  NotifyUsersSignInConstraintsChanged();
}

}  // namespace ash
