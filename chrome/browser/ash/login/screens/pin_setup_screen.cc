// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/pin_setup_screen.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/pin_setup_screen_handler.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "ui/display/screen.h"

namespace ash {
namespace {

constexpr const char kUserActionDoneButtonClicked[] = "done-button";
constexpr const char kUserActionSkipButtonClickedOnStart[] =
    "skip-button-on-start";
constexpr const char kUserActionSkipButtonClickedInFlow[] =
    "skip-button-in-flow";

struct PinSetupUserAction {
  const char* name_;
  PinSetupScreen::UserAction uma_name_;
};

const PinSetupUserAction actions[] = {
    {kUserActionDoneButtonClicked,
     PinSetupScreen::UserAction::kDoneButtonClicked},
    {kUserActionSkipButtonClickedOnStart,
     PinSetupScreen::UserAction::kSkipButtonClickedOnStart},
    {kUserActionSkipButtonClickedInFlow,
     PinSetupScreen::UserAction::kSkipButtonClickedInFlow},
};

void RecordPinSetupScreenAction(PinSetupScreen::UserAction value) {
  base::UmaHistogramEnumeration("OOBE.PinSetupScreen.UserActions", value);
}

void RecordUserAction(const std::string& action_id) {
  for (const auto& el : actions) {
    if (action_id == el.name_) {
      RecordPinSetupScreenAction(el.uma_name_);
      return;
    }
  }
  NOTREACHED_IN_MIGRATION() << "Unexpected action id: " << action_id;
}

}  // namespace

// static
std::string PinSetupScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kDone:
      return "Done";
    case Result::kUserSkip:
      return "Skipped";
    case Result::kTimedOut:
      return "TimedOut";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

PinSetupScreen::PinSetupScreen(base::WeakPtr<PinSetupScreenView> view,
                               const ScreenExitCallback& exit_callback)
    : BaseScreen(PinSetupScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback),
      auth_performer_(UserDataAuthClient::Get()),
      cryptohome_pin_engine_(&auth_performer_) {
  DCHECK(view_);

  quick_unlock::PinBackend::GetInstance()->HasLoginSupport(base::BindOnce(
      &PinSetupScreen::OnHasLoginSupport, weak_ptr_factory_.GetWeakPtr()));
}

PinSetupScreen::~PinSetupScreen() = default;

std::optional<PinSetupScreen::SkipReason> PinSetupScreen::GetSkipReason(
    WizardContext& context) {
  CHECK(has_login_support_.has_value());

  if (context.skip_post_login_screens_for_tests) {
    return SkipReason::kSkippedForTests;
  }

  if (!context.extra_factors_token.has_value()) {
    return SkipReason::kMissingExtraFactorsToken;
  }

  if (!ash::AuthSessionStorage::Get()->IsValid(
          context.extra_factors_token.value())) {
    return SkipReason::kExpiredToken;
  }

  if (chrome_user_manager_util::IsManagedGuestSessionOrEphemeralLogin()) {
    return SkipReason::kManagedGuestSessionOrEphemeralLogin;
  }

  AccountId account_id = ash::AuthSessionStorage::Get()
                             ->Peek(context.extra_factors_token.value())
                             ->GetAccountId();
  if (cryptohome_pin_engine_.ShouldSkipSetupBecauseOfPolicy(account_id)) {
    return SkipReason::kNotAllowedByPolicy;
  }

  // Hardware capability check. In order for the screen to be shown, the device
  // needs to support PIN for login, OR be a tablet device.
  const bool is_device_a_tablet =
      display::Screen::GetScreen()->InTabletMode() ||
      switches::ShouldOobeUseTabletModeFirstRun();
  if (!(is_device_a_tablet || has_login_support_.value())) {
    return SkipReason::kUsupportedHardware;
  }

  // Will not be skipped.
  return std::nullopt;
}

bool PinSetupScreen::MaybeSkip(WizardContext& context) {
  // If cryptohome takes very long to respond, `has_login_support_` may be null
  // here, but this is very unusual. In that case, assume that there is no
  // support for login.
  if (!has_login_support_.has_value()) {
    LOG(WARNING) << "Could not determine hardware support support for login";
    has_login_support_ = false;
  }

  const auto skip_reason = GetSkipReason(context);
  if (skip_reason.has_value()) {
    ClearAuthData(context);
    // TODO(b/365059362) : Create new metric to track the detailed skip reason.
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  return false;
}

void PinSetupScreen::ShowImpl() {
  token_lifetime_timeout_.Start(FROM_HERE,
                                quick_unlock::AuthToken::kTokenExpiration,
                                base::BindOnce(&PinSetupScreen::OnTokenTimedOut,
                                               weak_ptr_factory_.GetWeakPtr()));
  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  quick_unlock_storage->MarkStrongAuth();
  std::string token;
  CHECK(context()->extra_factors_token);
  token = *context()->extra_factors_token;
  bool is_child_account =
      user_manager::UserManager::Get()->IsLoggedInAsChildUser();

  if (view_) {
    view_->Show(token, is_child_account, has_login_support_.value_or(false),
                using_pin_as_main_factor_.value_or(false));
  }
}

void PinSetupScreen::HideImpl() {
  token_lifetime_timeout_.Stop();
  ClearAuthData(*context());
}

void PinSetupScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionDoneButtonClicked) {
    RecordUserAction(action_id);
    token_lifetime_timeout_.Stop();
    exit_callback_.Run(Result::kDone);
    return;
  }
  if (action_id == kUserActionSkipButtonClickedOnStart ||
      action_id == kUserActionSkipButtonClickedInFlow) {
    RecordUserAction(action_id);
    token_lifetime_timeout_.Stop();
    exit_callback_.Run(Result::kUserSkip);
    return;
  }
  BaseScreen::OnUserAction(args);
}

void PinSetupScreen::ClearAuthData(WizardContext& context) {
  if (context.extra_factors_token.has_value()) {
    ash::AuthSessionStorage::Get()->Invalidate(
        context.extra_factors_token.value(), base::DoNothing());
    context.extra_factors_token = std::nullopt;
  }
}

void PinSetupScreen::OnHasLoginSupport(bool login_available) {
  has_login_support_ = login_available;
  if (!is_hidden() && view_) {
    view_->SetLoginSupportAvailable(has_login_support_.value());
  }

  // When hardware support is available, PIN will be offered as a main factor.
  if (ash::switches::IsOobePinOnlyPrototypeEnabled()) {
    // The first value is only set once, based on hardware capability.
    if (using_pin_as_main_factor_.has_value()) {
      return;
    }
    using_pin_as_main_factor_ = login_available;
  }
}

void PinSetupScreen::OnTokenTimedOut() {
  ClearAuthData(*context());
  exit_callback_.Run(Result::kTimedOut);
}

}  // namespace ash
