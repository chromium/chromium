// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/supervised/supervised_user_login_flow.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_constants.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_webui.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/login/auth/key.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

SupervisedUserLoginFlow::SupervisedUserLoginFlow(const AccountId& account_id)
    : ExtendedUserFlow(account_id) {}

SupervisedUserLoginFlow::~SupervisedUserLoginFlow() {}

void SupervisedUserLoginFlow::AppendAdditionalCommandLineSwitches() {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager->IsCurrentUserNew()) {
    // Supervised users should launch into empty desktop on first run.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kSilentLaunch);
  }
}

bool SupervisedUserLoginFlow::CanLockScreen() {
  return true;
}

bool SupervisedUserLoginFlow::CanStartArc() {
  return false;
}

bool SupervisedUserLoginFlow::ShouldLaunchBrowser() {
  return data_loaded_;
}

bool SupervisedUserLoginFlow::ShouldSkipPostLoginScreens() {
  return true;
}

bool SupervisedUserLoginFlow::SupportsEarlyRestartToApplyFlags() {
  return false;
}

bool SupervisedUserLoginFlow::HandleLoginFailure(const AuthFailure& failure) {
  return false;
}

void SupervisedUserLoginFlow::OnSyncSetupDataLoaded(const std::string& token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ConfigureSync(token);
}

void SupervisedUserLoginFlow::ConfigureSync(const std::string& token) {
  data_loaded_ = true;

  // TODO(antrim): add error handling (no token loaded).
  // See also: http://crbug.com/312751
  ChromeUserManager::Get()->GetSupervisedUserManager()->ConfigureSyncWithToken(
      profile_, token);
  SupervisedUserAuthentication* auth =
      ChromeUserManager::Get()->GetSupervisedUserManager()->GetAuthentication();

  if (auth->HasScheduledPasswordUpdate(account_id().GetUserEmail())) {
    auth->LoadPasswordUpdateData(
        account_id().GetUserEmail(),
        base::Bind(&SupervisedUserLoginFlow::OnPasswordChangeDataLoaded,
                   weak_factory_.GetWeakPtr()),
        base::Bind(&SupervisedUserLoginFlow::OnPasswordChangeDataLoadFailed,
                   weak_factory_.GetWeakPtr()));
    return;
  }
  Finish();
}

void SupervisedUserLoginFlow::HandleLoginSuccess(
    const UserContext& login_context) {
  context_ = login_context;
}

void SupervisedUserLoginFlow::OnPasswordChangeDataLoaded(
    const base::DictionaryValue* password_data) {
  // Edge case, when manager has signed in and already updated the password.
  SupervisedUserAuthentication* auth =
      ChromeUserManager::Get()->GetSupervisedUserManager()->GetAuthentication();
  if (!auth->NeedPasswordChange(account_id().GetUserEmail(), password_data)) {
    VLOG(1) << "Password already changed for " << account_id().Serialize();
    auth->ClearScheduledPasswordUpdate(account_id().GetUserEmail());
    Finish();
    return;
  }

  // Two cases now - we can currently have either old-style password, or new
  // password.
  std::string base64_signature;
  std::string signature;
  std::string password;
  int revision = 0;
  int schema = 0;
  bool success = password_data->GetStringWithoutPathExpansion(
      kPasswordSignature, &base64_signature);
  success &= password_data->GetIntegerWithoutPathExpansion(kPasswordRevision,
                                                           &revision);
  success &=
      password_data->GetIntegerWithoutPathExpansion(kSchemaVersion, &schema);
  success &= password_data->GetStringWithoutPathExpansion(kEncryptedPassword,
                                                          &password);
  if (!success) {
    LOG(ERROR) << "Incomplete data for password change";

    UMA_HISTOGRAM_ENUMERATION(
        "ManagedUsers.ChromeOS.PasswordChange",
        SupervisedUserAuthentication::PASSWORD_CHANGE_FAILED_INCOMPLETE_DATA,
        SupervisedUserAuthentication::PASSWORD_CHANGE_RESULT_MAX_VALUE);
    Finish();
    return;
  }
  base::Base64Decode(base64_signature, &signature);
  std::unique_ptr<base::DictionaryValue> data_copy(password_data->DeepCopy());
  auto key = cryptohome::KeyDefinition::CreateForPassword(
      password, kCryptohomeSupervisedUserKeyLabel,
      kCryptohomeSupervisedUserKeyPrivileges);

  authenticator_ = ExtendedAuthenticator::Create(this);
  SupervisedUserAuthentication::Schema current_schema =
      auth->GetPasswordSchema(account_id().GetUserEmail());

  key.revision = revision;

  if (SupervisedUserAuthentication::SCHEMA_PLAIN == current_schema) {
    // We need to add new key, and block old one. As we don't actually have
    // signature key, use Migrate privilege instead of AuthorizedUpdate.
    key.privileges = kCryptohomeSupervisedUserIncompleteKeyPrivileges;

    VLOG(1) << "Adding new schema key";
    DCHECK(context_.GetKey()->GetLabel().empty());
    authenticator_->AddKey(
        context_, key, false /* no key exists */,
        base::Bind(&SupervisedUserLoginFlow::OnNewKeyAdded,
                   weak_factory_.GetWeakPtr(), Passed(&data_copy)));
  } else if (SupervisedUserAuthentication::SCHEMA_SALT_HASHED ==
             current_schema) {
    VLOG(1) << "Updating the key";

    if (auth->HasIncompleteKey(account_id().GetUserEmail())) {
      // We need to use Migrate instead of Authorized Update privilege.
      key.privileges = kCryptohomeSupervisedUserIncompleteKeyPrivileges;
    }
    // Just update the key.
    DCHECK_EQ(context_.GetKey()->GetLabel(), kCryptohomeSupervisedUserKeyLabel);
    authenticator_->UpdateKeyAuthorized(
        context_, key, signature,
        base::Bind(&SupervisedUserLoginFlow::OnPasswordUpdated,
                   weak_factory_.GetWeakPtr(), Passed(&data_copy)));
  } else {
    NOTREACHED() << "Unsupported password schema";
  }
}

