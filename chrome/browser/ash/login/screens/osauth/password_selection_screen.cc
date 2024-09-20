// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/password_selection_screen.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/password_selection_screen_handler.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config_utils.h"

namespace ash {

namespace {

constexpr char kUserActionBack[] = "back";
constexpr char kUserActionLocalPassword[] = "local-password";
constexpr char kUserActionGaiaPassword[] = "gaia-password";

// Returns `true` if the active Profile is enterprise managed.
bool IsUserEnterpriseManaged() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  return profile->GetProfilePolicyConnector()->IsManaged() &&
         !profile->IsChild();
}

}  // namespace

// static
std::string PasswordSelectionScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
    case Result::BACK:
      return "Back";
    case Result::LOCAL_PASSWORD_CHOICE:
      return "LocalPasswordChoice";
    case Result::GAIA_PASSWORD_CHOICE:
      return "GaiaPasswordChoice";
    case Result::LOCAL_PASSWORD_FORCED:
      return "LocalPasswordForced";
    case Result::GAIA_PASSWORD_FALLBACK:
      return "GaiaPasswordFallback";
    case Result::GAIA_PASSWORD_ENTERPRISE:
      return "GaiaPasswordEnterprise";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

PasswordSelectionScreen::PasswordSelectionScreen(
    base::WeakPtr<PasswordSelectionScreenView> view,
    ScreenExitCallback exit_callback)
    : BaseOSAuthSetupScreen(PasswordSelectionScreenView::kScreenId,
                            OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(std::move(exit_callback)) {}

PasswordSelectionScreen::~PasswordSelectionScreen() = default;

void PasswordSelectionScreen::ShowImpl() {
  is_shown_ = true;
  if (!view_) {
    return;
  }
  view_->Show();
  InspectContextAndContinue(
      base::BindOnce(&PasswordSelectionScreen::InspectContext,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PasswordSelectionScreen::ProcessOptions,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasswordSelectionScreen::HideImpl() {
  BaseOSAuthSetupScreen::HideImpl();
  is_shown_ = false;
}

void PasswordSelectionScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionBack) {
    exit_callback_.Run(Result::BACK);
    return;
  }
  if (action_id == kUserActionLocalPassword) {
    LOG(WARNING) << "Choice : Local password";
    context()->knowledge_factor_setup.local_password_forced = false;
    exit_callback_.Run(Result::LOCAL_PASSWORD_CHOICE);
    return;
  }
  if (action_id == kUserActionGaiaPassword) {
    LOG(WARNING) << "Choice : Online password";
    exit_callback_.Run(Result::GAIA_PASSWORD_CHOICE);
    return;
  }
  BaseScreen::OnUserAction(args);
}

bool PasswordSelectionScreen::MaybeSkip(WizardContext& wizard_context) {
  if (wizard_context.skip_post_login_screens_for_tests && is_shown_) {
    CHECK_IS_TEST();
    // WizardController::SkipPostLoginScreensForTesting() can be triggered
    // after screen is shown.
    exit_callback_.Run(Result::GAIA_PASSWORD_FALLBACK);
    return true;
  }
  return false;
}

void PasswordSelectionScreen::InspectContext(UserContext* user_context) {
  if (!user_context) {
    LOG(ERROR) << "Session expired while waiting for user's decision";
    // TODO(b/291808449): This should be an error.
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return;
  }
  // In reauthentication flow we don't always request AuthFactorsConfiguration.
  if (context()->knowledge_factor_setup.auth_setup_flow ==
      WizardContext::AuthChangeFlow::kReauthentication) {
    return;
  }
  CHECK(user_context->HasAuthFactorsConfiguration());
  auth_factors_config_ = user_context->GetAuthFactorsConfiguration();
  has_online_password_ = user_context->GetOnlinePassword().has_value();
}

void PasswordSelectionScreen::ProcessOptions() {
  if (auth_factors_config_.HasConfiguredFactor(
          cryptohome::AuthFactorType::kSmartCard)) {
    LOG(WARNING) << "Login using Smartcard, no password should be set up";
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return;
  }

  // Only show UI if the user is going trough initial setup, otherwise
  // detect which password need to be updated and return immediately.
  switch (context()->knowledge_factor_setup.auth_setup_flow) {
    case WizardContext::AuthChangeFlow::kReauthentication:
      LOG(WARNING) << "In reauthentication flow, should not update anything";
      exit_callback_.Run(Result::NOT_APPLICABLE);
      return;
    case WizardContext::AuthChangeFlow::kRecovery:
      if (!auth_factors_config_.HasConfiguredFactor(
              cryptohome::AuthFactorType::kPassword)) {
        // Here if the user does not have any password configured then we can
        // let them set their password according to the same condition as OOBE.
        // What is to note here is that after recovery, we already know the GAIA
        // password so both GAIA and local password factor are possible.
        LOG(ERROR) << "User does not have password configured when "
                      "performing recovery unexpectedly.";
        base::debug::DumpWithoutCrashing();
        break;
      } else if (auth::IsLocalPassword(*auth_factors_config_.FindFactorByType(
                     cryptohome::AuthFactorType::kPassword))) {
        exit_callback_.Run(Result::LOCAL_PASSWORD_FORCED);
        return;
      } else {
        CHECK(auth::IsGaiaPassword(*auth_factors_config_.FindFactorByType(
            cryptohome::AuthFactorType::kPassword)));
        if (!has_online_password_) {
          LOG(WARNING)
              << "User does not have online password, forcing local password";
          exit_callback_.Run(Result::LOCAL_PASSWORD_FORCED);
        } else {
          exit_callback_.Run(Result::GAIA_PASSWORD_FALLBACK);
        }
        return;
      }
    case WizardContext::AuthChangeFlow::kInitialSetup:
      break;
  }

  if (context()->skip_post_login_screens_for_tests) {
    LOG(WARNING) << "Skipping post-login screens, assuming that user has "
                    "online password";
    CHECK(has_online_password_)
        << "Skipping post-login screens requires non-empty GAIA password";
    // Some test configurations might already have a password configured for the
    // user.
    if (auth_factors_config_.HasConfiguredFactor(
            cryptohome::AuthFactorType::kPassword)) {
      LOG(WARNING) << "User already has a password configured.";
      exit_callback_.Run(Result::NOT_APPLICABLE);
    } else {
      exit_callback_.Run(Result::GAIA_PASSWORD_FALLBACK);
    }
    return;
  }

  if (auth_factors_config_.HasConfiguredFactor(
          cryptohome::AuthFactorType::kPassword)) {
    LOG(WARNING) << "User already has a password configured.";
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return;
  }

  if (IsUserEnterpriseManaged()) {
    LOG(WARNING) << "Managed user must use online password.";
    CHECK(has_online_password_)
        << "Managed users should have online password by this point";
    exit_callback_.Run(Result::GAIA_PASSWORD_ENTERPRISE);
    return;
  }
  if (!has_online_password_) {
    LOG(WARNING)
        << "User does not have online password, forcing local password";
    context()->knowledge_factor_setup.local_password_forced = true;
    exit_callback_.Run(Result::LOCAL_PASSWORD_FORCED);
    return;
  }

  EstablishKnowledgeFactorGuard(
      base::BindOnce(&PasswordSelectionScreen::ShowPasswordChoice,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasswordSelectionScreen::ShowPasswordChoice() {
  view_->ShowPasswordChoice();
}

}  // namespace ash
