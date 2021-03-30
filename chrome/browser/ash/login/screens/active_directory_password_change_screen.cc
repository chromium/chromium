// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/active_directory_password_change_screen.h"

#include <memory>

#include "base/bind.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/active_directory_password_change_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/login/auth/key.h"
#include "components/user_manager/known_user.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr char kUserActionCancel[] = "cancel";

// Possible error states of the Active Directory password change screen. Must be
// in the same order as ACTIVE_DIRECTORY_PASSWORD_CHANGE_ERROR_STATE enum
// values.
enum class ActiveDirectoryPasswordChangeErrorState {
  NO_ERROR = 0,
  WRONG_OLD_PASSWORD = 1,
  NEW_PASSWORD_REJECTED = 2,
};
}  // namespace

namespace chromeos {

ActiveDirectoryPasswordChangeScreen::ActiveDirectoryPasswordChangeScreen(
    ActiveDirectoryPasswordChangeView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(ActiveDirectoryPasswordChangeView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      authpolicy_login_helper_(std::make_unique<AuthPolicyHelper>()),
      view_(view),
      exit_callback_(exit_callback) {
  if (view_)
    view_->Bind(this);
}

ActiveDirectoryPasswordChangeScreen::~ActiveDirectoryPasswordChangeScreen() {
  if (view_)
    view_->Unbind();
}

void ActiveDirectoryPasswordChangeScreen::OnViewDestroyed(
    ActiveDirectoryPasswordChangeView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void ActiveDirectoryPasswordChangeScreen::SetUsername(
    const std::string& username) {
  username_ = username;
}

void ActiveDirectoryPasswordChangeScreen::ShowImpl() {
  if (view_)
    view_->Show(
        username_,
        static_cast<int>(ActiveDirectoryPasswordChangeErrorState::NO_ERROR));
}

void ActiveDirectoryPasswordChangeScreen::HideImpl() {
  username_.clear();
}

void ActiveDirectoryPasswordChangeScreen::OnUserAction(
    const std::string& action_id) {
  if (action_id == kUserActionCancel) {
    HandleCancel();
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void ActiveDirectoryPasswordChangeScreen::HandleCancel() {
  authpolicy_login_helper_->CancelRequestsAndRestart();
  exit_callback_.Run();
}

void ActiveDirectoryPasswordChangeScreen::ChangePassword(
    const std::string& old_password,
    const std::string& new_password) {
  authpolicy_login_helper_->AuthenticateUser(
      username_, std::string() /* object_guid */,
      old_password + "\n" + new_password + "\n" + new_password,
      base::BindOnce(&ActiveDirectoryPasswordChangeScreen::OnAuthFinished,
                     weak_factory_.GetWeakPtr(), username_, Key(new_password)));
}

void ActiveDirectoryPasswordChangeScreen::OnAuthFinished(
    const std::string& username,
    const Key& key,
    authpolicy::ErrorType error,
    const authpolicy::ActiveDirectoryAccountInfo& account_info) {
  switch (error) {
    case authpolicy::ERROR_NONE: {
      DCHECK(account_info.has_account_id() &&
             !account_info.account_id().empty());
      const AccountId account_id = user_manager::known_user::GetAccountId(
          username, account_info.account_id(), AccountType::ACTIVE_DIRECTORY);
      DCHECK(LoginDisplayHost::default_host());
      LoginDisplayHost::default_host()->SetDisplayAndGivenName(
          account_info.display_name(), account_info.given_name());
      UserContext user_context(
          user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY, account_id);
      user_context.SetKey(key);
      user_context.SetAuthFlow(UserContext::AUTH_FLOW_ACTIVE_DIRECTORY);
      user_context.SetIsUsingOAuth(false);
      LoginDisplayHost::default_host()->CompleteLogin(user_context);
      break;
    }
    case authpolicy::ERROR_BAD_PASSWORD:
      view_->Show(
          username_,
          static_cast<int>(
              ActiveDirectoryPasswordChangeErrorState::WRONG_OLD_PASSWORD));
      break;
    case authpolicy::ERROR_PASSWORD_REJECTED:
      view_->Show(
          username_,
          static_cast<int>(
              ActiveDirectoryPasswordChangeErrorState::NEW_PASSWORD_REJECTED));
      view_->ShowSignInError(l10n_util::GetStringUTF8(
          IDS_AD_PASSWORD_CHANGE_NEW_PASSWORD_REJECTED_LONG_ERROR));
      break;
    default:
      NOTREACHED() << "Unhandled error: " << error;
      view_->Show(
          username_,
          static_cast<int>(ActiveDirectoryPasswordChangeErrorState::NO_ERROR));
      view_->ShowSignInError(
          l10n_util::GetStringUTF8(IDS_AD_AUTH_UNKNOWN_ERROR));
  }
}

}  // namespace chromeos
