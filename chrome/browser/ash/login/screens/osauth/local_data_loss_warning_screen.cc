// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/local_data_loss_warning_screen.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/oobe_mojo_binder.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/osauth/local_data_loss_warning_screen_handler.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/login/auth/mount_performer.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/crash/core/app/crashpad.h"
#include "components/device_event_log/device_event_log.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {

bool isOwner(const AccountId& account_id) {
  auto* user = user_manager::UserManager::Get()->FindUser(account_id);

  if (!user) {
    LOG(ERROR) << "Could not find user for owner check";
    crash_reporter::DumpWithoutCrashing();
    return false;
  }

  return user_manager::UserManager::Get()->IsOwnerUser(user);
}

}  // namespace

// static
std::string LocalDataLossWarningScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kRemoveUser:
      return "RemoveUser";
    case Result::kBackToLocalAuth:
      return "Back";
    case Result::kBackToOnlineAuth:
      return "Back";
    case Result::kCryptohomeError:
      return "CryptohomeError";
    case Result::kCancel:
      return "Cancel";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

LocalDataLossWarningScreen::LocalDataLossWarningScreen(
    base::WeakPtr<LocalDataLossWarningScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseOSAuthSetupScreen(LocalDataLossWarningScreenView::kScreenId,
                            OobeScreenPriority::DEFAULT),
      OobeMojoBinder(this),
      view_(std::move(view)),
      exit_callback_(exit_callback),
      mount_performer_(std::make_unique<MountPerformer>()) {}

LocalDataLossWarningScreen::~LocalDataLossWarningScreen() = default;

void LocalDataLossWarningScreen::ShowImpl() {
  bool can_go_back = context()->knowledge_factor_setup.data_loss_back_option !=
                     WizardContext::DataLossBackOptions::kNone;
  view_->Show(isOwner(context()->user_context->GetAccountId()),
              context()->user_context->GetAccountId().GetUserEmail(),
              can_go_back);
}

void LocalDataLossWarningScreen::OnPowerwash() {
  if (is_hidden()) {
    return;
  }
  if (!isOwner(context()->user_context->GetAccountId())) {
    LOG(ERROR) << "Non owner user requesting powerwash, ignoring";
    return;
  }
  SessionManagerClient::Get()->StartDeviceWipe(base::DoNothing());
}

void LocalDataLossWarningScreen::OnRecreateUser() {
  if (is_hidden()) {
    return;
  }
  mount_performer_->RemoveUserDirectory(
      std::move(context()->user_context),
      base::BindOnce(&LocalDataLossWarningScreen::OnRemovedUserDirectory,
                     weak_factory_.GetWeakPtr()));
}

void LocalDataLossWarningScreen::OnCancel() {
  if (is_hidden()) {
    return;
  }
  exit_callback_.Run(Result::kCancel);
}

void LocalDataLossWarningScreen::OnBack() {
  if (is_hidden()) {
    return;
  }
  switch (context()->knowledge_factor_setup.data_loss_back_option) {
    case WizardContext::DataLossBackOptions::kNone:
      NOTREACHED_IN_MIGRATION() << "Back button should not be shown";
      return;
    case WizardContext::DataLossBackOptions::kBackToOnlineAuth:
      exit_callback_.Run(Result::kBackToOnlineAuth);
      return;
    case WizardContext::DataLossBackOptions::kBackToLocalAuth:
      exit_callback_.Run(Result::kBackToLocalAuth);
      return;
  }
}

void LocalDataLossWarningScreen::OnRemovedUserDirectory(
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  context()->user_context = std::move(user_context);
  if (error.has_value()) {
    LOGIN_LOG(ERROR) << "Failed to remove user home directory";
    context()->osauth_error = WizardContext::OSAuthErrorKind::kFatal;
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
  context()->knowledge_factor_setup.auth_setup_flow =
      WizardContext::AuthChangeFlow::kInitialSetup;

  // Move online password back so that it can be used as key.
  // See `ShowImpl()` to see where it was stored.
  if (context()->user_context->HasReplacementKey()) {
    context()->user_context->ReuseReplacementKey();
  }
  exit_callback_.Run(Result::kRemoveUser);
}

}  // namespace ash
