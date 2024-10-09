// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/pin_setup_screen.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/debug/dump_without_crashing.h"
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

using PinSetupMode = WizardContext::PinSetupMode;

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

// Utility to check if the screen is operating in the given mode. WizardContext
// is only available between the Show/Hide calls. During `MaybeSkip`
// WizardController provides a reference to it.
bool IsInSetupMode(PinSetupMode mode, WizardContext& context) {
  CHECK(context.knowledge_factor_setup.pin_setup_mode.has_value());
  const bool mode_matches =
      context.knowledge_factor_setup.pin_setup_mode.value() == mode;
  if (mode == PinSetupMode::kSetupAsPrimaryFactor ||
      mode == PinSetupMode::kAlreadyPerformed) {
    // These modes are only available when PasswordlessSetup is enabled.
    return mode_matches && ash::switches::IsOobePinOnlyPrototypeEnabled();
  } else {
    return mode_matches;
  }
}

}  // namespace

// static
std::string PinSetupScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    // Keep original 'Done'
    case Result::kDoneAsSecondaryFactor:
      return "Done";
    case Result::kUserSkip:
      return "Skipped";
    case Result::kTimedOut:
      return "TimedOut";
    case Result::kUserChosePassword:
      return "UserChosePassword";
    case Result::kDoneAsMainFactor:
      return "DoneAsMainFactor";
    case Result::kNotApplicable:
    case Result::kNotApplicableAsPrimaryFactor:
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
  CHECK(hardware_support_.has_value());

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
  const bool has_login_support =
      hardware_support_.value() == HardwareSupport::kLoginCompatible;
  if (!(is_device_a_tablet || has_login_support)) {
    return SkipReason::kUsupportedHardware;
  }

  // Further checks for the PIN-only setup mode. It needs to login support and
  // it is only available for consumers.
  if (IsInSetupMode(PinSetupMode::kSetupAsPrimaryFactor, context)) {
    if (!has_login_support) {
      return SkipReason::kNotSupportedAsPrimaryFactor;
    }
    // TODO(b/365059362): Skip for managed users.
  }

  // Second surfacing of the PIN setup screen after setting a PIN as primary.
  if (IsInSetupMode(PinSetupMode::kAlreadyPerformed, context)) {
    return SkipReason::kPinAlreadySet;
  }

  // Will not be skipped.
  return std::nullopt;
}

bool PinSetupScreen::MaybeSkip(WizardContext& context) {
  DetermineHardwareSupport();

  // TODO(b/365059362): Create new metric to track the detailed skip reason.
  const auto skip_reason = GetSkipReason(context);
  if (skip_reason.has_value()) {
    if (IsInSetupMode(PinSetupMode::kSetupAsPrimaryFactor, context)) {
      exit_callback_.Run(Result::kNotApplicableAsPrimaryFactor);
      return true;
    }

    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  return false;
}

void PinSetupScreen::ShowImpl() {
  // The following are considered invariants when the screen is shown. These
  // values must have either been set at this point, or the screen should have
  // been skipped.
  CHECK(hardware_support_.has_value());
  CHECK(context()->extra_factors_token);
  CHECK(!IsInSetupMode(PinSetupMode::kAlreadyPerformed, *context()));

  // When the screen is being shown offering PIN as a secondary factor
  // factor, a timer is used for invalidating the AuthSession.
  // TODO(b/365059362): Replace legacy timer logic with a AuthSessionStorage
  // notification that will be triggered upon invalidation.
  if (IsInSetupMode(PinSetupMode::kSetupAsSecondaryFactor, *context())) {
    token_lifetime_timeout_.Start(
        FROM_HERE, quick_unlock::AuthToken::kTokenExpiration,
        base::BindOnce(&PinSetupScreen::OnTokenTimedOut,
                       weak_ptr_factory_.GetWeakPtr()));
    quick_unlock::QuickUnlockStorage* quick_unlock_storage =
        quick_unlock::QuickUnlockFactory::GetForProfile(
            ProfileManager::GetActiveUserProfile());
    quick_unlock_storage->MarkStrongAuth();
  } else {
    // When PIN is being offered as the main factor, the AuthSession must remain
    // alive until the user sets their PIN, or until a password is set.
    CHECK(IsInSetupMode(PinSetupMode::kSetupAsPrimaryFactor, *context()));
    session_refresher_ = AuthSessionStorage::Get()->KeepAlive(
        context()->extra_factors_token.value());
  }

  const std::string token = *context()->extra_factors_token;
  bool is_child_account =
      user_manager::UserManager::Get()->IsLoggedInAsChildUser();

  const bool using_pin_as_main_factor =
      IsInSetupMode(PinSetupMode::kSetupAsPrimaryFactor, *context());
  const bool has_login_support =
      hardware_support_.value() == HardwareSupport::kLoginCompatible;
  if (view_) {
    // TODO(b/365059362): Wrap arguments in a struct.
    view_->Show(token, is_child_account, has_login_support,
                using_pin_as_main_factor);
  }
}

void PinSetupScreen::HideImpl() {
  token_lifetime_timeout_.Stop();
  session_refresher_.reset();
}

void PinSetupScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionDoneButtonClicked) {
    RecordUserAction(action_id);
    token_lifetime_timeout_.Stop();
    if (IsInSetupMode(PinSetupMode::kSetupAsPrimaryFactor, *context())) {
      exit_callback_.Run(Result::kDoneAsMainFactor);
    } else {
      exit_callback_.Run(Result::kDoneAsSecondaryFactor);
    }
    return;
  }
  if (action_id == kUserActionSkipButtonClickedOnStart ||
      action_id == kUserActionSkipButtonClickedInFlow) {
    RecordUserAction(action_id);
    token_lifetime_timeout_.Stop();
    if (IsInSetupMode(PinSetupMode::kSetupAsPrimaryFactor, *context())) {
      exit_callback_.Run(Result::kUserChosePassword);
    } else {
      exit_callback_.Run(Result::kUserSkip);
    }
    return;
  }
  BaseScreen::OnUserAction(args);
}

void PinSetupScreen::DetermineHardwareSupport() {
  // If cryptohome takes very long to respond, the hardware support status may
  // still be undetermined. (This is very unusual and is being tracked in
  // b/369749485). In that case, assume that there is no support for login.
  if (!hardware_support_.has_value()) {
    LOG(WARNING) << "Could not determine hardware support support for login";
    hardware_support_ = HardwareSupport::kUnlockOnly;
  }
}

void PinSetupScreen::OnHasLoginSupport(bool login_available) {
  if (hardware_support_.has_value()) {
    LOG(WARNING) << "Hardware support for login determined too late.";
    // Generate a crash report to investigate this edge case. This is unlikely
    // to happen nowadays.
    // TODO(b/369749485): Remove once no longer necessary.
    base::debug::DumpWithoutCrashing();
    return;
  }

  hardware_support_ = login_available ? HardwareSupport::kLoginCompatible
                                      : HardwareSupport::kUnlockOnly;
}

void PinSetupScreen::OnTokenTimedOut() {
  exit_callback_.Run(Result::kTimedOut);
}

}  // namespace ash
