// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/chrome_user_manager.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {
namespace {

std::string FullyCanonicalize(const std::string& email) {
  return gaia::CanonicalizeEmail(gaia::SanitizeEmail(email));
}

}  // namespace

ChromeUserManager::ChromeUserManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : UserManagerBase(std::move(task_runner)) {}

ChromeUserManager::~ChromeUserManager() {}

// static
void ChromeUserManager::RegisterPrefs(PrefRegistrySimple* registry) {
  UserManagerBase::RegisterPrefs(registry);

  registry->RegisterListPref(::prefs::kReportingUsers);
}

bool ChromeUserManager::IsCurrentUserNew() const {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceFirstRunUI))
    return true;

  return UserManagerBase::IsCurrentUserNew();
}

void ChromeUserManager::UpdateLoginState(const user_manager::User* active_user,
                                         const user_manager::User* primary_user,
                                         bool is_current_user_owner) const {
  if (!LoginState::IsInitialized())
    return;  // LoginState may be uninitialized in tests.

  LoginState::LoggedInState logged_in_state;
  LoginState::LoggedInUserType logged_in_user_type;
  if (active_user) {
    logged_in_state = LoginState::LOGGED_IN_ACTIVE;
    logged_in_user_type =
        GetLoggedInUserType(*active_user, is_current_user_owner);
  } else {
    logged_in_state = LoginState::LOGGED_IN_NONE;
    logged_in_user_type = LoginState::LOGGED_IN_USER_NONE;
  }

  if (primary_user) {
    LoginState::Get()->SetLoggedInStateAndPrimaryUser(
        logged_in_state, logged_in_user_type, primary_user->username_hash());
  } else {
    LoginState::Get()->SetLoggedInState(logged_in_state, logged_in_user_type);
  }
}

bool ChromeUserManager::GetPlatformKnownUserId(
    const std::string& user_email,
    AccountId* out_account_id) const {
  if (user_email == user_manager::kStubUserEmail) {
    *out_account_id = user_manager::StubAccountId();
    return true;
  }

  if (user_email == user_manager::kStubAdUserEmail) {
    *out_account_id = user_manager::StubAdAccountId();
    return true;
  }

  if (user_email == user_manager::kGuestUserName) {
    *out_account_id = user_manager::GuestAccountId();
    return true;
  }

  return false;
}

LoginState::LoggedInUserType ChromeUserManager::GetLoggedInUserType(
    const user_manager::User& active_user,
    bool is_current_user_owner) const {
  if (is_current_user_owner)
    return LoginState::LOGGED_IN_USER_OWNER;

  switch (active_user.GetType()) {
    case user_manager::USER_TYPE_REGULAR:
      return LoginState::LOGGED_IN_USER_REGULAR;
    case user_manager::USER_TYPE_GUEST:
      return LoginState::LOGGED_IN_USER_GUEST;
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
      return LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT;
    case user_manager::USER_TYPE_KIOSK_APP:
      return LoginState::LOGGED_IN_USER_KIOSK;
    case user_manager::USER_TYPE_CHILD:
      return LoginState::LOGGED_IN_USER_CHILD;
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
      return LoginState::LOGGED_IN_USER_KIOSK;
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
      // NOTE(olsen) There's no LOGGED_IN_USER_ACTIVE_DIRECTORY - is it needed?
      return LoginState::LOGGED_IN_USER_REGULAR;
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
      return LoginState::LOGGED_IN_USER_KIOSK;
    case user_manager::NUM_USER_TYPES:
      break;  // Go to invalid-type handling code.
      // Since there is no default, the compiler warns about unhandled types.
  }
  NOTREACHED() << "Invalid type for active user: " << active_user.GetType();
  return LoginState::LOGGED_IN_USER_REGULAR;
}

// static
ChromeUserManager* ChromeUserManager::Get() {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  return user_manager ? static_cast<ChromeUserManager*>(user_manager) : nullptr;
}

bool ChromeUserManager::ShouldReportUser(const std::string& user_id) const {
  const base::Value::List& reporting_users =
      GetLocalState()->GetList(::prefs::kReportingUsers);
  base::Value user_id_value(FullyCanonicalize(user_id));
  return base::Contains(reporting_users, user_id_value);
}

void ChromeUserManager::AddReportingUser(const AccountId& account_id) {
  ScopedListPrefUpdate users_update(GetLocalState(), ::prefs::kReportingUsers);
  base::Value email_value(account_id.GetUserEmail());
  if (!base::Contains(users_update.Get(), email_value)) {
    users_update->Append(std::move(email_value));
  }
}

void ChromeUserManager::RemoveReportingUser(const AccountId& account_id) {
  ScopedListPrefUpdate users_update(GetLocalState(), ::prefs::kReportingUsers);
  base::Value::List& update_list = users_update.Get();
  auto it = base::ranges::find(
      update_list, base::Value(FullyCanonicalize(account_id.GetUserEmail())));
  if (it == update_list.end()) {
    return;
  }
  update_list.erase(it);
}

}  // namespace ash
