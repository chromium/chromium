// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/gaia_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/reauth_reason.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/account_status_check_fetcher.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config_utils.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {

constexpr char kUserActionBack[] = "back";
constexpr char kUserActionCancel[] = "cancel";
constexpr char kUserActionStartEnrollment[] = "startEnrollment";
constexpr char kUserActionReloadDefault[] = "reloadDefault";
constexpr char kUserActionEnterIdentifier[] = "identifierEntered";
constexpr char kUserActionQuickStartButtonClicked[] = "activateQuickStart";

bool ShouldPrepareForRecovery(const AccountId& account_id) {
  if (!account_id.is_valid()) {
    return false;
  }

  // Cryptohome recovery is probably needed when password is entered incorrectly
  // for many times or password changed.
  // TODO(b/197615068): Add metric to record the number of times we prepared for
  // recovery and the number of times recovery is actually required.
  static constexpr int kPossibleReasons[] = {
      static_cast<int>(ReauthReason::kIncorrectPasswordEntered),
      static_cast<int>(ReauthReason::kInvalidTokenHandle),
      static_cast<int>(ReauthReason::kSyncFailed),
      static_cast<int>(ReauthReason::kPasswordUpdateSkipped),
      static_cast<int>(ReauthReason::kForgotPassword),
      static_cast<int>(ReauthReason::kCryptohomeRecovery),
      static_cast<int>(ReauthReason::kOther),
  };
  user_manager::KnownUser known_user(g_browser_process->local_state());
  absl::optional<int> reauth_reason = known_user.FindReauthReason(account_id);
  return reauth_reason.has_value() &&
         base::Contains(kPossibleReasons, reauth_reason.value());
}

bool ShouldUseReauthEndpoint(const AccountId& account_id,
                             bool is_recovery_configured) {
  if (!account_id.is_valid()) {
    return false;
  }
  auto* user = user_manager::UserManager::Get()->FindUser(account_id);
  DCHECK(user);
  // Use reauth endpoint for child users.
  if (user && user->IsChild()) {
    return true;
  }

  // Use reauth endpoint in potential recovery flow.
  if (ShouldPrepareForRecovery(account_id) && is_recovery_configured) {
    return true;
  }

  return features::IsGaiaReauthEndpointEnabled();
}

}  // namespace

// static
std::string GaiaScreen::GetResultString(Result result) {
  switch (result) {
    case Result::BACK:
      return "Back";
    case Result::BACK_CHILD:
      return "BackChild";
    case Result::CANCEL:
      return "Cancel";
    case Result::ENTERPRISE_ENROLL:
      return "EnterpriseEnroll";
    case Result::START_CONSUMER_KIOSK:
      return "StartConsumerKiosk";
    case Result::QUICK_START:
      return "QuickStart";
  }
}

GaiaScreen::GaiaScreen(base::WeakPtr<TView> view,
                       const ScreenExitCallback& exit_callback)
    : BaseScreen(GaiaView::kScreenId, OobeScreenPriority::DEFAULT),
      auth_factor_editor_(UserDataAuthClient::Get()),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

GaiaScreen::~GaiaScreen() {
  backlights_forced_off_observation_.Reset();
}

void GaiaScreen::LoadOnlineGaia() {
  if (!view_) {
    return;
  }

  auto* context = LoginDisplayHost::default_host()->GetWizardContext();
  switch (context->gaia_config.gaia_path) {
    case WizardContext::GaiaPath::kDefault:
    case WizardContext::GaiaPath::kSamlRedirect:
    case WizardContext::GaiaPath::kReauth:
      LoadDefaultOnlineGaia(context->gaia_config.prefilled_account);
      break;
    case WizardContext::GaiaPath::kChildSignin:
    case WizardContext::GaiaPath::kChildSignup:
      view_->LoadGaiaAsync(EmptyAccountId());
      break;
  }
}

