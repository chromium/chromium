// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/add_child_screen.h"

#include <string>

#include "ash/public/cpp/login_screen.h"
#include "chrome/browser/ash/login/error_screens_histogram_helper.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/add_child_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {
namespace {

constexpr char kUserActionBack[] = "child-back";
constexpr char kUserActionChildSignIn[] = "child-signin";
constexpr char kUserActionChildAccountCreate[] = "child-account-create";

}  // namespace

// static
std::string AddChildScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::CHILD_SIGNIN:
      return "SignInAsChild";
    case Result::CHILD_ACCOUNT_CREATE:
      return "CreateChildAccount";
    case Result::ENTERPRISE_ENROLL:
      return "EnterpriseEnroll";
    case Result::KIOSK_ENTERPRISE_ENROLL:
      return "KioskEnterpriseEnroll";
    case Result::BACK:
      return "Back";
    case Result::SKIPPED:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

AddChildScreen::AddChildScreen(base::WeakPtr<AddChildScreenView> view,
                               ErrorScreen* error_screen,
                               const ScreenExitCallback& exit_callback)
    : BaseScreen(AddChildScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      histogram_helper_(std::make_unique<ErrorScreensHistogramHelper>(
          ErrorScreensHistogramHelper::ErrorParentScreen::kAddChild)),
      error_screen_(error_screen),
      exit_callback_(exit_callback) {
  network_state_informer_ = base::MakeRefCounted<NetworkStateInformer>();
  network_state_informer_->Init();
}

AddChildScreen::~AddChildScreen() = default;

bool AddChildScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_to_login_for_tests) {
    exit_callback_.Run(Result::SKIPPED);
    return true;
  }

  return false;
}

void AddChildScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  scoped_observation_.Observe(network_state_informer_.get());

  // Fixing the Guest shelfButton
  // Todo(b/291766001) Nuke is_first_signin_step_ in LoginshelfView
  LoginScreen::Get()->SetIsFirstSigninStep(true);

  UpdateState(NetworkError::ERROR_REASON_UPDATE);

  if (!error_screen_visible_) {
    view_->Show();
  }

  histogram_helper_->OnScreenShow();
}

void AddChildScreen::HideImpl() {
  scoped_observation_.Reset();
  error_screen_visible_ = false;
  error_screen_->SetParentScreen(OOBE_SCREEN_UNKNOWN);
  error_screen_->Hide();
}

void AddChildScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionChildSignIn) {
    context()->sign_in_as_child = true;
    context()->is_child_gaia_account_new = false;
    exit_callback_.Run(Result::CHILD_SIGNIN);
  } else if (action_id == kUserActionChildAccountCreate) {
    context()->sign_in_as_child = true;
    context()->is_child_gaia_account_new = true;
    exit_callback_.Run(Result::CHILD_ACCOUNT_CREATE);
  } else if (action_id == kUserActionBack) {
    exit_callback_.Run(Result::BACK);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

bool AddChildScreen::HandleAccelerator(LoginAcceleratorAction action) {
  if (action == LoginAcceleratorAction::kStartEnrollment) {
    exit_callback_.Run(Result::ENTERPRISE_ENROLL);
    return true;
  }
  if (action == LoginAcceleratorAction::kStartKioskEnrollment) {
    exit_callback_.Run(Result::KIOSK_ENTERPRISE_ENROLL);
    return true;
  }
  return false;
}

void AddChildScreen::UpdateState(NetworkError::ErrorReason reason) {
  NetworkStateInformer::State state = network_state_informer_->state();
  if (state != NetworkStateInformer::ONLINE ||
      reason == NetworkError::ERROR_REASON_LOADING_TIMEOUT) {
    error_screen_visible_ = true;
    error_screen_->SetParentScreen(AddChildScreenView::kScreenId);
    error_screen_->ShowNetworkErrorMessage(state, reason);
    histogram_helper_->OnErrorShow(error_screen_->GetErrorState());
  } else {
    error_screen_->HideCaptivePortal();
    if (error_screen_visible_ &&
        error_screen_->GetParentScreen() == AddChildScreenView::kScreenId) {
      error_screen_visible_ = false;
      error_screen_->SetParentScreen(OOBE_SCREEN_UNKNOWN);
      error_screen_->Hide();
      view_->Show();
      histogram_helper_->OnErrorHide();
    }
  }
}

}  // namespace ash
