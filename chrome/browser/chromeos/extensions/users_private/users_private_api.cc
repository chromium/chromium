// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/users_private/users_private_api.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/extensions/users_private/users_private_delegate.h"
#include "chrome/browser/chromeos/extensions/users_private/users_private_delegate_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/users_private.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "extensions/browser/extension_function_registry.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace extensions {

namespace {

bool IsEnterpriseManaged() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->IsEnterpriseManaged();
}

bool IsChild(Profile* profile) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return false;

  return user->GetType() == user_manager::UserType::USER_TYPE_CHILD;
}

bool IsOwnerProfile(Profile* profile) {
  return profile &&
         ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(profile)
             ->IsOwner();
}

bool CanModifyUserList(content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return !IsEnterpriseManaged() && IsOwnerProfile(profile) && !IsChild(profile);
}

bool IsExistingUser(const std::string& username) {
  return ash::CrosSettings::Get()->FindEmailInList(
      chromeos::kAccountsPrefUsers, username, /*wildcard_match=*/nullptr);
}

// Creates User object for the exising user_manager::User .
api::users_private::User CreateApiUser(const std::string& email,
                                       const user_manager::User& user) {
  api::users_private::User api_user;
  api_user.email = email;
  api_user.display_email = user.GetDisplayEmail();
  api_user.name = base::UTF16ToUTF8(user.GetDisplayName());
  api_user.is_owner = user.GetAccountId() ==
                      user_manager::UserManager::Get()->GetOwnerAccountId();
  api_user.is_supervised = user.IsChildOrDeprecatedSupervised();
  api_user.is_child = user.IsChild();
  return api_user;
}

// Creates User object for the unknown user (i.e. not on device).
api::users_private::User CreateUnknownApiUser(const std::string& email) {
  api::users_private::User api_user;
  api_user.email = email;
  api_user.display_email = email;
  api_user.name = email;
  api_user.is_owner = false;
  api_user.is_supervised = false;
  api_user.is_child = false;
  return api_user;
}

std::unique_ptr<base::ListValue> GetUsersList(
    content::BrowserContext* browser_context) {
  std::unique_ptr<base::ListValue> user_list(new base::ListValue);

  if (!CanModifyUserList(browser_context))
    return user_list;

  // Create one list to set. This is needed because user white list update is
  // asynchronous and sequential. Before previous write comes back, cached
  // list is stale and should not be used for appending. See
  // http://crbug.com/127215
  std::unique_ptr<base::ListValue> email_list;

  UsersPrivateDelegate* delegate =
      UsersPrivateDelegateFactory::GetForBrowserContext(browser_context);
  PrefsUtil* prefs_util = delegate->GetPrefsUtil();

  std::unique_ptr<api::settings_private::PrefObject> users_pref_object =
      prefs_util->GetPref(chromeos::kAccountsPrefUsers);
  if (users_pref_object->value) {
    const base::ListValue* existing = nullptr;
    users_pref_object->value->GetAsList(&existing);
    email_list.reset(existing->DeepCopy());
  } else {
    email_list.reset(new base::ListValue());
  }

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();

  // Remove all supervised users. On the next step only supervised users
  // present on the device will be added back. Thus not present SU are
  // removed. No need to remove usual users as they can simply login back.
  for (size_t i = 0; i < email_list->GetSize(); ++i) {
    std::string email;
    email_list->GetString(i, &email);
    if (user_manager->IsDeprecatedSupervisedAccountId(
            AccountId::FromUserEmail(email))) {
      email_list->Remove(i, nullptr);
      --i;
    }
  }

  const user_manager::UserList& users = user_manager->GetUsers();
  for (const auto* user : users) {
    email_list->AppendIfNotPresent(
        std::make_unique<base::Value>(user->GetAccountId().GetUserEmail()));
  }

  if (ash::OwnerSettingsServiceAsh* service =
          ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
              browser_context)) {
    service->Set(chromeos::kAccountsPrefUsers, *email_list.get());
  }

  // Now populate the list of User objects for returning to the JS.
  for (size_t i = 0; i < email_list->GetSize(); ++i) {
    std::string email;
    email_list->GetString(i, &email);
    AccountId account_id = AccountId::FromUserEmail(email);
    const user_manager::User* user = user_manager->FindUser(account_id);
    user_list->Append(
        (user ? CreateApiUser(email, *user) : CreateUnknownApiUser(email))
            .ToValue());
  }

  return user_list;
}

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateGetUsersFunction

UsersPrivateGetUsersFunction::UsersPrivateGetUsersFunction() = default;

UsersPrivateGetUsersFunction::~UsersPrivateGetUsersFunction() = default;

