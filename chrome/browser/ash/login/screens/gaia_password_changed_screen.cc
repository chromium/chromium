// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/gaia_password_changed_screen.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_password_changed_screen_handler.h"

namespace chromeos {

namespace {
constexpr const char kUserActionCancelLogin[] = "cancel";
constexpr const char kUserActionResyncData[] = "resync";

}  // namespace

void RecordEulaScreenAction(GaiaPasswordChangedScreen::UserAction value) {
  base::UmaHistogramEnumeration("OOBE.GaiaPasswordChangedScreen.UserActions",
                                value);
}

GaiaPasswordChangedScreen::GaiaPasswordChangedScreen(
    const ScreenExitCallback& exit_callback,
    GaiaPasswordChangedView* view)
    : BaseScreen(GaiaPasswordChangedView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      exit_callback_(exit_callback) {
  view_ = view;
  if (view_)
    view_->Bind(this);
}

GaiaPasswordChangedScreen::~GaiaPasswordChangedScreen() {
  if (view_)
    view_->Unbind();
}

void GaiaPasswordChangedScreen::OnViewDestroyed(GaiaPasswordChangedView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void GaiaPasswordChangedScreen::ShowImpl() {
  DCHECK(account_id_.is_valid());
  if (view_)
    view_->Show(account_id_.GetUserEmail(), show_error_);
}

void GaiaPasswordChangedScreen::HideImpl() {
  account_id_.clear();
  show_error_ = false;
}

void GaiaPasswordChangedScreen::Configure(const AccountId& account_id,
                                          bool after_incorrect_attempt) {
  DCHECK(account_id.is_valid());
  account_id_ = account_id;
  show_error_ = after_incorrect_attempt;
  if (after_incorrect_attempt)
    RecordEulaScreenAction(UserAction::kIncorrectOldPassword);
}

void GaiaPasswordChangedScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionCancelLogin) {
    RecordEulaScreenAction(UserAction::kCancel);
    CancelPasswordChangedFlow();
  } else if (action_id == kUserActionResyncData) {
    RecordEulaScreenAction(UserAction::kResyncUserData);
    // LDH will pass control to ExistingUserController to proceed with clearing
    // cryptohome.
    exit_callback_.Run(Result::RESYNC);
  }
}

void GaiaPasswordChangedScreen::MigrateUserData(
    const std::string& old_password) {
  RecordEulaScreenAction(UserAction::kMigrateUserData);
  // LDH will pass control to ExistingUserController to proceed with updating
  // cryptohome keys.
  if (LoginDisplayHost::default_host())
    LoginDisplayHost::default_host()->MigrateUserData(old_password);
}

void GaiaPasswordChangedScreen::CancelPasswordChangedFlow() {
  if (account_id_.is_valid()) {
    RecordReauthReason(account_id_, ReauthReason::PASSWORD_UPDATE_SKIPPED);
  }
  ProfileHelper* profile_helper = ProfileHelper::Get();
  profile_helper->ClearSigninProfile(
      base::BindOnce(&GaiaPasswordChangedScreen::OnCookiesCleared,
                     weak_factory_.GetWeakPtr()));
}

void GaiaPasswordChangedScreen::OnCookiesCleared() {
  exit_callback_.Run(Result::CANCEL);
}

}  // namespace chromeos
