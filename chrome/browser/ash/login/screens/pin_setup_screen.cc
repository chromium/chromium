// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/pin_setup_screen.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/pin_setup_screen_handler.h"
#include "chromeos/login/auth/cryptohome_key_constants.h"
#include "chromeos/login/auth/user_context.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

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

bool IsPinSetupUserAction(const std::string& action_id) {
  for (const auto& el : actions) {
    if (action_id == el.name_)
      return true;
  }
  return false;
}

void RecordUserAction(const std::string& action_id) {
  for (const auto& el : actions) {
    if (action_id == el.name_) {
      RecordPinSetupScreenAction(el.uma_name_);
      return;
    }
  }
  NOTREACHED() << "Unexpected action id: " << action_id;
}

}  // namespace

// static
std::string PinSetupScreen::GetResultString(Result result) {
  switch (result) {
    case Result::DONE:
      return "Done";
    case Result::USER_SKIP:
      return "Skipped";
    case Result::TIMED_OUT:
      return "TimedOut";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

// static
bool PinSetupScreen::ShouldSkipBecauseOfPolicy() {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  if (chrome_user_manager_util::IsPublicSessionOrEphemeralLogin() ||
      !quick_unlock::IsPinEnabled(prefs) ||
      quick_unlock::IsPinDisabledByPolicy(prefs)) {
    return true;
  }

  return false;
}

PinSetupScreen::PinSetupScreen(PinSetupScreenView* view,
                               const ScreenExitCallback& exit_callback)
    : BaseScreen(PinSetupScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);

  if (features::IsPinSetupForFamilyLinkEnabled()) {
    quick_unlock::PinBackend::GetInstance()->HasLoginSupport(base::BindOnce(
        &PinSetupScreen::OnHasLoginSupport, weak_ptr_factory_.GetWeakPtr()));
  }
}

PinSetupScreen::~PinSetupScreen() {
  if (view_)
    view_->Bind(nullptr);
}

bool PinSetupScreen::MaybeSkip(WizardContext* context) {
  if (ShouldSkipBecauseOfPolicy()) {
    ClearAuthData(context);
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  // Show setup for Family Link users on tablet and clamshell if the device
  // supports PIN for login.
  bool show_for_family_link_user =
      features::IsPinSetupForFamilyLinkEnabled() &&
      ProfileManager::GetActiveUserProfile()->IsChild() &&
      has_login_support_.value_or(false);

  // Skip the screen if the device is not in tablet mode, unless tablet mode
  // first user run is forced on the device.
  if (!ash::TabletMode::Get()->InTabletMode() &&
      !switches::ShouldOobeUseTabletModeFirstRun() &&
      !show_for_family_link_user) {
    ClearAuthData(context);
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  // Just a precaution:
  if (!context->extra_factors_auth_session) {
    exit_callback_.Run(Result::TIMED_OUT);
    return true;
  }
  return false;
}

void PinSetupScreen::ShowImpl() {
  token_lifetime_timeout_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(
          chromeos::quick_unlock::AuthToken::kTokenExpirationSeconds),
      base::BindOnce(&PinSetupScreen::OnTokenTimedOut,
                     weak_ptr_factory_.GetWeakPtr()));
  chromeos::quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      chromeos::quick_unlock::QuickUnlockFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  quick_unlock_storage->MarkStrongAuth();
  std::unique_ptr<UserContext> user_context =
      std::move(context()->extra_factors_auth_session);

  // Due to crbug.com/1203420 we need to mark the key as a wildcard (no label).
  if (user_context->GetKey()->GetLabel() == chromeos::kCryptohomeGaiaKeyLabel) {
    user_context->GetKey()->SetLabel(chromeos::kCryptohomeWildcardLabel);
  }

  const std::string token =
      quick_unlock_storage->CreateAuthToken(*user_context);

  if (view_)
    view_->Show(token);

  chromeos::quick_unlock::PinBackend::GetInstance()->HasLoginSupport(
      base::BindOnce(&PinSetupScreen::OnHasLoginSupport,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PinSetupScreen::HideImpl() {
  view_->Hide();
  token_lifetime_timeout_.Stop();
  ClearAuthData(context());
}

void PinSetupScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionDoneButtonClicked) {
    RecordUserAction(action_id);
    token_lifetime_timeout_.Stop();
    exit_callback_.Run(Result::DONE);
    return;
  }
  if (action_id == kUserActionSkipButtonClickedOnStart ||
      action_id == kUserActionSkipButtonClickedInFlow) {
    RecordUserAction(action_id);
    token_lifetime_timeout_.Stop();
    exit_callback_.Run(Result::USER_SKIP);
    return;
  }
  BaseScreen::OnUserAction(action_id);
}

void PinSetupScreen::ClearAuthData(WizardContext* context) {
  context->extra_factors_auth_session.reset();
}

void PinSetupScreen::OnHasLoginSupport(bool login_available) {
  if (view_)
    view_->SetLoginSupportAvailable(login_available);
  has_login_support_ = login_available;
}

void PinSetupScreen::OnTokenTimedOut() {
  ClearAuthData(context());
  exit_callback_.Run(Result::TIMED_OUT);
}

}  // namespace chromeos
