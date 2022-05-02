// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/pin_setup_screen.h"

#include <memory>

#include "ash/components/login/auth/cryptohome_key_constants.h"
#include "ash/components/login/auth/user_context.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/auto_reset.h"
#include "base/check.h"
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
#include "chrome/browser/ui/webui/chromeos/login/pin_setup_screen_handler.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {

constexpr const char kUserActionDoneButtonClicked[] = "done-button";
constexpr const char kUserActionSkipButtonClickedOnStart[] =
    "skip-button-on-start";
constexpr const char kUserActionSkipButtonClickedInFlow[] =
    "skip-button-in-flow";

// If set to true ShouldSkipBecauseOfPolicy returns false.
static bool g_force_no_skip_because_of_policy_for_tests = false;

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
  if (g_force_no_skip_because_of_policy_for_tests)
    return false;
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  if (chrome_user_manager_util::IsPublicSessionOrEphemeralLogin() ||
      quick_unlock::IsPinDisabledByPolicy(prefs, quick_unlock::Purpose::kAny)) {
    return true;
  }

  return false;
}

// static
std::unique_ptr<base::AutoReset<bool>>
PinSetupScreen::SetForceNoSkipBecauseOfPolicyForTests(bool value) {
  return std::make_unique<base::AutoReset<bool>>(
      &g_force_no_skip_because_of_policy_for_tests, value);
}

PinSetupScreen::PinSetupScreen(PinSetupScreenView* view,
                               const ScreenExitCallback& exit_callback)
    : BaseScreen(PinSetupScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);

  quick_unlock::PinBackend::GetInstance()->HasLoginSupport(base::BindOnce(
      &PinSetupScreen::OnHasLoginSupport, weak_ptr_factory_.GetWeakPtr()));
}

PinSetupScreen::~PinSetupScreen() {
  if (view_)
    view_->Bind(nullptr);
}

bool PinSetupScreen::SkipScreen(WizardContext* context) {
  ClearAuthData(context);
  exit_callback_.Run(Result::NOT_APPLICABLE);
  return true;
}

bool PinSetupScreen::MaybeSkip(WizardContext* context) {
  if (context->skip_post_login_screens_for_tests || ShouldSkipBecauseOfPolicy())
    return SkipScreen(context);

  // Just a precaution:
  if (!context->extra_factors_auth_session)
    return SkipScreen(context);

  Profile* active_user_profile = ProfileManager::GetActiveUserProfile();

  // Show setup for Family Link users on tablet and clamshell if the device
  // supports PIN for login.
  bool show_for_family_link_user =
      active_user_profile->IsChild() && has_login_support_.value_or(false);
  if (show_for_family_link_user)
    return false;

  // Show setup for managed users if the device supports PIN for login.
  const bool is_managed_user =
      active_user_profile->GetProfilePolicyConnector()->IsManaged() &&
      !active_user_profile->IsChild();
  const bool show_for_managed_users =
      is_managed_user && has_login_support_.value_or(false);
  if (show_for_managed_users)
    return false;

  // Show the screen if the device is in tablet mode or tablet mode first user
  // run is forced on the device.
  if (TabletMode::Get()->InTabletMode() ||
      switches::ShouldOobeUseTabletModeFirstRun()) {
    return false;
  }

  return SkipScreen(context);
}

void PinSetupScreen::ShowImpl() {
  token_lifetime_timeout_.Start(
      FROM_HERE,
      base::Seconds(quick_unlock::AuthToken::kTokenExpirationSeconds),
      base::BindOnce(&PinSetupScreen::OnTokenTimedOut,
                     weak_ptr_factory_.GetWeakPtr()));
  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  quick_unlock_storage->MarkStrongAuth();
  std::unique_ptr<UserContext> user_context =
      std::move(context()->extra_factors_auth_session);

  // Due to crbug.com/1203420 we need to mark the key as a wildcard (no label).
  if (user_context->GetKey()->GetLabel() == kCryptohomeGaiaKeyLabel) {
    user_context->GetKey()->SetLabel(kCryptohomeWildcardLabel);
  }

  const std::string token =
      quick_unlock_storage->CreateAuthToken(*user_context);
  bool is_child_account =
      user_manager::UserManager::Get()->IsLoggedInAsChildUser();

  if (view_)
    view_->Show(token, is_child_account);

  quick_unlock::PinBackend::GetInstance()->HasLoginSupport(base::BindOnce(
      &PinSetupScreen::OnHasLoginSupport, weak_ptr_factory_.GetWeakPtr()));
}

void PinSetupScreen::HideImpl() {
  view_->Hide();
  token_lifetime_timeout_.Stop();
  ClearAuthData(context());
}

void PinSetupScreen::OnUserActionDeprecated(const std::string& action_id) {
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
  BaseScreen::OnUserActionDeprecated(action_id);
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

}  // namespace ash
