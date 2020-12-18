// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/pin_setup_screen.h"

#include "ash/public/cpp/tablet_mode.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_backend.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/pin_setup_screen_handler.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {

constexpr char kFinished[] = "finished";

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
    case Result::NEXT:
      return "Next";
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
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  return false;
}

void PinSetupScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void PinSetupScreen::HideImpl() {
  view_->Hide();
}

void PinSetupScreen::OnHasLoginSupport(bool has_login_support) {
  has_login_support_ = has_login_support;
}

void PinSetupScreen::OnUserAction(const std::string& action_id) {
  // Only honor finish if discover is currently being shown.
  if (action_id == kFinished) {
    exit_callback_.Run(Result::NEXT);
    return;
  }
  if (IsPinSetupUserAction(action_id)) {
    RecordUserAction(action_id);
    return;
  }
  BaseScreen::OnUserAction(action_id);
}

}  // namespace chromeos
