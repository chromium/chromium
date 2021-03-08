// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/active_directory_login_screen.h"

#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/active_directory_login_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enrollment_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/login/auth/cryptohome_key_constants.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/user_manager/known_user.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr char kUserActionCancel[] = "cancel";

std::string GetErrorMessage(authpolicy::ErrorType error) {
  switch (error) {
    case authpolicy::ERROR_NETWORK_PROBLEM:
      return l10n_util::GetStringUTF8(IDS_AD_AUTH_NETWORK_ERROR);
    case authpolicy::ERROR_KDC_DOES_NOT_SUPPORT_ENCRYPTION_TYPE:
      return l10n_util::GetStringUTF8(IDS_AD_AUTH_NOT_SUPPORTED_ENCRYPTION);
    default:
      DLOG(WARNING) << "Unhandled error code: " << error;
      return l10n_util::GetStringUTF8(IDS_AD_AUTH_UNKNOWN_ERROR);
  }
}

}  // namespace

namespace chromeos {

ActiveDirectoryLoginScreen::ActiveDirectoryLoginScreen(
    ActiveDirectoryLoginView* view,
    ErrorScreen* error_screen,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(ActiveDirectoryLoginView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      authpolicy_login_helper_(std::make_unique<AuthPolicyHelper>()),
      view_(view),
      error_screen_(error_screen),
      exit_callback_(exit_callback) {
  network_state_informer_ = base::MakeRefCounted<NetworkStateInformer>();
  network_state_informer_->Init();
  if (view_)
    view_->Bind(this);
}

ActiveDirectoryLoginScreen::~ActiveDirectoryLoginScreen() {
  if (view_)
    view_->Unbind();
}

void ActiveDirectoryLoginScreen::OnViewDestroyed(
    ActiveDirectoryLoginView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void ActiveDirectoryLoginScreen::ShowImpl() {
  if (!view_)
    return;
  scoped_observer_ = std::make_unique<
      ScopedObserver<NetworkStateInformer, NetworkStateInformerObserver>>(this);
  scoped_observer_->Add(network_state_informer_.get());
  UpdateState(NetworkError::ERROR_REASON_UPDATE);
  if (!error_screen_visible_)
    view_->Show();
}

void ActiveDirectoryLoginScreen::HideImpl() {
  scoped_observer_.reset();
  view_->Reset();
  authpolicy_login_helper_->CancelRequestsAndRestart();
  error_screen_visible_ = false;
  error_screen_->SetParentScreen(OobeScreen::SCREEN_UNKNOWN);
  error_screen_->Hide();
}

void ActiveDirectoryLoginScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionCancel) {
    HandleCancel();
    return;
  }
  BaseScreen::OnUserAction(action_id);
}

bool ActiveDirectoryLoginScreen::HandleAccelerator(
    ash::LoginAcceleratorAction action) {
  if (action == ash::LoginAcceleratorAction::kCancelScreenAction) {
    HandleCancel();
    return true;
  }
  return false;
}

void ActiveDirectoryLoginScreen::HandleCancel() {
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
      const AccountId account_id(user_manager::known_user::GetAccountId(
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
      view_->SetErrorState(
          username, static_cast<int>(ActiveDirectoryErrorState::BAD_USERNAME));
      break;
    case authpolicy::ERROR_BAD_PASSWORD:
      view_->SetErrorState(
          username,
          static_cast<int>(ActiveDirectoryErrorState::BAD_AUTH_PASSWORD));
      break;
    default:
      view_->SetErrorState(username,
                           static_cast<int>(ActiveDirectoryErrorState::NONE));
      view_->ShowSignInError(GetErrorMessage(error));
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
      error_screen_->SetParentScreen(OobeScreen::SCREEN_UNKNOWN);
      error_screen_->Hide();
      view_->Show();
    }
  }
}

}  // namespace chromeos
