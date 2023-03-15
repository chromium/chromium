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
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/browser_process.h"
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
  };
  user_manager::KnownUser known_user(g_browser_process->local_state());
  absl::optional<int> reauth_reason = known_user.FindReauthReason(account_id);
  return reauth_reason.has_value() &&
         base::Contains(kPossibleReasons, reauth_reason.value());
}

}  // namespace

// static
std::string GaiaScreen::GetResultString(Result result) {
  switch (result) {
    case Result::BACK:
      return "Back";
    case Result::CANCEL:
      return "Cancel";
    case Result::ENTERPRISE_ENROLL:
      return "EnterpriseEnroll";
    case Result::START_CONSUMER_KIOSK:
      return "StartConsumerKiosk";
  }
}

GaiaScreen::GaiaScreen(base::WeakPtr<TView> view,
                       const ScreenExitCallback& exit_callback)
    : BaseScreen(GaiaView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

GaiaScreen::~GaiaScreen() {
  backlights_forced_off_observation_.Reset();
}

void GaiaScreen::LoadOnline(const AccountId& account) {
  if (!view_)
    return;
  auto gaia_path = GaiaView::GaiaPath::kDefault;
  if (!account.empty()) {
    auto* user = user_manager::UserManager::Get()->FindUser(account);
    DCHECK(user);
    if (user && (user->IsChild() || features::IsGaiaReauthEndpointEnabled()))
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

  // TODO(272474463): remove the Gaia path check.
  if (gaia_path == GaiaView::GaiaPath::kDefault &&
      ShouldPrepareForRecovery(account)) {
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
}

void GaiaScreen::HideImpl() {
  if (!view_)
    return;
  view_->SetGaiaPath(GaiaView::GaiaPath::kDefault);
  view_->Hide();
  backlights_forced_off_observation_.Reset();
}

void GaiaScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionBack) {
    exit_callback_.Run(Result::BACK);
  } else if (action_id == kUserActionCancel) {
    exit_callback_.Run(Result::CANCEL);
  } else if (action_id == kUserActionStartEnrollment) {
    exit_callback_.Run(Result::ENTERPRISE_ENROLL);
  } else if (action_id == kUserActionReloadDefault) {
    Reset();
    LoadOnline(EmptyAccountId());
  } else if (action_id == kUserActionRetry) {
    LoadOnline(EmptyAccountId());
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

}  // namespace ash
