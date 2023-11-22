// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/gaia_password_changed_screen.h"

#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/profiles/signin_profile_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_password_changed_screen_handler.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/mount_performer.h"
#include "components/device_event_log/device_event_log.h"
#include "components/user_manager/user_manager.h"

// TODO(b/274018437): Remove after figuring out the root cause of the bug
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {
namespace {

constexpr const char kUserActionCancelLogin[] = "cancel";
constexpr const char kUserActionResyncData[] = "resync";
constexpr const char kUserActionMigrateUserData[] = "migrate-user-data";
constexpr const char kUserActionSetupRecovery[] = "setup-recovery";
constexpr const char kUserActionDontSetupRecovery[] = "no-recovery";

void RecordScreenAction(GaiaPasswordChangedScreen::UserAction value) {
  base::UmaHistogramEnumeration("OOBE.GaiaPasswordChangedScreen.UserActions",
                                value);
}

}  // namespace

// static
std::string GaiaPasswordChangedScreen::GetResultString(Result result) {
  switch (result) {
    case Result::CANCEL:
      return "Cancel";
    case Result::CONTINUE_LOGIN:
      return "ContinueLogin";
    case Result::RECREATE_USER:
      return "RecreateUser";
    case Result::CRYPTOHOME_ERROR:
      return "CryptohomeError";
  }
}

GaiaPasswordChangedScreen::GaiaPasswordChangedScreen(
    const ScreenExitCallback& exit_callback,
    base::WeakPtr<GaiaPasswordChangedView> view)
    : BaseScreen(GaiaPasswordChangedView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

GaiaPasswordChangedScreen::~GaiaPasswordChangedScreen() = default;

void GaiaPasswordChangedScreen::ShowImpl() {
  VLOG(1) << "GaiaPasswordChangedScreen::ShowImpl";
  DCHECK(context()->user_context);
  if (view_)
    view_->Show(context()->user_context->GetAccountId().GetUserEmail());
  auth_performer_ = std::make_unique<AuthPerformer>(UserDataAuthClient::Get());
  factor_editor_ =
      std::make_unique<AuthFactorEditor>(UserDataAuthClient::Get());
  mount_performer_ = std::make_unique<MountPerformer>();
  // Store password obtained during online authentication.
  // It will be either used to replace old password or
  // be restored as main key in re-creating user flow.
  DCHECK(!context()->user_context->HasReplacementKey());
  context()->user_context->SaveKeyForReplacement();
}

void GaiaPasswordChangedScreen::HideImpl() {
  VLOG(1) << "GaiaPasswordChangedScreen::HideImpl";
  DCHECK(!auth_performer_);
  DCHECK(!factor_editor_);
  DCHECK(!mount_performer_);
}

void GaiaPasswordChangedScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  VLOG(1) << "GaiaPasswordChangedScreen::OnUserAction "
          << "action_id= " << action_id;
  if (action_id == kUserActionCancelLogin) {
    RecordScreenAction(UserAction::kCancel);
    CancelPasswordChangedFlow();
  } else if (action_id == kUserActionResyncData) {
    RecreateUser();
  } else if (action_id == kUserActionSetupRecovery) {
    // TODO(b/257225574): Currently setting up recovery requires profile.
    // Actually add recovery when we migrate from quick_unlock as
    // UserContext storage.
    LOG(FATAL) << "Setting up recovery is not implemented yet";
  } else if (action_id == kUserActionDontSetupRecovery) {
    // Auth session is authenticated, password is updated, just exit screen.
    FinishWithResult(Result::CONTINUE_LOGIN);
  } else if (action_id == kUserActionMigrateUserData) {
    CHECK_EQ(args.size(), 2u);
    const std::string& old_password = args[1].GetString();
    AttemptAuthentication(old_password);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void GaiaPasswordChangedScreen::AttemptAuthentication(
    const std::string& old_password) {
  VLOG(1) << "GaiaPasswordChangedScreen::AttemptAuthentication "
          << "auth_performer_= " << auth_performer_.get();
  RecordScreenAction(UserAction::kMigrateUserData);
  DCHECK(!context()->user_context->GetAuthSessionId().empty());
  auto* factor =
      context()->user_context->GetAuthFactorsConfiguration().FindFactorByType(
          cryptohome::AuthFactorType::kPassword);
  DCHECK(factor);
  auth_performer_->AuthenticateWithPassword(
      factor->ref().label().value(), old_password,
      std::move(context()->user_context),
      base::BindOnce(&GaiaPasswordChangedScreen::OnPasswordAuthentication,
                     weak_factory_.GetWeakPtr()));
}

void GaiaPasswordChangedScreen::OnPasswordAuthentication(
    std::unique_ptr<UserContext> user_context,
    absl::optional<AuthenticationError> error) {
  context()->user_context = std::move(user_context);
  if (error.has_value()) {
    if (error->get_cryptohome_code() ==
        user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED) {
      RecordScreenAction(UserAction::kIncorrectOldPassword);
      view_->ShowWrongPasswordError();
      return;
    }
    // TODO(b/239420684): Send an error to the UI.
    FinishWithResult(Result::CRYPTOHOME_ERROR);
    return;
  }

  // Auth session is authenticated, update password to one stored in context.
  factor_editor_->ReplaceContextKey(
      std::move(context()->user_context),
      base::BindOnce(&GaiaPasswordChangedScreen::OnPasswordUpdated,
                     weak_factory_.GetWeakPtr()));
}

void GaiaPasswordChangedScreen::OnPasswordUpdated(
    std::unique_ptr<UserContext> user_context,
    absl::optional<AuthenticationError> error) {
  context()->user_context = std::move(user_context);
  if (error.has_value()) {
    // TODO(b/239420684): Send an error to the UI.
    FinishWithResult(Result::CRYPTOHOME_ERROR);
    return;
  }
  // We currently need to use this to determine Recovery factor eligibility.
  factor_editor_->GetAuthFactorsConfiguration(
      std::move(context()->user_context),
      base::BindOnce(&GaiaPasswordChangedScreen::OnGetConfiguration,
                     weak_factory_.GetWeakPtr()));
}

void GaiaPasswordChangedScreen::OnGetConfiguration(
    std::unique_ptr<UserContext> user_context,
    absl::optional<AuthenticationError> error) {
  context()->user_context = std::move(user_context);
  if (error.has_value()) {
    // TODO(b/239420684): Send an error to the UI.
    FinishWithResult(Result::CRYPTOHOME_ERROR);
    return;
  }

  // TODO(b/257225574): Currently setting up recovery requires profile.
  // Actually add recovery when we migrate from quick_unlock as
  // UserContext storage.
  bool can_set_recovery = false;

  if (can_set_recovery) {
    auto factors_config =
        context()->user_context->GetAuthFactorsConfiguration();
    can_set_recovery &= factors_config.get_supported_factors().Has(
                            cryptohome::AuthFactorType::kRecovery) &&
                        (!factors_config.HasConfiguredFactor(
                            cryptohome::AuthFactorType::kRecovery));
  }

  if (!can_set_recovery) {
    // Auth session is authenticated, password is updated, report the success.
    FinishWithResult(Result::CONTINUE_LOGIN);
    return;
  }
  view_->SuggestRecovery();
}

void GaiaPasswordChangedScreen::RecreateUser() {
  LOGIN_LOG(USER) << "Re-creating user.";
  VLOG(1) << "GaiaPasswordChangedScreen::RecreateUser "
          << "mount_performer_= " << mount_performer_.get();
  RecordScreenAction(UserAction::kResyncUserData);
  mount_performer_->RemoveUserDirectory(
      std::move(context()->user_context),
      base::BindOnce(&GaiaPasswordChangedScreen::OnRemovedUserDirectory,
                     weak_factory_.GetWeakPtr()));
}

void GaiaPasswordChangedScreen::OnRemovedUserDirectory(
    std::unique_ptr<UserContext> user_context,
    absl::optional<AuthenticationError> error) {
  context()->user_context = std::move(user_context);
  if (error.has_value()) {
    LOGIN_LOG(ERROR) << "Failed to remove user home directory";
    // TODO(b/239420684): Send an error to the UI.
    FinishWithResult(Result::CRYPTOHOME_ERROR);
    return;
  }
  // Force user to go through onboarding again, so that they have
  // consistent experience.
  // Do not notify about removal, as we are still inside the login
  // flow. Otherwise, GAIA screen might be shown (if this user was
  // the only user on the device).
  // TODO(b/270040728): Use `RemoveUserFromList` once internal architecture
  // allows better solution.
  user_manager::UserManager::Get()->RemoveUserFromListForRecreation(
      context()->user_context->GetAccountId());
  // Now that user is deleted, reset everything in UserContext
  // related to cryptohome state.
  context()->user_context->ResetAuthSessionIds();
  context()->user_context->ClearAuthFactorsConfiguration();
  // Move online password back so that it can be used as key.
  // See `ShowImpl()` to see where it was stored.
  context()->user_context->ReuseReplacementKey();
  FinishWithResult(Result::RECREATE_USER);
}

void GaiaPasswordChangedScreen::CancelPasswordChangedFlow() {
  if (context()->user_context->GetAccountId().is_valid()) {
    RecordReauthReason(context()->user_context->GetAccountId(),
                       ReauthReason::kPasswordUpdateSkipped);
  }
  SigninProfileHandler::Get()->ClearSigninProfile(
      base::BindOnce(&GaiaPasswordChangedScreen::OnCookiesCleared,
                     weak_factory_.GetWeakPtr()));
}

void GaiaPasswordChangedScreen::OnCookiesCleared() {
  FinishWithResult(Result::CANCEL);
}

void GaiaPasswordChangedScreen::FinishWithResult(Result result) {
  VLOG(1) << "GaiaPasswordChangedScreen::FinishWithResult "
          << "result= " << static_cast<int>(result);
  weak_factory_.InvalidateWeakPtrs();
  auth_performer_.reset();
  factor_editor_.reset();
  mount_performer_.reset();
  exit_callback_.Run(result);
}

}  // namespace ash