void GaiaScreen::LoadDefaultOnlineGaia(const AccountId& account) {
  if (!view_)
    return;

  view_->SetReauthRequestToken(std::string());

  // Always fetch Gaia reauth request token if the testing switch is set. It
  // will allow to test the recovery without triggering the real recovery
  // conditions which may be difficult as of now.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceCryptohomeRecoveryForTesting)) {
    LoginDisplayHost::default_host()
        ->GetWizardContext()
        ->gaia_config.gaia_path = WizardContext::GaiaPath::kReauth;
    FetchGaiaReauthToken(account);
    return;
  }

  if (account.is_valid()) {
    auto user_context = std::make_unique<UserContext>();
    user_context->SetAccountId(account);
    auth_factor_editor_.GetAuthFactorsConfiguration(
        std::move(user_context),
        base::BindOnce(&GaiaScreen::OnGetAuthFactorsConfiguration,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    if (GaiaScreenHandler::GetGaiaScreenMode(/*email=*/"") ==
        GaiaScreenHandler::GaiaScreenMode::GAIA_SCREEN_MODE_SAML_REDIRECT) {
      LoginDisplayHost::default_host()
          ->GetWizardContext()
          ->gaia_config.gaia_path = WizardContext::GaiaPath::kSamlRedirect;
    }
    view_->LoadGaiaAsync(account);
  }
}

void GaiaScreen::Reset() {
  if (!view_)
    return;

  auto* context = LoginDisplayHost::default_host()->GetWizardContext();
  context->gaia_config.gaia_path = WizardContext::GaiaPath::kDefault;
  context->gaia_config.prefilled_account = EmptyAccountId();
  view_->SetIsGaiaPasswordRequired(false);
  view_->Reset();
}

void GaiaScreen::ReloadGaiaAuthenticator() {
  if (!view_)
    return;
  view_->ReloadGaiaAuthenticator();
}

const std::string& GaiaScreen::EnrollmentNudgeEmail() {
  return enrollment_nudge_email_;
}

void GaiaScreen::ShowImpl() {
  if (!view_)
    return;

  if (!backlights_forced_off_observation_.IsObserving()) {
    backlights_forced_off_observation_.Observe(
        Shell::Get()->backlights_forced_off_setter());
  }

  LoadOnlineGaia();

  // Landed on the login screen. No longer skipping enrollment for tests.
  context()->skip_to_login_for_tests = false;
  view_->Show();

  // QuickStart should not be enabled for Demo mode or OS Install flows
  if (features::IsOobeQuickStartEnabled() &&
      !DemoSetupController::IsOobeDemoSetupFlowInProgress() &&
      !switches::IsOsInstallAllowed()) {
    // Determine the QuickStart button visibility
    WizardController::default_controller()
        ->quick_start_controller()
        ->DetermineEntryPointVisibility(
            base::BindOnce(&GaiaScreen::SetQuickStartButtonVisibility,
                           weak_ptr_factory_.GetWeakPtr()));
  }
}

void GaiaScreen::HideImpl() {
  // In the enrollment nudge flow it is assumed that `enrollment_nudge_email_`
  // was passed to enrollment screen before the execution of
  // `GaiaScreen::HideImpl()`. Here we are resetting it to make sure that we
  // don't accidentally reuse it in the future.
  enrollment_nudge_email_.clear();
  if (!view_)
    return;

  // Reset gaia_config after storing the current GAIA Path in
  // `gaia_config.last_gaia_path_shown`.
  context()->gaia_config.last_gaia_path_shown =
      context()->gaia_config.gaia_path;
  Reset();

  view_->Hide();
  backlights_forced_off_observation_.Reset();
}

void GaiaScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionBack) {
    WizardContext::GaiaPath gaiaPath = context()->gaia_config.gaia_path;
    if (gaiaPath == WizardContext::GaiaPath::kChildSignup ||
        gaiaPath == WizardContext::GaiaPath::kChildSignin) {
      exit_callback_.Run(Result::BACK_CHILD);
    } else {
      exit_callback_.Run(Result::BACK);
    }
  } else if (action_id == kUserActionCancel) {
    exit_callback_.Run(Result::CANCEL);
  } else if (action_id == kUserActionStartEnrollment) {
    exit_callback_.Run(Result::ENTERPRISE_ENROLL);
  } else if (action_id == kUserActionReloadDefault) {
    Reset();
    LoadDefaultOnlineGaia(EmptyAccountId());
  } else if (action_id == kUserActionEnterIdentifier) {
    CHECK_EQ(2u, args.size());
    const std::string& email = args[1].GetString();
    HandleIdentifierEntered(email);
  } else if (action_id == kUserActionQuickStartButtonClicked) {
    OnQuickStartButtonClicked();
  } else {
    BaseScreen::OnUserAction(args);
  }
}

bool GaiaScreen::HandleAccelerator(LoginAcceleratorAction action) {
  if (action == LoginAcceleratorAction::kStartEnrollment) {
    exit_callback_.Run(Result::ENTERPRISE_ENROLL);
    return true;
  }
  if (action == LoginAcceleratorAction::kEnableConsumerKiosk) {
    exit_callback_.Run(Result::START_CONSUMER_KIOSK);
    return true;
  }

  return false;
}

void GaiaScreen::OnScreenBacklightStateChanged(
    ScreenBacklightState screen_backlight_state) {
  if (screen_backlight_state == ScreenBacklightState::ON)
    return;
  exit_callback_.Run(Result::CANCEL);
}

void GaiaScreen::HandleIdentifierEntered(const std::string& user_email) {
  if (ShouldFetchEnrollmentNudgePolicy(user_email)) {
    view_->ToggleLoadingUI(true);
    account_status_fetcher_.reset();
    account_status_fetcher_ =
        std::make_unique<policy::AccountStatusCheckFetcher>(user_email);
    account_status_fetcher_->Fetch(
        base::BindOnce(&GaiaScreen::OnAccountStatusFetched,
                       base::Unretained(this), user_email),
        /*fetch_enrollment_nudge_policy=*/true);
    // Note: we don't check if user is allowlisted since
    // `ShouldFetchEnrollmentNudgePolicy` would return true only for unowned
    // devices in which case there are no device policies yet.
    return;
  }

  if (view_) {
    view_->CheckIfAllowlisted(user_email);
  }
}

