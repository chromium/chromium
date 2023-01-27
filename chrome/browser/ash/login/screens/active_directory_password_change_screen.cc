// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/active_directory_password_change_screen.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/active_directory_password_change_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "components/user_manager/known_user.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
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

ActiveDirectoryPasswordChangeScreen::ActiveDirectoryPasswordChangeScreen(
    base::WeakPtr<TView> view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(TView::kScreenId, OobeScreenPriority::DEFAULT),
      authpolicy_login_helper_(std::make_unique<AuthPolicyHelper>()),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

ActiveDirectoryPasswordChangeScreen::~ActiveDirectoryPasswordChangeScreen() =
    default;

void ActiveDirectoryPasswordChangeScreen::SetUsername(
    const std::string& username) {
  username_ = username;
}

void ActiveDirectoryPasswordChangeScreen::ShowImpl() {
  if (!view_)
    return;
  view_->Show(
      username_,
      static_cast<int>(ActiveDirectoryPasswordChangeErrorState::NO_ERROR));
}

void ActiveDirectoryPasswordChangeScreen::HideImpl() {
  username_.clear();
}

void ActiveDirectoryPasswordChangeScreen::OnUserAction(
    const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionCancel) {
    HandleCancel();
    return;
  }
  if (action_id == "changePassword") {
    CHECK_EQ(3u, args.size());
    const std::string& old_password = args[1].GetString();
    const std::string& new_password = args[2].GetString();
    HandleChangePassword(old_password, new_password);
    return;
  }
  BaseScreen::OnUserAction(args);
}

void ActiveDirectoryPasswordChangeScreen::HandleCancel() {
  authpolicy_login_helper_->CancelRequestsAndRestart();
  exit_callback_.Run();
}

void ActiveDirectoryPasswordChangeScreen::HandleChangePassword(
    const std::string& old_password,
    const std::string& new_password) {
  DCHECK(!old_password.empty() && !new_password.empty())
      << "Empty passwords should have been blocked in the UI";

  // The Cryptohome key label is required when changing the password of an
  // ephemeral user or a new user. Without this label, the login will fail
  // with a Cryptohome mount error. For historical reasons, Active Directory
  // users have the same label as GAIA users.
  Key key(new_password);
  key.SetLabel(kCryptohomeGaiaKeyLabel);

  DCHECK(authpolicy_login_helper_);
  authpolicy_login_helper_->AuthenticateUser(
      username_, std::string() /* object_guid */,
      old_password + "\n" + new_password + "\n" + new_password,
      base::BindOnce(&ActiveDirectoryPasswordChangeScreen::OnAuthFinished,
                     weak_factory_.GetWeakPtr(), username_, key));
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
      user_manager::KnownUser known_user(g_browser_process->local_state());
      const AccountId account_id = known_user.GetAccountId(
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
      if (view_) {
        view_->Show(
            username_,
            static_cast<int>(
                ActiveDirectoryPasswordChangeErrorState::WRONG_OLD_PASSWORD));
      }
      break;
    case authpolicy::ERROR_PASSWORD_REJECTED:
      if (view_) {
        view_->Show(username_,
                    static_cast<int>(ActiveDirectoryPasswordChangeErrorState::
                                         NEW_PASSWORD_REJECTED));
        view_->ShowSignInError(l10n_util::GetStringUTF8(
            IDS_AD_PASSWORD_CHANGE_NEW_PASSWORD_REJECTED_LONG_ERROR));
      }
      break;
    default:
      NOTREACHED() << "Unhandled error: " << error;
      if (view_) {
        view_->Show(username_,
                    static_cast<int>(
                        ActiveDirectoryPasswordChangeErrorState::NO_ERROR));
        view_->ShowSignInError(
            l10n_util::GetStringUTF8(IDS_AD_AUTH_UNKNOWN_ERROR));
      }
  }
}

}  // namespace ash
