// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/local_data_loss_warning_screen.h"

#include "base/logging.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/osauth/local_data_loss_warning_screen_handler.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/device_event_log/device_event_log.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {

constexpr const char kUserActionContinueAnyway[] = "recreateUser";
constexpr const char kUserActionPowerwash[] = "powerwash";
constexpr const char kUserActionBack[] = "back";

}  // namespace

// static
std::string LocalDataLossWarningScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kRemoveUser:
      return "removeUser";
    case Result::kBack:
      return "Back";
    case Result::kCryptohomeError:
      return "CryptohomeError";
  }
}

LocalDataLossWarningScreen::LocalDataLossWarningScreen(
    base::WeakPtr<LocalDataLossWarningScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseOSAuthSetupScreen(LocalDataLossWarningScreenView::kScreenId,
                            OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback),
      mount_performer_(std::make_unique<MountPerformer>()) {}

LocalDataLossWarningScreen::~LocalDataLossWarningScreen() = default;

void LocalDataLossWarningScreen::ShowImpl() {
  view_->Show(context()->user_context->GetAccountId().GetUserEmail());
}

void LocalDataLossWarningScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionContinueAnyway) {
    mount_performer_->RemoveUserDirectory(
        std::move(context()->user_context),
        base::BindOnce(&LocalDataLossWarningScreen::OnRemovedUserDirectory,
                       weak_factory_.GetWeakPtr()));
    return;
  } else if (action_id == kUserActionPowerwash) {
    if (auto* user_manager = user_manager::UserManager::Get();
        !user_manager->IsOwnerUser(
            user_manager->FindUser(context()->user_context->GetAccountId()))) {
      LOG(ERROR) << "Non owner user requesting powerwash, ignoring";
      return;
    }
    SessionManagerClient::Get()->StartDeviceWipe(base::DoNothing());
    return;
  } else if (action_id == kUserActionBack) {
    exit_callback_.Run(Result::kBack);
    return;
  }
  BaseOSAuthSetupScreen::OnUserAction(args);
}

void LocalDataLossWarningScreen::OnRemovedUserDirectory(
    std::unique_ptr<UserContext> user_context,
    absl::optional<AuthenticationError> error) {
  context()->user_context = std::move(user_context);
  if (error.has_value()) {
    LOGIN_LOG(ERROR) << "Failed to remove user home directory";
    // TODO(b/239420684): Send an error to the UI.
    exit_callback_.Run(Result::kCryptohomeError);
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
  exit_callback_.Run(Result::kRemoveUser);
}

}  // namespace ash