void GaiaScreen::OnGetAuthFactorsConfiguration(
    std::unique_ptr<UserContext> user_context,
    absl::optional<AuthenticationError> error) {
  bool is_recovery_configured = false;
  bool is_gaia_password_configured = true;
  if (error.has_value()) {
    LOG(WARNING) << "Failed to get auth factors configuration, code "
                 << error->get_cryptohome_code()
                 << ", skip fetching reauth request token";
  } else {
    const auto& config = user_context->GetAuthFactorsConfiguration();
    is_recovery_configured =
        config.HasConfiguredFactor(cryptohome::AuthFactorType::kRecovery);
    auto* password_factor =
        config.FindFactorByType(cryptohome::AuthFactorType::kPassword);
    is_gaia_password_configured =
        password_factor && auth::IsGaiaPassword(*password_factor);
  }

  if (is_gaia_password_configured) {
    // Disallow passwordless login when Gaia password is configured.
    view_->SetIsGaiaPasswordRequired(true);
  }

  const AccountId& account_id = user_context->GetAccountId();
  WizardContext::GaiaPath& gaia_path = LoginDisplayHost::default_host()
                                           ->GetWizardContext()
                                           ->gaia_config.gaia_path;
  if (GaiaScreenHandler::GetGaiaScreenMode(account_id.GetUserEmail()) ==
      GaiaScreenHandler::GaiaScreenMode::GAIA_SCREEN_MODE_SAML_REDIRECT) {
    // TODO(b/309131477): use reauth path in "SAML redirect" mode if this is a
    // reauthentication of a known user.
    gaia_path = WizardContext::GaiaPath::kSamlRedirect;
  } else if (ShouldUseReauthEndpoint(account_id, is_recovery_configured)) {
    gaia_path = WizardContext::GaiaPath::kReauth;
  }

  if (ShouldPrepareForRecovery(account_id) && is_recovery_configured) {
    FetchGaiaReauthToken(account_id);
  } else {
    view_->LoadGaiaAsync(account_id);
  }
}

void GaiaScreen::FetchGaiaReauthToken(const AccountId& account) {
  gaia_reauth_token_fetcher_ = std::make_unique<GaiaReauthTokenFetcher>(
      base::BindOnce(&GaiaScreen::OnGaiaReauthTokenFetched,
                     weak_ptr_factory_.GetWeakPtr(), account));
  gaia_reauth_token_fetcher_->Fetch();
}

void GaiaScreen::OnGaiaReauthTokenFetched(const AccountId& account,
                                          const std::string& token) {
  if (token.empty()) {
    LOG(ERROR) << "Gaia reauth request token is empty";
  }
  gaia_reauth_token_fetcher_.reset();
  view_->SetReauthRequestToken(token);
  view_->LoadGaiaAsync(account);
}

void GaiaScreen::OnAccountStatusFetched(const std::string& user_email,
                                        bool fetch_succeeded,
                                        policy::AccountStatus status) {
  view_->ToggleLoadingUI(false);
  if (!fetch_succeeded) {
    // Enrollment Nudge is perceived as a non-critical UX improvement, so it is
    // acceptable to allow users to sign in if fetch fails for some reason.
    // Hence we just log an error here.
    // TODO(b/290924246): maybe also record this with UMA?
    LOG(ERROR) << "Failed to fetch Enrollment Nudge policy";
    return;
  }
  if (status.enrollment_required) {
    const std::string email_domain =
        chrome::enterprise_util::GetDomainFromEmail(user_email);
    // Cache email in case we will need to pass it to the enrollment screen.
    enrollment_nudge_email_ = user_email;
    view_->ShowEnrollmentNudge(email_domain);
  }
}

bool GaiaScreen::ShouldFetchEnrollmentNudgePolicy(
    const std::string& user_email) {
  const bool is_enterprise_managed = g_browser_process->platform_part()
                                         ->browser_policy_connector_ash()
                                         ->IsDeviceEnterpriseManaged();
  if (is_enterprise_managed) {
    // Device either already went through enterprise enrollment flow or goes
    // through it right now. No need for nudging.
    return false;
  }
  const bool is_first_user =
      user_manager::UserManager::Get()->GetUsers().empty();
  if (!is_first_user) {
    // Enrollment nudge targets only initial OOBE flow on unowned devices.
    // Current user is not a first user which means that device is already
    // owned.
    return false;
  }
  const std::string email_domain =
      chrome::enterprise_util::GetDomainFromEmail(user_email);
  // Enrollment nudging can't apply to users not belonging to a managed domain
  return !chrome::enterprise_util::IsKnownConsumerDomain(email_domain);
}

void GaiaScreen::OnQuickStartButtonClicked() {
  CHECK(context()->quick_start_enabled);
  exit_callback_.Run(Result::QUICK_START);
}

void GaiaScreen::SetQuickStartButtonVisibility(bool visible) {
  if (visible && view_) {
    view_->SetQuickStartEnabled();
  }
}

}  // namespace ash
