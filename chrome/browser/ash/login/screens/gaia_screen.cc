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
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/account_status_check_fetcher.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
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
constexpr char kUserActionRetry[] = "retry";
constexpr char kUserActionEnterIdentifier[] = "identifierEntered";
constexpr char kUserActionQuickStartButtonClicked[] = "activateQuickStart";

bool ShouldPrepareForRecovery(const AccountId& account_id) {
  if (!features::IsCryptohomeRecoveryEnabled() || !account_id.is_valid()) {
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

bool ShouldUseReauthEndpoint(const AccountId& account_id) {
  if (account_id.empty()) {
    return false;
  }
  auto* user = user_manager::UserManager::Get()->FindUser(account_id);
  DCHECK(user);
  // Use reauth endpoint for child users.
  if (user && user->IsChild()) {
    return true;
  }
  // Use reauth endpoint for potential recovery use cases (exclude cases where
  // reauth enforced by policy).
  if (features::IsGaiaReauthEndpointEnabled() &&
      ShouldPrepareForRecovery(account_id)) {
    return true;
  }
  return false;
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

void GaiaScreen::LoadOnline(const AccountId& account) {
  if (!view_)
    return;
  auto gaia_path = GaiaView::GaiaPath::kDefault;
  if (ShouldUseReauthEndpoint(account)) {
    gaia_path = GaiaView::GaiaPath::kReauth;
  }
  view_->SetGaiaPath(gaia_path);
  view_->SetReauthRequestToken(std::string());

  // Always fetch Gaia reauth request token if the testing switch is set. It
  // will allow to test the recovery without triggering the real recovery
  // conditions which may be difficult as of now.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceCryptohomeRecoveryForTesting)) {
    DCHECK(features::IsCryptohomeRecoveryEnabled());
    FetchGaiaReauthToken(account);
    return;
  }

  if (ShouldPrepareForRecovery(account)) {
    auto user_context = std::make_unique<UserContext>();
    user_context->SetAccountId(account);
    auth_factor_editor_.GetAuthFactorsConfiguration(
        std::move(user_context),
        base::BindOnce(&GaiaScreen::OnGetAuthFactorsConfiguration,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    view_->LoadGaiaAsync(account);
  }
}

void GaiaScreen::LoadOnlineForChildSignup() {
  if (!view_)
    return;
  view_->SetGaiaPath(GaiaView::GaiaPath::kChildSignup);
  view_->LoadGaiaAsync(EmptyAccountId());
}

void GaiaScreen::LoadOnlineForChildSignin() {
  if (!view_)
    return;
  view_->SetGaiaPath(GaiaView::GaiaPath::kChildSignin);
  view_->LoadGaiaAsync(EmptyAccountId());
}

void GaiaScreen::ShowAllowlistCheckFailedError() {
  if (!view_)
    return;
  view_->ShowAllowlistCheckFailedError();
}

void GaiaScreen::Reset() {
  if (!view_)
    return;
  view_->SetGaiaPath(GaiaView::GaiaPath::kDefault);
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
  // Landed on the login screen. No longer skipping enrollment for tests.
  context()->skip_to_login_for_tests = false;
  view_->Show();

  // Quick Start can be enabled either by feature flag or by keyboard shortcut.
  // The shortcut method enables a simpler workflow for testers, while the
  // feature flag will enable us to perform a first run field trial.
  // QuickStart should not be enabled for Demo mode or OS Install flows
  if (features::IsOobeQuickStartEnabled() &&
      !DemoSetupController::IsOobeDemoSetupFlowInProgress() &&
      !switches::IsOsInstallAllowed()) {
    EnableQuickStart();
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
  view_->SetGaiaPath(GaiaView::GaiaPath::kDefault);
  view_->Hide();
  backlights_forced_off_observation_.Reset();
  if (context()->quick_start_enabled) {
    bootstrap_controller_.reset();
  }
}

void GaiaScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionBack) {
    GaiaView::GaiaPath gaiaPath = view_->GetGaiaPath();
    if (gaiaPath == GaiaView::GaiaPath::kChildSignup ||
        gaiaPath == GaiaView::GaiaPath::kChildSignin) {
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
    LoadOnline(EmptyAccountId());
  } else if (action_id == kUserActionRetry) {
    LoadOnline(EmptyAccountId());
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
        /*fetch_entollment_nudge_policy=*/true);
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
  if (error.has_value()) {
    LOG(WARNING) << "Failed to get auth factors configuration, code "
                 << error->get_cryptohome_code()
                 << ", skip fetching reauth request token";
    view_->LoadGaiaAsync(user_context->GetAccountId());
    return;
  }

  const auto& config = user_context->GetAuthFactorsConfiguration();
  bool is_configured =
      config.HasConfiguredFactor(cryptohome::AuthFactorType::kRecovery);
  if (is_configured) {
    FetchGaiaReauthToken(user_context->GetAccountId());
  } else {
    view_->LoadGaiaAsync(user_context->GetAccountId());
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
    context()->gaia_reauth_token_fetch_error = true;
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

void GaiaScreen::EnableQuickStart() {
  context()->quick_start_enabled = true;
  bootstrap_controller_ =
      LoginDisplayHost::default_host()->GetQuickStartBootstrapController();

  bootstrap_controller_->GetFeatureSupportStatusAsync(
      base::BindOnce(&GaiaScreen::OnGetQuickStartFeatureSupportStatus,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GaiaScreen::OnGetQuickStartFeatureSupportStatus(
    quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus status) {
  if (status != quick_start::TargetDeviceConnectionBroker::
                    FeatureSupportStatus::kSupported) {
    return;
  }

  if (!view_) {
    return;
  }
  view_->SetQuickStartEnabled();
}

}  // namespace ash
