// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/active_directory_login_screen.h"

#include "base/check_op.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/signin_fatal_error_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/signin_ui.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/active_directory_login_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enrollment_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/user_manager/known_user.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

constexpr char kUserActionCancel[] = "cancel";

SigninError GetSigninError(authpolicy::ErrorType error) {
  switch (error) {
    case authpolicy::ERROR_NETWORK_PROBLEM:
      return SigninError::kActiveDirectoryNetworkProblem;
    case authpolicy::ERROR_KDC_DOES_NOT_SUPPORT_ENCRYPTION_TYPE:
      return SigninError::kActiveDirectoryNotSupportedEncryption;
    default:
      DLOG(WARNING) << "Unhandled error code: " << error;
      return SigninError::kActiveDirectoryUnknownError;
  }
}

}  // namespace

ActiveDirectoryLoginScreen::ActiveDirectoryLoginScreen(
    base::WeakPtr<ActiveDirectoryLoginView> view,
    ErrorScreen* error_screen,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(ActiveDirectoryLoginView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      authpolicy_login_helper_(std::make_unique<AuthPolicyHelper>()),
      view_(std::move(view)),
      error_screen_(error_screen),
      exit_callback_(exit_callback) {
  network_state_informer_ = base::MakeRefCounted<NetworkStateInformer>();
  network_state_informer_->Init();
}

ActiveDirectoryLoginScreen::~ActiveDirectoryLoginScreen() = default;

void ActiveDirectoryLoginScreen::ShowImpl() {
  if (!view_)
    return;
  scoped_observation_.Observe(network_state_informer_.get());
  UpdateState(NetworkError::ERROR_REASON_UPDATE);
  if (!error_screen_visible_)
    view_->Show();
}

void ActiveDirectoryLoginScreen::HideImpl() {
  scoped_observation_.Reset();
  if (view_)
    view_->Reset();
  authpolicy_login_helper_->CancelRequestsAndRestart();
  error_screen_visible_ = false;
  error_screen_->SetParentScreen(OOBE_SCREEN_UNKNOWN);
  error_screen_->Hide();
}

void ActiveDirectoryLoginScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionCancel) {
    HandleCancel();
    return;
  }
  if (action_id == "completeAdAuthentication") {
    CHECK_EQ(args.size(), 3u);
    const std::string& username = args[1].GetString();
    const std::string& password = args[2].GetString();
    HandleCompleteAuth(username, password);
    return;
  }
  BaseScreen::OnUserAction(args);
}

bool ActiveDirectoryLoginScreen::HandleAccelerator(
    LoginAcceleratorAction action) {
  if (action == LoginAcceleratorAction::kCancelScreenAction) {
    HandleCancel();
    return true;
  }
  return false;
}

void ActiveDirectoryLoginScreen::HandleCancel() {
  if (view_)
    view_->Reset();
  authpolicy_login_helper_->CancelRequestsAndRestart();
  if (LoginDisplayHost::default_host()->HasUserPods()) {
    exit_callback_.Run();
  }
}

void ActiveDirectoryLoginScreen::HandleCompleteAuth(
    const std::string& username,
    const std::string& password) {
  if (LoginDisplayHost::default_host())
    LoginDisplayHost::default_host()->SetDisplayEmail(username);

  DCHECK(authpolicy_login_helper_);
  Key key(password);
  key.SetLabel(kCryptohomeGaiaKeyLabel);
  authpolicy_login_helper_->AuthenticateUser(
      username, std::string() /* object_guid */, password,
      base::BindOnce(&ActiveDirectoryLoginScreen::OnAdAuthResult,
                     weak_factory_.GetWeakPtr(), username, key));
}

void ActiveDirectoryLoginScreen::OnAdAuthResult(
    const std::string& username,
    const Key& key,
    authpolicy::ErrorType error,
    const authpolicy::ActiveDirectoryAccountInfo& account_info) {
  if (error != authpolicy::ERROR_NONE)
    authpolicy_login_helper_->CancelRequestsAndRestart();

  switch (error) {
    case authpolicy::ERROR_NONE: {
      DCHECK(account_info.has_account_id() &&
             !account_info.account_id().empty() &&
             LoginDisplayHost::default_host());
      user_manager::KnownUser known_user(g_browser_process->local_state());
      const AccountId account_id(known_user.GetAccountId(
          username, account_info.account_id(), AccountType::ACTIVE_DIRECTORY));
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
    case authpolicy::ERROR_PASSWORD_EXPIRED:
      LoginDisplayHost::default_host()
          ->GetWizardController()
          ->ShowActiveDirectoryPasswordChangeScreen(username);
      break;
    case authpolicy::ERROR_PARSE_UPN_FAILED:
    case authpolicy::ERROR_BAD_USER_NAME:
      if (view_)
        view_->SetErrorState(
            username,
            static_cast<int>(ActiveDirectoryErrorState::BAD_USERNAME));
      break;
    case authpolicy::ERROR_BAD_PASSWORD:
      if (view_)
        view_->SetErrorState(
            username,
            static_cast<int>(ActiveDirectoryErrorState::BAD_AUTH_PASSWORD));
      break;
    default:
      if (view_)
        view_->SetErrorState(username,
                             static_cast<int>(ActiveDirectoryErrorState::NONE));
      LoginDisplayHost::default_host()->GetSigninUI()->ShowSigninError(
          GetSigninError(error), /*details=*/std::string());
  }
}

void ActiveDirectoryLoginScreen::UpdateState(NetworkError::ErrorReason reason) {
  NetworkStateInformer::State state = network_state_informer_->state();
  const bool is_online = NetworkStateInformer::IsOnline(state, reason);
  if (!is_online) {
    error_screen_visible_ = true;
    error_screen_->SetParentScreen(ActiveDirectoryLoginView::kScreenId);
    error_screen_->ShowNetworkErrorMessage(state, reason);
  } else {
    error_screen_->HideCaptivePortal();
    if (error_screen_visible_ && error_screen_->GetParentScreen() ==
                                     ActiveDirectoryLoginView::kScreenId) {
      error_screen_visible_ = false;
      error_screen_->SetParentScreen(OOBE_SCREEN_UNKNOWN);
      error_screen_->Hide();
      if (view_)
        view_->Show();
    }
  }
}

}  // namespace ash
