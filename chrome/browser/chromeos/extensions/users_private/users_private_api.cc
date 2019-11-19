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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/extensions/users_private/users_private_delegate.h"
#include "chrome/browser/chromeos/extensions/users_private/users_private_delegate_factory.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
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
         chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
             profile)
             ->IsOwner();
}

bool CanModifyWhitelistedUsers(Profile* profile) {
  return !IsEnterpriseManaged() && IsOwnerProfile(profile) && !IsChild(profile);
}

bool IsExistingWhitelistedUser(const std::string& username) {
  return chromeos::CrosSettings::Get()->FindEmailInList(
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
  api_user.is_supervised = user.IsSupervised();
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

std::unique_ptr<base::ListValue> GetUsersList(Profile* profile,
                                      content::BrowserContext* browser_context)
{
  std::unique_ptr<base::ListValue> user_list(new base::ListValue);

  if (!CanModifyWhitelistedUsers(profile))
    return user_list;

  // Create one list to set. This is needed because user white list update is
  // asynchronous and sequential. Before previous write comes back, cached list
  // is stale and should not be used for appending. See http://crbug.com/127215
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

  // Remove all supervised users. On the next step only supervised users present
  // on the device will be added back. Thus not present SU are removed.
  // No need to remove usual users as they can simply login back.
  for (size_t i = 0; i < email_list->GetSize(); ++i) {
    std::string whitelisted_user;
    email_list->GetString(i, &whitelisted_user);
    if (user_manager->IsSupervisedAccountId(
            AccountId::FromUserEmail(whitelisted_user))) {
      email_list->Remove(i, NULL);
      --i;
    }
  }

  const user_manager::UserList& users = user_manager->GetUsers();
  for (const auto* user : users) {
    email_list->AppendIfNotPresent(
        std::make_unique<base::Value>(user->GetAccountId().GetUserEmail()));
  }

  if (chromeos::OwnerSettingsServiceChromeOS* service =
          chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
              profile)) {
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
// UsersPrivateGetWhitelistedUsersFunction

UsersPrivateGetWhitelistedUsersFunction::
    UsersPrivateGetWhitelistedUsersFunction()
    : chrome_details_(this) {
}

UsersPrivateGetWhitelistedUsersFunction::
    ~UsersPrivateGetWhitelistedUsersFunction() {
}

ExtensionFunction::ResponseAction
UsersPrivateGetWhitelistedUsersFunction::Run() {
  Profile* profile = chrome_details_.GetProfile();
  return RespondNow(OneArgument(GetUsersList(profile, browser_context())));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateIsWhitelistedUserFunction

UsersPrivateIsWhitelistedUserFunction::UsersPrivateIsWhitelistedUserFunction()
    : chrome_details_(this) {
}

UsersPrivateIsWhitelistedUserFunction::
    ~UsersPrivateIsWhitelistedUserFunction() {}

ExtensionFunction::ResponseAction UsersPrivateIsWhitelistedUserFunction::Run() {
  std::unique_ptr<api::users_private::IsWhitelistedUser::Params> parameters =
      api::users_private::IsWhitelistedUser::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  // GetUsersList called to clear the stale user name cache
  GetUsersList(chrome_details_.GetProfile(), browser_context());

  std::string username = gaia::CanonicalizeEmail(parameters->email);
  if (IsExistingWhitelistedUser(username)) {
    return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
  }
  return RespondNow(OneArgument(std::make_unique<base::Value>(false)));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateAddWhitelistedUserFunction

UsersPrivateAddWhitelistedUserFunction::UsersPrivateAddWhitelistedUserFunction()
    : chrome_details_(this) {
}

UsersPrivateAddWhitelistedUserFunction::
    ~UsersPrivateAddWhitelistedUserFunction() {
}

ExtensionFunction::ResponseAction
UsersPrivateAddWhitelistedUserFunction::Run() {
  std::unique_ptr<api::users_private::AddWhitelistedUser::Params> parameters =
      api::users_private::AddWhitelistedUser::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  // Non-owners should not be able to add users.
  if (!CanModifyWhitelistedUsers(chrome_details_.GetProfile())) {
    return RespondNow(OneArgument(std::make_unique<base::Value>(false)));
  }

  std::string username = gaia::CanonicalizeEmail(parameters->email);
  if (IsExistingWhitelistedUser(username)) {
    return RespondNow(OneArgument(std::make_unique<base::Value>(false)));
  }

  base::Value username_value(username);

  UsersPrivateDelegate* delegate =
      UsersPrivateDelegateFactory::GetForBrowserContext(browser_context());
  PrefsUtil* prefs_util = delegate->GetPrefsUtil();
  bool added = prefs_util->AppendToListCrosSetting(chromeos::kAccountsPrefUsers,
                                                   username_value);
  return RespondNow(OneArgument(std::make_unique<base::Value>(added)));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateRemoveWhitelistedUserFunction

UsersPrivateRemoveWhitelistedUserFunction::
    UsersPrivateRemoveWhitelistedUserFunction()
    : chrome_details_(this) {
}

UsersPrivateRemoveWhitelistedUserFunction::
    ~UsersPrivateRemoveWhitelistedUserFunction() {
}

ExtensionFunction::ResponseAction
UsersPrivateRemoveWhitelistedUserFunction::Run() {
  std::unique_ptr<api::users_private::RemoveWhitelistedUser::Params>
      parameters =
          api::users_private::RemoveWhitelistedUser::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  // Non-owners should not be able to remove users.
  if (!CanModifyWhitelistedUsers(chrome_details_.GetProfile())) {
    return RespondNow(OneArgument(std::make_unique<base::Value>(false)));
  }

  base::Value canonical_email(gaia::CanonicalizeEmail(parameters->email));

  UsersPrivateDelegate* delegate =
      UsersPrivateDelegateFactory::GetForBrowserContext(browser_context());
  PrefsUtil* prefs_util = delegate->GetPrefsUtil();
  bool removed = prefs_util->RemoveFromListCrosSetting(
      chromeos::kAccountsPrefUsers, canonical_email);
  user_manager::UserManager::Get()->RemoveUser(
      AccountId::FromUserEmail(parameters->email), NULL);
  return RespondNow(OneArgument(std::make_unique<base::Value>(removed)));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateIsWhitelistManagedFunction

UsersPrivateIsWhitelistManagedFunction::
    UsersPrivateIsWhitelistManagedFunction() {
}

UsersPrivateIsWhitelistManagedFunction::
    ~UsersPrivateIsWhitelistManagedFunction() {
}

ExtensionFunction::ResponseAction
UsersPrivateIsWhitelistManagedFunction::Run() {
  return RespondNow(
      OneArgument(std::make_unique<base::Value>(IsEnterpriseManaged())));
}

////////////////////////////////////////////////////////////////////////////////
// UsersPrivateGetCurrentUserFunction

UsersPrivateGetCurrentUserFunction::UsersPrivateGetCurrentUserFunction()
    : chrome_details_(this) {}

UsersPrivateGetCurrentUserFunction::~UsersPrivateGetCurrentUserFunction() {}

ExtensionFunction::ResponseAction UsersPrivateGetCurrentUserFunction::Run() {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(
          chrome_details_.GetProfile());
  return user ? RespondNow(OneArgument(
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

  auto result = std::make_unique<base::DictionaryValue>();
  result->SetKey("isLoggedIn", base::Value(is_logged_in));
  result->SetKey("isScreenLocked", base::Value(is_screen_locked));
  return RespondNow(OneArgument(std::move(result)));
}

}  // namespace extensions
