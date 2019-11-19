// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/app_launch_signin_screen.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/screens/user_selection_screen.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/login/auth/user_context.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

user_manager::UserManager* AppLaunchSigninScreen::test_user_manager_ = NULL;

AppLaunchSigninScreen::AppLaunchSigninScreen(OobeUI* oobe_ui,
                                             Delegate* delegate)
    : oobe_ui_(oobe_ui), delegate_(delegate), webui_handler_(NULL) {}

AppLaunchSigninScreen::~AppLaunchSigninScreen() {
  oobe_ui_->ResetSigninScreenHandlerDelegate();
}

void AppLaunchSigninScreen::Show() {
  InitOwnerUserList();
  oobe_ui_->web_ui()->CallJavascriptFunctionUnsafe(
      "login.AccountPickerScreen.setShouldShowApps", base::Value(false));
  oobe_ui_->ShowSigninScreen(LoginScreenContext(), this, NULL);
}

void AppLaunchSigninScreen::InitOwnerUserList() {
  user_manager::UserManager* user_manager = GetUserManager();
  const std::string& owner_email =
      user_manager->GetOwnerAccountId().GetUserEmail();
  const user_manager::UserList& all_users = user_manager->GetUsers();

  owner_user_list_.clear();
  for (user_manager::UserList::const_iterator it = all_users.begin();
       it != all_users.end(); ++it) {
    user_manager::User* user = *it;
    if (user->GetAccountId().GetUserEmail() == owner_email) {
      owner_user_list_.push_back(user);
      break;
    }
  }
}

// static
void AppLaunchSigninScreen::SetUserManagerForTesting(
    user_manager::UserManager* user_manager) {
  test_user_manager_ = user_manager;
}

user_manager::UserManager* AppLaunchSigninScreen::GetUserManager() {
  return test_user_manager_ ? test_user_manager_
                            : user_manager::UserManager::Get();
}

void AppLaunchSigninScreen::CancelUserAdding() {
  NOTREACHED();
}

void AppLaunchSigninScreen::Login(const UserContext& user_context,
                                  const SigninSpecifics& specifics) {
  // Note: CreateAuthenticator doesn't necessarily create
  // a new Authenticator object, and could reuse an existing one.
  authenticator_ = UserSessionManager::GetInstance()->CreateAuthenticator(this);
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&Authenticator::AuthenticateToUnlock,
                                authenticator_.get(), user_context));
}

void AppLaunchSigninScreen::OnSigninScreenReady() {}

void AppLaunchSigninScreen::RemoveUser(const AccountId& account_id) {
  NOTREACHED();
}

void AppLaunchSigninScreen::ShowEnterpriseEnrollmentScreen() {
  NOTREACHED();
}

void AppLaunchSigninScreen::ShowEnableDebuggingScreen() {
  NOTREACHED();
}

void AppLaunchSigninScreen::ShowKioskEnableScreen() {
  NOTREACHED();
}

void AppLaunchSigninScreen::ShowKioskAutolaunchScreen() {
  NOTREACHED();
}

void AppLaunchSigninScreen::ShowUpdateRequiredScreen() {
  NOTREACHED();
}

void AppLaunchSigninScreen::ShowWrongHWIDScreen() {
  NOTREACHED();
}

void AppLaunchSigninScreen::SetWebUIHandler(
    LoginDisplayWebUIHandler* webui_handler) {
  webui_handler_ = webui_handler;
}

const user_manager::UserList& AppLaunchSigninScreen::GetUsers() const {
  if (test_user_manager_) {
    return test_user_manager_->GetUsers();
  }
  return owner_user_list_;
}

bool AppLaunchSigninScreen::IsShowGuest() const {
  return false;
}

bool AppLaunchSigninScreen::IsShowUsers() const {
  return true;
}

bool AppLaunchSigninScreen::ShowUsersHasChanged() const {
  return false;
}

bool AppLaunchSigninScreen::IsAllowNewUser() const {
  return true;
}

bool AppLaunchSigninScreen::AllowNewUserChanged() const {
  return false;
}

bool AppLaunchSigninScreen::IsSigninInProgress() const {
  // Return true to suppress network processing in the signin screen.
  return true;
}

bool AppLaunchSigninScreen::IsUserSigninCompleted() const {
  return false;
}

void AppLaunchSigninScreen::Signout() {
  NOTREACHED();
}

void AppLaunchSigninScreen::OnAuthFailure(const AuthFailure& error) {
  LOG(ERROR) << "Unlock failure: " << error.reason();
  webui_handler_->ClearAndEnablePassword();
  webui_handler_->ShowError(
      0, l10n_util::GetStringUTF8(IDS_LOGIN_ERROR_AUTHENTICATING_KIOSK),
      std::string(), HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
}

void AppLaunchSigninScreen::OnAuthSuccess(const UserContext& user_context) {
  delegate_->OnOwnerSigninSuccess();
}

void AppLaunchSigninScreen::HandleGetUsers() {
  base::ListValue users_list;
  const user_manager::UserList& users = GetUsers();

  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end(); ++it) {
    proximity_auth::mojom::AuthType initial_auth_type =
        UserSelectionScreen::ShouldForceOnlineSignIn(*it)
            ? proximity_auth::mojom::AuthType::ONLINE_SIGN_IN
            : proximity_auth::mojom::AuthType::OFFLINE_PASSWORD;
    auto user_dict = std::make_unique<base::DictionaryValue>();
    UserSelectionScreen::FillUserDictionary(
        *it, true,               /* is_owner */
        false,                   /* is_signin_to_add */
        initial_auth_type, NULL, /* public_session_recommended_locales */
        user_dict.get());
    users_list.Append(std::move(user_dict));
  }

  webui_handler_->LoadUsers(users, users_list);
}

void AppLaunchSigninScreen::CheckUserStatus(const AccountId& account_id) {}

}  // namespace chromeos
