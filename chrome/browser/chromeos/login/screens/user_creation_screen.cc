// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/user_creation_screen.h"

#include "ash/public/cpp/login_screen.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/wizard_context.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"
#include "chromeos/constants/chromeos_features.h"

namespace {
constexpr char kUserActionSignIn[] = "signin";
constexpr char kUserActionChildSignIn[] = "child-signin";
constexpr char kUserActionChildAccountCreate[] = "child-account-create";
constexpr char kUserActionCancel[] = "cancel";
}  // namespace

namespace chromeos {

// static
std::string UserCreationScreen::GetResultString(Result result) {
  switch (result) {
    case Result::SIGNIN:
      return "SignIn";
    case Result::CHILD_SIGNIN:
      return "SignInAsChild";
    case Result::CHILD_ACCOUNT_CREATE:
      return "CreateChildAccount";
    case Result::ENTERPRISE_ENROLL:
      return "EnterpriseEnroll";
    case Result::CANCEL:
      return "Cancel";
    case Result::SKIPPED:
      return BaseScreen::kNotApplicable;
  }
}

UserCreationScreen::UserCreationScreen(UserCreationView* view,
                                       const ScreenExitCallback& exit_callback)
    : BaseScreen(UserCreationView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  if (view_)
    view_->Bind(this);
}

UserCreationScreen::~UserCreationScreen() {
  if (view_)
    view_->Unbind();
}

void UserCreationScreen::OnViewDestroyed(UserCreationView* view) {
  if (view_ == view)
    view_ = nullptr;
}

bool UserCreationScreen::MaybeSkip(WizardContext* context) {
  if (!features::IsChildSpecificSigninEnabled() ||
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->IsEnterpriseManaged() ||
      context->skip_to_login_for_tests) {
    context->is_user_creation_enabled = false;
    exit_callback_.Run(Result::SKIPPED);
    return true;
  }
  context->is_user_creation_enabled = true;
  return false;
}

void UserCreationScreen::ShowImpl() {
  if (!view_)
    return;

  ash::LoginScreen::Get()->ShowGuestButtonInOobe(true);

  // Back button is only available in login screen (add user flow) which is
  // indicated by if the device has users. Back button is hidden in the oobe
  // flow.
  view_->SetIsBackButtonVisible(context()->device_has_users);
  view_->Show();
}

void UserCreationScreen::HideImpl() {}

void UserCreationScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionSignIn) {
    context()->sign_in_as_child = false;
    exit_callback_.Run(Result::SIGNIN);
  } else if (action_id == kUserActionChildSignIn) {
    context()->sign_in_as_child = true;
    context()->is_child_gaia_account_new = false;
    exit_callback_.Run(Result::CHILD_SIGNIN);
  } else if (action_id == kUserActionChildAccountCreate) {
    context()->sign_in_as_child = true;
    context()->is_child_gaia_account_new = true;
    exit_callback_.Run(Result::CHILD_ACCOUNT_CREATE);
  } else if (action_id == kUserActionCancel) {
    exit_callback_.Run(Result::CANCEL);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

bool UserCreationScreen::HandleAccelerator(ash::LoginAcceleratorAction action) {
  if (action == ash::LoginAcceleratorAction::kStartEnrollment) {
    exit_callback_.Run(Result::ENTERPRISE_ENROLL);
    return true;
  }
  return false;
}

}  // namespace chromeos
