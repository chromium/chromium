// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/chrome_user_selection_screen.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/lock_screen_utils.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chrome/browser/ui/webui/ash/login/l10n_util.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"

namespace ash {

ChromeUserSelectionScreen::ChromeUserSelectionScreen(
    DisplayedScreen display_type)
    : UserSelectionScreen(display_type) {
  device_local_account_policy_service_ =
      g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->GetDeviceLocalAccountPolicyService();
  if (device_local_account_policy_service_) {
    device_local_account_policy_service_->AddObserver(this);
  }
}

ChromeUserSelectionScreen::~ChromeUserSelectionScreen() {
  if (device_local_account_policy_service_) {
    device_local_account_policy_service_->RemoveObserver(this);
  }
}

void ChromeUserSelectionScreen::Init(const user_manager::UserList& users) {
  UserSelectionScreen::Init(users);

  // Retrieve the current policy for all users.
  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end(); ++it) {
    if ((*it)->GetType() == user_manager::UserType::kPublicAccount) {
      OnPolicyUpdated((*it)->GetAccountId().GetUserEmail());
    }
  }
}

void ChromeUserSelectionScreen::OnPolicyUpdated(const std::string& user_id) {
  policy::DeviceLocalAccountPolicyBroker* broker =
      device_local_account_policy_service_->GetBrokerForUser(user_id);
  if (!broker)
    return;

  CheckForPublicSessionDisplayNameChange(broker);
  CheckForPublicSessionLocalePolicyChange(broker);
  CheckIfFullManagementDisclosureNeeded(broker);
}

void ChromeUserSelectionScreen::OnDeviceLocalAccountsChanged() {
  // Nothing to do here. When the list of device-local accounts changes, the
  // entire UI is reloaded.
}

void ChromeUserSelectionScreen::CheckForPublicSessionDisplayNameChange(
    policy::DeviceLocalAccountPolicyBroker* broker) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const AccountId account_id = known_user.GetAccountId(
      broker->user_id(), std::string() /* id */, AccountType::UNKNOWN);
  DCHECK(account_id.is_valid());
  const std::string& display_name = broker->GetDisplayName();
  if (display_name == public_session_display_names_[account_id])
    return;

  public_session_display_names_[account_id] = display_name;

  if (!users_loaded_)
    return;

  if (!display_name.empty()) {
    // If a new display name was set by policy, notify the UI about it.
    LoginScreen::Get()->GetModel()->SetPublicSessionDisplayName(account_id,
                                                                display_name);
    return;
  }

  // When no display name is set by policy, the `User`, owned by `UserManager`,
  // decides what display name to use. However, the order in which `UserManager`
  // and `this` are informed of the display name change is undefined. Post a
  // task that will update the UI after the UserManager is guaranteed to have
  // been informed of the change.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromeUserSelectionScreen::SetPublicSessionDisplayName,
                     weak_factory_.GetWeakPtr(), account_id));
}

void ChromeUserSelectionScreen::CheckForPublicSessionLocalePolicyChange(
    policy::DeviceLocalAccountPolicyBroker* broker) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const AccountId account_id = known_user.GetAccountId(
      broker->user_id(), std::string() /* id */, AccountType::UNKNOWN);
  DCHECK(account_id.is_valid());
  const policy::PolicyMap::Entry* entry =
      broker->core()->store()->policy_map().Get(policy::key::kSessionLocales);

  // Parse the list of recommended locales set by policy.
  std::vector<std::string> new_recommended_locales;
  if (entry && entry->level == policy::POLICY_LEVEL_RECOMMENDED &&
      entry->value(base::Value::Type::LIST)) {
    for (const auto& locale_entry :
         entry->value(base::Value::Type::LIST)->GetList()) {
      if (!locale_entry.is_string()) {
        NOTREACHED_IN_MIGRATION();
        new_recommended_locales.clear();
        break;
      }
      new_recommended_locales.push_back(locale_entry.GetString());
    }
  }

  std::vector<std::string>& recommended_locales =
      public_session_recommended_locales_[account_id];

  if (new_recommended_locales != recommended_locales)
    SetPublicSessionLocales(account_id, new_recommended_locales);

  if (new_recommended_locales.empty())
    public_session_recommended_locales_.erase(account_id);
  else
    recommended_locales = new_recommended_locales;
}

void ChromeUserSelectionScreen::CheckIfFullManagementDisclosureNeeded(
    policy::DeviceLocalAccountPolicyBroker* broker) {
  SetPublicSessionShowFullManagementDisclosure(
      ash::login::IsFullManagementDisclosureNeeded(broker));
}

void ChromeUserSelectionScreen::SetPublicSessionDisplayName(
    const AccountId& account_id) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  if (!user || user->GetType() != user_manager::UserType::kPublicAccount) {
    return;
  }

  LoginScreen::Get()->GetModel()->SetPublicSessionDisplayName(
      account_id, base::UTF16ToUTF8(user->GetDisplayName()));
}

void ChromeUserSelectionScreen::SetPublicSessionLocales(
    const AccountId& account_id,
    const std::vector<std::string>& recommended_locales) {
  if (!users_loaded_)
    return;

  // Construct the list of available locales. This list consists of the
  // recommended locales, followed by all others.
  base::Value::List available_locales =
      GetUILanguageList(&recommended_locales, std::string(),
                        input_method::InputMethodManager::Get());

  // Set the initially selected locale to the first recommended locale that is
  // actually available or the current UI locale if none of them are available.
  const std::string default_locale =
      FindMostRelevantLocale(recommended_locales, available_locales,
                             g_browser_process->GetApplicationLocale());

  // Set a flag to indicate whether the list of recommended locales contains at
  // least two entries. This is used to decide whether the public session pod
  // expands to its basic form (for zero or one recommended locales) or the
  // advanced form (two or more recommended locales).
  const bool two_or_more_recommended_locales = recommended_locales.size() >= 2;

  // Notify the UI.
  LoginScreen::Get()->GetModel()->SetPublicSessionLocales(
      account_id,
      lock_screen_utils::FromListValueToLocaleItem(
          std::move(available_locales)),
      default_locale, two_or_more_recommended_locales);

  // Send a request to get keyboard layouts for `default_locale`.
  LoginScreenClientImpl::Get()->RequestPublicSessionKeyboardLayouts(
      account_id, default_locale);
}

void ChromeUserSelectionScreen::SetPublicSessionShowFullManagementDisclosure(
    bool show_full_management_disclosure) {
  LoginScreen::Get()->GetModel()->SetPublicSessionShowFullManagementDisclosure(
      show_full_management_disclosure);
}

}  // namespace ash
