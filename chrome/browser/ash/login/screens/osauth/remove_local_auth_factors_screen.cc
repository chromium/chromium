// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/remove_local_auth_factors_screen.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/ui/webui/ash/login/remove_local_auth_factors_screen_handler.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/auth_factors_configuration.h"
#include "chromeos/ash/components/login/auth/public/auth_types.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom.h"
#include "components/account_id/account_id.h"

namespace ash {
namespace {

constexpr char kUserActionDoneButtonClicked[] = "done";

}  // namespace

// static
std::string RemoveLocalAuthFactorsScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kSuccess:
      return "Next";
    case Result::kError:
      return "Error";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

RemoveLocalAuthFactorsScreen::RemoveLocalAuthFactorsScreen(
    PrefService* local_state,
    base::WeakPtr<RemoveLocalAuthFactorsScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseOSAuthSetupScreen(RemoveLocalAuthFactorsScreenView::kScreenId,
                            OobeScreenPriority::DEFAULT),
      local_state_(CHECK_DEREF(local_state)),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

RemoveLocalAuthFactorsScreen::~RemoveLocalAuthFactorsScreen() = default;

void RemoveLocalAuthFactorsScreen::ShowImpl() {
  if (!view_) {
    return;
  }
  CHECK(context()->user_context)
      << "User context should not be null when removing the local auth factors";
  if (!context()->user_context->GetAccountId().is_valid()) {
    LOG(ERROR) << "Invalid AccountId detected";
  }

  std::string domain = enterprise_util::GetDomainFromEmail(
      context()->user_context->GetAccountId().GetUserEmail());
  if (domain.empty()) {
    LOG(ERROR) << "Unable to resolve a domain name for remove local auth "
                  "factors screen";
  }

  view_->Show(domain);

  GetAuthFactorEditor()->GetAuthFactorsConfiguration(
      std::move(context()->user_context),
      base::BindOnce(
          &RemoveLocalAuthFactorsScreen::OnGetAuthFactorsConfiguration,
          weak_ptr_factory_.GetWeakPtr()));
}

void RemoveLocalAuthFactorsScreen::HideImpl() {}

void RemoveLocalAuthFactorsScreen::OnGetAuthFactorsConfiguration(
    std::unique_ptr<ash::UserContext> user_context,
    std::optional<ash::AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to get auth factors configuration, code "
               << error->get_cryptohome_error();
    context()->user_context = std::move(user_context);
    context()->osauth_error = WizardContext::OSAuthErrorKind::kFatal;
    exit_callback_.Run(Result::kError);
    return;
  }
  LOG(WARNING) << "AuthFactor configuration successully fetched, proceeding "
                  "with local AuthFactor removal.";

  online_password_ = user_context->GetOnlinePassword();
  auth_factors_config_ = user_context->GetAuthFactorsConfiguration();
  // Once we populate the AuthFactorsConfiguration, we should store the
  // `user_context` inside of the `extra_factors_token` as the pin and password
  // factor editor(s) expect it as a token. Keeping it inside the wizard_context
  // initially saves us a withdraw and store call, as this is done
  // asynchronously without user input the token validity should not be
  // affected.
  context()->extra_factors_token =
      AuthSessionStorage::Get()->Store(std::move(user_context));
  KeepAliveAuthSession();
  SetOnlinePasswordAndRemoveLocalAuthFactors();
}

void RemoveLocalAuthFactorsScreen::
    SetOnlinePasswordAndRemoveLocalAuthFactors() {
  auth::mojom::PasswordFactorEditor& password_factor_editor =
      auth::GetPasswordFactorEditor(
          quick_unlock::QuickUnlockFactory::GetDelegate(), &local_state_.get());

  password_factor_editor.UpdateOrSetOnlinePassword(
      GetToken(), online_password_.value().value(),
      base::BindOnce(&RemoveLocalAuthFactorsScreen::RemoveLocalAuthFactors,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RemoveLocalAuthFactorsScreen::RemoveLocalAuthFactors(
    auth::mojom::ConfigureResult result) {
  // Clear the online password as soon as we do not need it.
  online_password_.reset();
  if (result != auth::mojom::ConfigureResult::kSuccess) {
    context()->osauth_error = WizardContext::OSAuthErrorKind::kFatal;
    exit_callback_.Run(Result::kError);
    LOG(ERROR) << "Could not set online password";
    return;
  }
  LOG(WARNING) << "Set online password success.";

  // Early success if the user does not have a pin setup.
  if (!auth_factors_config_.HasConfiguredFactor(
          cryptohome::AuthFactorType::kPin)) {
    LOG(WARNING) << "No pin configured, showing success.";
    context()->knowledge_factor_setup.modified_factors.Put(
        AshAuthFactor::kGaiaPassword);
    ShowRemoveLocalAuthFactorsSucess();
    return;
  }

  auth::mojom::PinFactorEditor& pin_factor_editor = auth::GetPinFactorEditor(
      quick_unlock::QuickUnlockFactory::GetDelegate(), &local_state_.get(),
      *quick_unlock::PinBackend::GetInstance());
  pin_factor_editor.RemovePin(
      GetToken(), base::BindOnce(&RemoveLocalAuthFactorsScreen::OnPinRemoved,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void RemoveLocalAuthFactorsScreen::OnPinRemoved(
    auth::mojom::ConfigureResult result) {
  if (result != auth::mojom::ConfigureResult::kSuccess) {
    context()->osauth_error = WizardContext::OSAuthErrorKind::kFatal;
    LOG(ERROR) << "Could not remove Pin, still letting the user login as we "
                  "were able to set an online password.";
    // We still let the user login, to make sure that they do not get locked out
    // of their account due to any cryptohome related issue with removing the
    // pin.
    exit_callback_.Run(Result::kSuccess);
    return;
  }
  LOG(WARNING) << "Successfully removed Pin. All LocalAuthFactors removed.";

  context()->knowledge_factor_setup.modified_factors.Put(
      AshAuthFactor::kCryptohomePin);
  ShowRemoveLocalAuthFactorsSucess();
}

void RemoveLocalAuthFactorsScreen::ShowRemoveLocalAuthFactorsSucess() {
  if (!view_) {
    return;
  }
  view_->ShowRemoveLocalAuthFactorsSuccessStep();
}

void RemoveLocalAuthFactorsScreen::OnUserAction(const base::ListValue& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionDoneButtonClicked) {
    exit_callback_.Run(Result::kSuccess);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

AuthFactorEditor* RemoveLocalAuthFactorsScreen::GetAuthFactorEditor() {
  if (!auth_factor_editor_) {
    auth_factor_editor_ =
        std::make_unique<ash::AuthFactorEditor>(ash::UserDataAuthClient::Get());
  }
  return auth_factor_editor_.get();
}

}  // namespace ash
