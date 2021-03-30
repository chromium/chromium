// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/gaia_screen.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace {
constexpr char kUserActionBack[] = "back";
constexpr char kUserActionCancel[] = "cancel";
constexpr char kUserActionStartEnrollment[] = "startEnrollment";
}  // namespace

namespace chromeos {

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

GaiaScreen::GaiaScreen(const ScreenExitCallback& exit_callback)
    : BaseScreen(GaiaView::kScreenId, OobeScreenPriority::DEFAULT),
      exit_callback_(exit_callback) {}

GaiaScreen::~GaiaScreen() {
  if (view_)
    view_->Unbind();
}

void GaiaScreen::SetView(GaiaView* view) {
  view_ = view;
  if (view_)
    view_->Bind(this);
}

void GaiaScreen::LoadOnline(const AccountId& account) {
  if (!view_)
    return;
  auto gaia_path = GaiaView::GaiaPath::kDefault;
  if (!account.empty() && features::IsGaiaReauthEndpointEnabled()) {
    auto* user = user_manager::UserManager::Get()->FindUser(account);
    DCHECK(user);
    if (user && user->IsChild())
      gaia_path = GaiaView::GaiaPath::kReauth;
  }
  view_->SetGaiaPath(gaia_path);
  view_->LoadGaiaAsync(account);
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

void GaiaScreen::ShowImpl() {
  // Landed on the login screen. No longer skipping enrollment for tests.
  context()->skip_to_login_for_tests = false;
  view_->Show();
}

void GaiaScreen::HideImpl() {
  view_->SetGaiaPath(GaiaView::GaiaPath::kDefault);
  view_->Hide();
}

void GaiaScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionBack) {
    exit_callback_.Run(Result::BACK);
  } else if (action_id == kUserActionCancel) {
    exit_callback_.Run(Result::CANCEL);
  } else if (action_id == kUserActionStartEnrollment) {
    exit_callback_.Run(Result::ENTERPRISE_ENROLL);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

bool GaiaScreen::HandleAccelerator(ash::LoginAcceleratorAction action) {
  if (action == ash::LoginAcceleratorAction::kStartEnrollment) {
    exit_callback_.Run(Result::ENTERPRISE_ENROLL);
    return true;
  }
  if (action == ash::LoginAcceleratorAction::kEnableConsumerKiosk) {
    exit_callback_.Run(Result::START_CONSUMER_KIOSK);
    return true;
  }
  return false;
}

}  // namespace chromeos