ExtensionFunction::ResponseAction UsersPrivateGetUsersFunction::Run() {
  return RespondNow(OneArgument(
      base::Value::FromUniquePtrValue(GetUsersList(browser_context()))));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateIsUserInListFunction

UsersPrivateIsUserInListFunction::UsersPrivateIsUserInListFunction() = default;

UsersPrivateIsUserInListFunction::~UsersPrivateIsUserInListFunction() = default;

ExtensionFunction::ResponseAction UsersPrivateIsUserInListFunction::Run() {
  std::unique_ptr<api::users_private::IsUserInList::Params> parameters =
      api::users_private::IsUserInList::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  std::string username = gaia::CanonicalizeEmail(parameters->email);
  if (IsExistingUser(username)) {
    return RespondNow(OneArgument(base::Value(true)));
  }
  return RespondNow(OneArgument(base::Value(false)));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateAddUserFunction

UsersPrivateAddUserFunction::UsersPrivateAddUserFunction() = default;

UsersPrivateAddUserFunction::~UsersPrivateAddUserFunction() = default;

ExtensionFunction::ResponseAction UsersPrivateAddUserFunction::Run() {
  std::unique_ptr<api::users_private::AddUser::Params> parameters =
      api::users_private::AddUser::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  // Non-owners should not be able to add users.
  if (!CanModifyUserList(browser_context())) {
    return RespondNow(OneArgument(base::Value(false)));
  }

  std::string username = gaia::CanonicalizeEmail(parameters->email);
  if (IsExistingUser(username)) {
    return RespondNow(OneArgument(base::Value(false)));
  }

  base::Value username_value(username);

  UsersPrivateDelegate* delegate =
      UsersPrivateDelegateFactory::GetForBrowserContext(browser_context());
  PrefsUtil* prefs_util = delegate->GetPrefsUtil();
  bool added = prefs_util->AppendToListCrosSetting(chromeos::kAccountsPrefUsers,
                                                   username_value);
  return RespondNow(OneArgument(base::Value(added)));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateRemoveUserFunction

UsersPrivateRemoveUserFunction::UsersPrivateRemoveUserFunction() = default;

UsersPrivateRemoveUserFunction::~UsersPrivateRemoveUserFunction() = default;

ExtensionFunction::ResponseAction UsersPrivateRemoveUserFunction::Run() {
  std::unique_ptr<api::users_private::RemoveUser::Params> parameters =
      api::users_private::RemoveUser::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  // Non-owners should not be able to remove users.
  if (!CanModifyUserList(browser_context())) {
    return RespondNow(OneArgument(base::Value(false)));
  }

  base::Value canonical_email(gaia::CanonicalizeEmail(parameters->email));

  UsersPrivateDelegate* delegate =
      UsersPrivateDelegateFactory::GetForBrowserContext(browser_context());
  PrefsUtil* prefs_util = delegate->GetPrefsUtil();
  bool removed = prefs_util->RemoveFromListCrosSetting(
      chromeos::kAccountsPrefUsers, canonical_email);
  user_manager::UserManager::Get()->RemoveUser(
      AccountId::FromUserEmail(parameters->email), NULL);
  return RespondNow(OneArgument(base::Value(removed)));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateIsUserListManagedFunction

UsersPrivateIsUserListManagedFunction::UsersPrivateIsUserListManagedFunction() {
}

UsersPrivateIsUserListManagedFunction::
    ~UsersPrivateIsUserListManagedFunction() {}

ExtensionFunction::ResponseAction UsersPrivateIsUserListManagedFunction::Run() {
  return RespondNow(OneArgument(base::Value(IsEnterpriseManaged())));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateGetCurrentUserFunction

UsersPrivateGetCurrentUserFunction::UsersPrivateGetCurrentUserFunction() =
    default;

UsersPrivateGetCurrentUserFunction::~UsersPrivateGetCurrentUserFunction() =
    default;

ExtensionFunction::ResponseAction UsersPrivateGetCurrentUserFunction::Run() {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(
          Profile::FromBrowserContext(browser_context()));
  return user ? RespondNow(OneArgument(base::Value::FromUniquePtrValue(
                    CreateApiUser(user->GetAccountId().GetUserEmail(), *user)
                        .ToValue())))
              : RespondNow(Error("No Current User"));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateGetLoginStatusFunction

UsersPrivateGetLoginStatusFunction::UsersPrivateGetLoginStatusFunction() =
    default;
UsersPrivateGetLoginStatusFunction::~UsersPrivateGetLoginStatusFunction() =
    default;

ExtensionFunction::ResponseAction UsersPrivateGetLoginStatusFunction::Run() {
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  const bool is_logged_in = user_manager && user_manager->IsUserLoggedIn();
  const bool is_screen_locked =
      session_manager::SessionManager::Get()->IsScreenLocked();

  auto result = std::make_unique<base::DictionaryValue>();
  result->SetKey("isLoggedIn", base::Value(is_logged_in));
  result->SetKey("isScreenLocked", base::Value(is_screen_locked));
  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(std::move(result))));
}

}  // namespace extensions