void SupervisedUserLoginFlow::OnNewKeyAdded(
    std::unique_ptr<base::DictionaryValue> password_data) {
  VLOG(1) << "New key added";
  SupervisedUserAuthentication* auth =
      ChromeUserManager::Get()->GetSupervisedUserManager()->GetAuthentication();
  auth->StorePasswordData(account_id().GetUserEmail(), *password_data.get());
  auth->MarkKeyIncomplete(account_id().GetUserEmail(), true /* incomplete */);
  authenticator_->RemoveKey(
      context_, kLegacyCryptohomeSupervisedUserKeyLabel,
      base::Bind(&SupervisedUserLoginFlow::OnOldKeyRemoved,
                 weak_factory_.GetWeakPtr()));
}

void SupervisedUserLoginFlow::OnOldKeyRemoved() {
  UMA_HISTOGRAM_ENUMERATION(
      "ManagedUsers.ChromeOS.PasswordChange",
      SupervisedUserAuthentication::PASSWORD_CHANGED_IN_USER_SESSION,
      SupervisedUserAuthentication::PASSWORD_CHANGE_RESULT_MAX_VALUE);
  Finish();
}

void SupervisedUserLoginFlow::OnPasswordChangeDataLoadFailed() {
  LOG(ERROR) << "Could not load data for password change";

  UMA_HISTOGRAM_ENUMERATION(
      "ManagedUsers.ChromeOS.PasswordChange",
      SupervisedUserAuthentication::PASSWORD_CHANGE_FAILED_LOADING_DATA,
      SupervisedUserAuthentication::PASSWORD_CHANGE_RESULT_MAX_VALUE);
  Finish();
}

void SupervisedUserLoginFlow::OnAuthenticationFailure(
    ExtendedAuthenticator::AuthState state) {
  LOG(ERROR) << "Authentication error during password change";

  UMA_HISTOGRAM_ENUMERATION(
      "ManagedUsers.ChromeOS.PasswordChange",
      SupervisedUserAuthentication::
          PASSWORD_CHANGE_FAILED_AUTHENTICATION_FAILURE,
      SupervisedUserAuthentication::PASSWORD_CHANGE_RESULT_MAX_VALUE);
  Finish();
}

void SupervisedUserLoginFlow::OnPasswordUpdated(
    std::unique_ptr<base::DictionaryValue> password_data) {
  VLOG(1) << "Updated password for supervised user";

  SupervisedUserAuthentication* auth =
      ChromeUserManager::Get()->GetSupervisedUserManager()->GetAuthentication();

  // Incomplete state is not there in password_data, carry it from old state.
  const bool was_incomplete =
      auth->HasIncompleteKey(account_id().GetUserEmail());
  auth->StorePasswordData(account_id().GetUserEmail(), *password_data.get());
  if (was_incomplete)
    auth->MarkKeyIncomplete(account_id().GetUserEmail(), true /* incomplete */);

  UMA_HISTOGRAM_ENUMERATION(
      "ManagedUsers.ChromeOS.PasswordChange",
      SupervisedUserAuthentication::PASSWORD_CHANGED_IN_USER_SESSION,
      SupervisedUserAuthentication::PASSWORD_CHANGE_RESULT_MAX_VALUE);
  Finish();
}

void SupervisedUserLoginFlow::Finish() {
  UserSessionManager::GetInstance()->DoBrowserLaunch(profile_, host());
  profile_ = NULL;
  UnregisterFlowSoon();
}

void SupervisedUserLoginFlow::LaunchExtraSteps(Profile* profile) {
  profile_ = profile;
  ChromeUserManager::Get()->GetSupervisedUserManager()->LoadSupervisedUserToken(
      profile, base::Bind(&SupervisedUserLoginFlow::OnSyncSetupDataLoaded,
                          weak_factory_.GetWeakPtr()));
}

}  // namespace chromeos
