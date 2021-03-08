// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/edu_coexistence_login_screen.h"

#include <string>

#include "base/feature_list.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/login_display_host_mojo.h"
#include "chrome/browser/ash/login/ui/oobe_ui_dialog_delegate.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/supervised_user_features.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/signin/inline_login_dialog_chromeos_onboarding.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos {

// static
constexpr StaticOobeScreenId EduCoexistenceLoginScreen::kScreenId;

// static
EduCoexistenceLoginScreen* EduCoexistenceLoginScreen::Get(
    ScreenManager* screen_manager) {
  return static_cast<EduCoexistenceLoginScreen*>(
      screen_manager->GetScreen(EduCoexistenceLoginScreen::kScreenId));
}

// static
std::string EduCoexistenceLoginScreen::GetResultString(Result result) {
  switch (result) {
    case Result::DONE:
      return "Done";
    case Result::SKIPPED:
      return BaseScreen::kNotApplicable;
  }
}

EduCoexistenceLoginScreen::EduCoexistenceLoginScreen(
    const ScreenExitCallback& exit_callback)
    : BaseScreen(EduCoexistenceLoginScreen::kScreenId,
                 OobeScreenPriority::DEFAULT),
      exit_callback_(exit_callback) {
  observed_login_display_host_.Add(LoginDisplayHost::default_host());
}

EduCoexistenceLoginScreen::~EduCoexistenceLoginScreen() {}

bool EduCoexistenceLoginScreen::MaybeSkip(WizardContext* context) {
  if (!base::FeatureList::IsEnabled(supervised_users::kEduCoexistenceFlowV2)) {
    exit_callback_.Run(Result::SKIPPED);
    return true;
  }

  if (!ProfileManager::GetActiveUserProfile()->IsChild()) {
    exit_callback_.Run(Result::SKIPPED);
    return true;
  }

  return false;
}

void EduCoexistenceLoginScreen::ShowImpl() {
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  DCHECK(host);
  OobeUI* oobe_ui = host->GetOobeUI();
  DCHECK(oobe_ui);
  DCHECK(!dialog_delegate_);

  InlineLoginDialogChromeOSOnboarding* dialog =
      InlineLoginDialogChromeOSOnboarding::Show(
          oobe_ui->GetViewSize(),
          /* parent window */ oobe_ui->GetTopLevelNativeWindow(),
          base::BindOnce(exit_callback_, Result::DONE));

  dialog_delegate_ =
      std::make_unique<InlineLoginDialogChromeOSOnboarding::Delegate>(dialog);
  dialog_delegate_->UpdateDialogBounds(
      oobe_ui->GetNativeView()->GetBoundsInScreen());
}

void EduCoexistenceLoginScreen::HideImpl() {
  if (dialog_delegate_) {
    dialog_delegate_->CloseWithoutCallback();
    dialog_delegate_.reset();
  }
}

void EduCoexistenceLoginScreen::WebDialogViewBoundsChanged(
    const gfx::Rect& bounds) {
  if (!dialog_delegate_)
    return;

  dialog_delegate_->UpdateDialogBounds(bounds);
}

}  // namespace chromeos
