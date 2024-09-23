// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/users_private/users_private_api.h"

#include <stddef.h>

#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/extensions/users_private/users_private_delegate.h"
#include "chrome/browser/ash/extensions/users_private/users_private_delegate_factory.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/users_private.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "extensions/browser/extension_function_registry.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace extensions {

namespace {

bool IsDeviceEnterpriseManaged() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->IsDeviceEnterpriseManaged();
}

bool IsChild(Profile* profile) {
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return false;

  return user->GetType() == user_manager::UserType::kChild;
}

bool IsOwnerProfile(Profile* profile) {
  return profile &&
         ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(profile)
             ->IsOwner();
}

bool CanModifyUserList(content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return !IsDeviceEnterpriseManaged() && IsOwnerProfile(profile) &&
         !IsChild(profile);
}

bool IsExistingUser(const std::string& username) {
  return ash::CrosSettings::Get()->FindEmailInList(
      ash::kAccountsPrefUsers, username, /*wildcard_match=*/nullptr);
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
  api_user.is_child = false;
  return api_user;
}

base::Value::List GetUsersList(content::BrowserContext* browser_context) {
  base::Value::List user_list;

  if (!CanModifyUserList(browser_context))
    return user_list;

  // Create one list to set. This is needed because user white list update is
  // asynchronous and sequential. Before previous write comes back, cached
  // list is stale and should not be used for appending. See
  // http://crbug.com/127215
  base::Value::List email_list;

  UsersPrivateDelegate* delegate =
      UsersPrivateDelegateFactory::GetForBrowserContext(browser_context);
  PrefsUtil* prefs_util = delegate->GetPrefsUtil();

  std::optional<api::settings_private::PrefObject> users_pref_object =
      prefs_util->GetPref(ash::kAccountsPrefUsers);
  if (users_pref_object->value && users_pref_object->value->is_list()) {
    email_list = users_pref_object->value->GetList().Clone();
  }

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();

  // Remove all supervised users. On the next step only supervised users
  // present on the device will be added back. Thus not present SU are
  // removed. No need to remove usual users as they can simply login back.
  email_list.EraseIf([user_manager](const base::Value& email_value) {
    const std::string* email = email_value.GetIfString();
    return email && user_manager->IsDeprecatedSupervisedAccountId(
                        AccountId::FromUserEmail(*email));
  });

  const user_manager::UserList& users = user_manager->GetUsers();
  for (const user_manager::User* user : users) {
    base::Value email_value(user->GetAccountId().GetUserEmail());
    if (!base::Contains(email_list, email_value))
      email_list.Append(std::move(email_value));
  }

  // Now populate the list of User objects for returning to the JS.
  for (const base::Value& email_value : email_list) {
    const std::string* maybe_email = email_value.GetIfString();
    std::string email = maybe_email ? *maybe_email : std::string();
    AccountId account_id = AccountId::FromUserEmail(email);
    const user_manager::User* user = user_manager->FindUser(account_id);
    user_list.Append(
        (user ? CreateApiUser(email, *user) : CreateUnknownApiUser(email))
            .ToValue());
  }

  if (ash::OwnerSettingsServiceAsh* service =
          ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
              browser_context)) {
    service->Set(ash::kAccountsPrefUsers, base::Value(std::move(email_list)));
  }

  return user_list;
}

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateGetUsersFunction

UsersPrivateGetUsersFunction::UsersPrivateGetUsersFunction() = default;

UsersPrivateGetUsersFunction::~UsersPrivateGetUsersFunction() = default;

