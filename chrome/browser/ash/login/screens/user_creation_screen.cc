// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/user_creation_screen.h"

#include "ash/public/cpp/login_screen.h"
#include "chrome/browser/ash/login/error_screens_histogram_helper.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

namespace {

constexpr char kUserActionSignIn[] = "signin";
constexpr char kUserActionAddChild[] = "add-child";
constexpr char kUserActionCancel[] = "cancel";

// The following user actions are only possible when `OobeSoftwareUpdate` flag
// is enabled.
constexpr char kUserActionSignInTriage[] = "signin-triage";
constexpr char kUserActionSignInSchool[] = "signin-school";
constexpr char kUserActionEnroll[] = "enroll";
constexpr char kUserActionTriage[] = "triage";
constexpr char kUserActionChildSetup[] = "child-setup";

UserCreationScreen::UserCreationScreenExitTestDelegate* test_exit_delegate =
    nullptr;

}  // namespace

// static
std::string UserCreationScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::SIGNIN:
      return "SignIn";
    case Result::SIGNIN_TRIAGE:
      return "SignInTriage";
    case Result::ADD_CHILD:
      return "AddChild";
    case Result::ENTERPRISE_ENROLL_TRIAGE:
      return "EnterpriseEnrollTriage";
    case Result::ENTERPRISE_ENROLL_SHORTCUT:
      return "EnterpriseEnrollShortcut";
    case Result::KIOSK_ENTERPRISE_ENROLL:
      return "KioskEnterpriseEnroll";
    case Result::CANCEL:
      return "Cancel";
    case Result::SIGNIN_SCHOOL:
      return "SignInSchool";
    case Result::SKIPPED:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

UserCreationScreen::UserCreationScreen(base::WeakPtr<UserCreationView> view,
                                       ErrorScreen* error_screen,
                                       const ScreenExitCallback& exit_callback)
    : BaseScreen(UserCreationView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      histogram_helper_(std::make_unique<ErrorScreensHistogramHelper>(
          ErrorScreensHistogramHelper::ErrorParentScreen::kUserCreation)),
      error_screen_(error_screen),
      exit_callback_(exit_callback) {
  network_state_informer_ = base::MakeRefCounted<NetworkStateInformer>();
  network_state_informer_->Init();
}

UserCreationScreen::~UserCreationScreen() = default;

// static
void UserCreationScreen::SetUserCreationScreenExitTestDelegate(
    UserCreationScreen::UserCreationScreenExitTestDelegate* test_delegate) {
  test_exit_delegate = test_delegate;
}

bool UserCreationScreen::MaybeSkip(WizardContext& context) {
  const bool is_managed = ash::InstallAttributes::Get()->IsEnterpriseManaged();
  context.is_user_creation_enabled = !is_managed;
  if (context.skip_to_login_for_tests || is_managed) {
    RunExitCallback(Result::SKIPPED);
    return true;
  }
  return false;
}

void UserCreationScreen::ShowImpl() {
  if (!view_)
    return;

  scoped_observation_.Observe(network_state_informer_.get());

  LoginScreen::Get()->SetIsFirstSigninStep(true);

  // Back button is only available in login screen (add user flow) which is
  // indicated by if the device has users. Back button is hidden in the oobe
  // flow.
  view_->SetIsBackButtonVisible(
      LoginDisplayHost::default_host()->HasUserPods());

  UpdateState(NetworkError::ERROR_REASON_UPDATE);

  if (!error_screen_visible_)
    view_->Show();

  histogram_helper_->OnScreenShow();
}

void UserCreationScreen::HideImpl() {
  scoped_observation_.Reset();
  error_screen_visible_ = false;
  error_screen_->SetParentScreen(OOBE_SCREEN_UNKNOWN);
  error_screen_->Hide();
}

void UserCreationScreen::SetChildSetupStep() {
  if (!view_) {
    return;
  }
  view_->SetChildSetupStep();
}

void UserCreationScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionSignIn) {
    context()->sign_in_as_child = false;
    RunExitCallback(Result::SIGNIN);
  } else if (action_id == kUserActionAddChild) {
    RunExitCallback(Result::ADD_CHILD);
  } else if (action_id == kUserActionCancel) {
    context()->is_user_creation_enabled = false;
    RunExitCallback(Result::CANCEL);
  } else if (action_id == kUserActionEnroll) {
    RunExitCallback(Result::ENTERPRISE_ENROLL_TRIAGE);
  } else if (action_id == kUserActionTriage) {
    if (context()->is_add_person_flow) {
      RunExitCallback(Result::SIGNIN);
    } else {
      view_->SetTriageStep();
    }
  } else if (action_id == kUserActionSignInTriage) {
    RunExitCallback(Result::SIGNIN_TRIAGE);
  } else if (action_id == kUserActionChildSetup) {
    view_->SetChildSetupStep();
  } else if (action_id == kUserActionSignInSchool) {
    RunExitCallback(Result::SIGNIN_SCHOOL);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

bool UserCreationScreen::HandleAccelerator(LoginAcceleratorAction action) {
  if (action == LoginAcceleratorAction::kStartEnrollment) {
    RunExitCallback(Result::ENTERPRISE_ENROLL_SHORTCUT);
    return true;
  }
  if (action == LoginAcceleratorAction::kStartKioskEnrollment) {
    RunExitCallback(Result::KIOSK_ENTERPRISE_ENROLL);
    return true;
  }
  return false;
}

void UserCreationScreen::SetDefaultStep() {
  if (!view_) {
    return;
  }
  view_->SetDefaultStep();
}

void UserCreationScreen::UpdateState(NetworkError::ErrorReason reason) {
  NetworkStateInformer::State state = network_state_informer_->state();
  if (state != NetworkStateInformer::ONLINE ||
      reason == NetworkError::ERROR_REASON_LOADING_TIMEOUT) {
    error_screen_visible_ = true;
    error_screen_->SetParentScreen(UserCreationView::kScreenId);
    error_screen_->ShowNetworkErrorMessage(state, reason);
    histogram_helper_->OnErrorShow(error_screen_->GetErrorState());
  } else {
    error_screen_->HideCaptivePortal();
    if (error_screen_visible_ &&
        error_screen_->GetParentScreen() == UserCreationView::kScreenId) {
      error_screen_visible_ = false;
      error_screen_->SetParentScreen(OOBE_SCREEN_UNKNOWN);
      error_screen_->Hide();
      view_->Show();
      histogram_helper_->OnErrorHide();
    }
  }
}

void UserCreationScreen::RunExitCallback(Result result) {
  if (test_exit_delegate) {
    test_exit_delegate->OnUserCreationScreenExit(result, exit_callback_);
  } else {
    exit_callback_.Run(result);
  }
}

}  // namespace ash
