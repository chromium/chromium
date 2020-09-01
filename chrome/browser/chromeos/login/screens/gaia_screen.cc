// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/gaia_screen.h"

#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/wizard_context.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "components/account_id/account_id.h"

namespace {
constexpr char kUserActionCancel[] = "cancel";
}  // namespace

namespace chromeos {

// static
std::string GaiaScreen::GetResultString(Result result) {
  switch (result) {
    case Result::BACK:
      return "Back";
    case Result::CLOSE_DIALOG:
      return "CloseDialog";
  }
}

GaiaScreen::GaiaScreen(const ScreenExitCallback& exit_callback)
    : BaseScreen(GaiaView::kScreenId, OobeScreenPriority::DEFAULT),
      exit_callback_(exit_callback) {}

GaiaScreen::~GaiaScreen() {
  if (view_)
    view_->Unbind();
}

// static
GaiaScreen* GaiaScreen::Get(ScreenManager* manager) {
  return static_cast<GaiaScreen*>(manager->GetScreen(GaiaView::kScreenId));
}

void GaiaScreen::SetView(GaiaView* view) {
  view_ = view;
  if (view_)
    view_->Bind(this);
}

void GaiaScreen::MaybePreloadAuthExtension() {
  view_->MaybePreloadAuthExtension();
}

void GaiaScreen::LoadOnline(const AccountId& account) {
  view_->SetGaiaPath(GaiaView::GaiaPath::kDefault);
  view_->LoadGaiaAsync(account);
}

void GaiaScreen::LoadOnlineForChildSignup() {
  view_->SetGaiaPath(GaiaView::GaiaPath::kChildSignup);
  view_->LoadGaiaAsync(EmptyAccountId());
}

void GaiaScreen::LoadOnlineForChildSignin() {
  view_->SetGaiaPath(GaiaView::GaiaPath::kChildSignin);
  view_->LoadGaiaAsync(EmptyAccountId());
}

void GaiaScreen::LoadOffline(const AccountId& account) {
  view_->LoadOfflineGaia(account);
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
  if (action_id == kUserActionCancel) {
    if (context()->is_user_creation_enabled) {
      exit_callback_.Run(Result::BACK);
    } else if (context()->device_has_users) {
      exit_callback_.Run(Result::CLOSE_DIALOG);
    } else {
      LoadOnline(EmptyAccountId());
    }
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

}  // namespace chromeos