ExtensionFunction::ResponseAction UsersPrivateGetUsersFunction::Run() {
  return RespondNow(WithArguments(GetUsersList(browser_context())));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateIsUserInListFunction

UsersPrivateIsUserInListFunction::UsersPrivateIsUserInListFunction() = default;

UsersPrivateIsUserInListFunction::~UsersPrivateIsUserInListFunction() = default;

ExtensionFunction::ResponseAction UsersPrivateIsUserInListFunction::Run() {
  std::optional<api::users_private::IsUserInList::Params> parameters =
      api::users_private::IsUserInList::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  std::string username = gaia::CanonicalizeEmail(parameters->email);
  if (IsExistingUser(username)) {
    return RespondNow(WithArguments(true));
  }
  return RespondNow(WithArguments(false));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateAddUserFunction

UsersPrivateAddUserFunction::UsersPrivateAddUserFunction() = default;

UsersPrivateAddUserFunction::~UsersPrivateAddUserFunction() = default;

ExtensionFunction::ResponseAction UsersPrivateAddUserFunction::Run() {
  std::optional<api::users_private::AddUser::Params> parameters =
      api::users_private::AddUser::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  // Non-owners should not be able to add users.
  if (!CanModifyUserList(browser_context())) {
    return RespondNow(WithArguments(false));
  }

  std::string username = gaia::CanonicalizeEmail(parameters->email);
  if (IsExistingUser(username)) {
    return RespondNow(WithArguments(false));
  }

  base::Value username_value(username);

  UsersPrivateDelegate* delegate =
      UsersPrivateDelegateFactory::GetForBrowserContext(browser_context());
  PrefsUtil* prefs_util = delegate->GetPrefsUtil();
  bool added = prefs_util->AppendToListCrosSetting(ash::kAccountsPrefUsers,
                                                   username_value);
  return RespondNow(WithArguments(added));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateRemoveUserFunction

UsersPrivateRemoveUserFunction::UsersPrivateRemoveUserFunction() = default;

UsersPrivateRemoveUserFunction::~UsersPrivateRemoveUserFunction() = default;

ExtensionFunction::ResponseAction UsersPrivateRemoveUserFunction::Run() {
  std::optional<api::users_private::RemoveUser::Params> parameters =
      api::users_private::RemoveUser::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  // Non-owners should not be able to remove users.
  if (!CanModifyUserList(browser_context())) {
    return RespondNow(WithArguments(false));
  }

  base::Value canonical_email(gaia::CanonicalizeEmail(parameters->email));

  UsersPrivateDelegate* delegate =
      UsersPrivateDelegateFactory::GetForBrowserContext(browser_context());
  PrefsUtil* prefs_util = delegate->GetPrefsUtil();
  bool removed = prefs_util->RemoveFromListCrosSetting(ash::kAccountsPrefUsers,
                                                       canonical_email);
  user_manager::UserManager::Get()->RemoveUser(
      AccountId::FromUserEmail(parameters->email),
      user_manager::UserRemovalReason::LOCAL_USER_INITIATED);
  return RespondNow(WithArguments(removed));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateIsUserListManagedFunction

UsersPrivateIsUserListManagedFunction::UsersPrivateIsUserListManagedFunction() {
}

UsersPrivateIsUserListManagedFunction::
    ~UsersPrivateIsUserListManagedFunction() {}

ExtensionFunction::ResponseAction UsersPrivateIsUserListManagedFunction::Run() {
  return RespondNow(WithArguments(IsDeviceEnterpriseManaged()));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateGetCurrentUserFunction

UsersPrivateGetCurrentUserFunction::UsersPrivateGetCurrentUserFunction() =
    default;

UsersPrivateGetCurrentUserFunction::~UsersPrivateGetCurrentUserFunction() =
    default;

ExtensionFunction::ResponseAction UsersPrivateGetCurrentUserFunction::Run() {
  const user_manager::User* user = ash::ProfileHelper::Get()->GetUserByProfile(
      Profile::FromBrowserContext(browser_context()));
  return user ? RespondNow(WithArguments(
                    CreateApiUser(user->GetAccountId().GetUserEmail(), *user)
                        .ToValue()))
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

  base::Value::Dict result;
  result.Set("isLoggedIn", base::Value(is_logged_in));
  result.Set("isScreenLocked", base::Value(is_screen_locked));
  return RespondNow(WithArguments(std::move(result)));
}

}  // namespace extensions
