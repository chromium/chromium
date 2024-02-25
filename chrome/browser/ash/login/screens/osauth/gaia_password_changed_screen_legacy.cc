// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/gaia_password_changed_screen_legacy.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/profiles/signin_profile_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_password_changed_screen_handler.h"

namespace ash {
namespace {

constexpr const char kUserActionCancelLogin[] = "cancel";
constexpr const char kUserActionResyncData[] = "resync";
constexpr const char kUserActionMigrateUserData[] = "migrate-user-data";

void RecordScreenAction(GaiaPasswordChangedScreenLegacy::UserAction value) {
  base::UmaHistogramEnumeration("OOBE.GaiaPasswordChangedScreen.UserActions",
                                value);
}

}  // namespace

// static
std::string GaiaPasswordChangedScreenLegacy::GetResultString(Result result) {
  switch (result) {
    case Result::CANCEL:
      return "Cancel";
    case Result::MIGRATE:
      return "Migrate";
    case Result::RESYNC:
      return "Resync";
  }
}

GaiaPasswordChangedScreenLegacy::GaiaPasswordChangedScreenLegacy(
    const ScreenExitCallback& exit_callback,
    base::WeakPtr<GaiaPasswordChangedView> view)
    : BaseScreen(GaiaPasswordChangedView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

GaiaPasswordChangedScreenLegacy::~GaiaPasswordChangedScreenLegacy() = default;

void GaiaPasswordChangedScreenLegacy::ShowImpl() {
  DCHECK(account_id_.is_valid());
  if (view_) {
    view_->Show(account_id_.GetUserEmail(), show_error_);
  }
}

void GaiaPasswordChangedScreenLegacy::HideImpl() {
  account_id_.clear();
  show_error_ = false;
}

void GaiaPasswordChangedScreenLegacy::Configure(const AccountId& account_id,
                                                bool after_incorrect_attempt) {
  DCHECK(account_id.is_valid());
  account_id_ = account_id;
  show_error_ = after_incorrect_attempt;
  if (after_incorrect_attempt) {
    RecordScreenAction(UserAction::kIncorrectOldPassword);
  }
}

void GaiaPasswordChangedScreenLegacy::OnUserAction(
    const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();

  if (action_id == kUserActionCancelLogin) {
    RecordScreenAction(UserAction::kCancel);
    CancelPasswordChangedFlow();
  } else if (action_id == kUserActionResyncData) {
    RecordScreenAction(UserAction::kResyncUserData);
    // LDH will pass control to ExistingUserController to proceed with clearing
    // cryptohome.
    exit_callback_.Run(Result::RESYNC);
  } else if (action_id == kUserActionMigrateUserData) {
    CHECK_EQ(args.size(), 2u);
    const std::string& old_password = args[1].GetString();
    MigrateUserData(old_password);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void GaiaPasswordChangedScreenLegacy::MigrateUserData(
    const std::string& old_password) {
  RecordScreenAction(UserAction::kMigrateUserData);
  // LDH will pass control to ExistingUserController to proceed with updating
  // cryptohome keys.
  LoginDisplayHost::default_host()->MigrateUserData(old_password);
}

void GaiaPasswordChangedScreenLegacy::CancelPasswordChangedFlow() {
  if (account_id_.is_valid()) {
    RecordReauthReason(account_id_, ReauthReason::kPasswordUpdateSkipped);
  }
  SigninProfileHandler::Get()->ClearSigninProfile(
      base::BindOnce(&GaiaPasswordChangedScreenLegacy::OnCookiesCleared,
                     weak_factory_.GetWeakPtr()));
}

void GaiaPasswordChangedScreenLegacy::OnCookiesCleared() {
  exit_callback_.Run(Result::CANCEL);
}

}  // namespace ash
