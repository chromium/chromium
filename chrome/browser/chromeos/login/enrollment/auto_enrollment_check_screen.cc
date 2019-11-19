// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_check_screen.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/error_screens_histogram_helper.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"

namespace chromeos {

namespace {

NetworkPortalDetector::CaptivePortalStatus GetCaptivePortalStatus() {
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  return default_network ? network_portal_detector::GetInstance()
                               ->GetCaptivePortalState(default_network->guid())
                               .status
                         : NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN;
}

}  // namespace

// static
AutoEnrollmentCheckScreen* AutoEnrollmentCheckScreen::Get(
    ScreenManager* manager) {
  return static_cast<AutoEnrollmentCheckScreen*>(
      manager->GetScreen(AutoEnrollmentCheckScreenView::kScreenId));
}

AutoEnrollmentCheckScreen::AutoEnrollmentCheckScreen(
    AutoEnrollmentCheckScreenView* view,
    ErrorScreen* error_screen,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(AutoEnrollmentCheckScreenView::kScreenId),
      view_(view),
      error_screen_(error_screen),
      exit_callback_(exit_callback),
      auto_enrollment_controller_(nullptr),
      captive_portal_status_(
          NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN),
      auto_enrollment_state_(policy::AUTO_ENROLLMENT_STATE_IDLE),
      histogram_helper_(new ErrorScreensHistogramHelper("Enrollment")) {
  if (view_)
    view_->SetDelegate(this);
}

AutoEnrollmentCheckScreen::~AutoEnrollmentCheckScreen() {
  network_portal_detector::GetInstance()->RemoveObserver(this);
  if (view_)
    view_->SetDelegate(NULL);
}

void AutoEnrollmentCheckScreen::ClearState() {
  auto_enrollment_progress_subscription_.reset();
  connect_request_subscription_.reset();
  network_portal_detector::GetInstance()->RemoveObserver(this);

  auto_enrollment_state_ = policy::AUTO_ENROLLMENT_STATE_IDLE;
  captive_portal_status_ = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN;
}

void AutoEnrollmentCheckScreen::Show() {
  // If the decision got made already, don't show the screen at all.
  if (!AutoEnrollmentController::IsEnabled() || IsCompleted()) {
    SignalCompletion();
    return;
  }

  // Start from a clean slate.
  ClearState();

  // Bring up the screen. It's important to do this before updating the UI,
  // because the latter may switch to the error screen, which needs to stay on
  // top.
  view_->Show();
  histogram_helper_->OnScreenShow();

  // Set up state change observers.
  auto_enrollment_progress_subscription_ =
      auto_enrollment_controller_->RegisterProgressCallback(base::Bind(
          &AutoEnrollmentCheckScreen::OnAutoEnrollmentCheckProgressed,
          base::Unretained(this)));
  network_portal_detector::GetInstance()->AddObserver(this);

  // Perform an initial UI update.
  NetworkPortalDetector::CaptivePortalStatus new_captive_portal_status =
      GetCaptivePortalStatus();
  policy::AutoEnrollmentState new_auto_enrollment_state =
      auto_enrollment_controller_->state();

  if (!UpdateCaptivePortalStatus(new_captive_portal_status))
    UpdateAutoEnrollmentState(new_auto_enrollment_state);

  captive_portal_status_ = new_captive_portal_status;
  auto_enrollment_state_ = new_auto_enrollment_state;

  // Make sure gears are in motion in the background.
  // Note that if a previous auto-enrollment check ended with a failure,
  // IsCompleted() would still return false, and Show would not report result
  // early. In that case auto-enrollment check should be retried.
  if (auto_enrollment_controller_->state() ==
          policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR ||
      auto_enrollment_controller_->state() ==
          policy::AUTO_ENROLLMENT_STATE_SERVER_ERROR) {
    auto_enrollment_controller_->Retry();
  } else {
    auto_enrollment_controller_->Start();
  }
  network_portal_detector::GetInstance()->StartPortalDetection(
      false /* force */);
}

void AutoEnrollmentCheckScreen::Hide() {}

void AutoEnrollmentCheckScreen::OnViewDestroyed(
    AutoEnrollmentCheckScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void AutoEnrollmentCheckScreen::OnPortalDetectionCompleted(
    const NetworkState* /* network */,
    const NetworkPortalDetector::CaptivePortalState& /* state */) {
  UpdateState();
}

void AutoEnrollmentCheckScreen::OnAutoEnrollmentCheckProgressed(
    policy::AutoEnrollmentState state) {
  if (IsCompleted()) {
    SignalCompletion();
    return;
  }

  UpdateState();
}

void AutoEnrollmentCheckScreen::UpdateState() {
  NetworkPortalDetector::CaptivePortalStatus new_captive_portal_status =
      GetCaptivePortalStatus();
  policy::AutoEnrollmentState new_auto_enrollment_state =
      auto_enrollment_controller_->state();

  // Configure the error screen to show the appropriate error message.
  if (!UpdateCaptivePortalStatus(new_captive_portal_status))
    UpdateAutoEnrollmentState(new_auto_enrollment_state);

  // Update the connecting indicator.
  error_screen_->ShowConnectingIndicator(new_auto_enrollment_state ==
                                         policy::AUTO_ENROLLMENT_STATE_PENDING);

  // Determine whether a retry is in order.
  bool retry = (new_captive_portal_status ==
                NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE) &&
               (captive_portal_status_ !=
                NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);

  // Save the new state.
  captive_portal_status_ = new_captive_portal_status;
  auto_enrollment_state_ = new_auto_enrollment_state;

  // Retry if applicable. This is last so eventual callbacks find consistent
  // state.
  if (retry)
    auto_enrollment_controller_->Retry();
}

bool AutoEnrollmentCheckScreen::UpdateCaptivePortalStatus(
    NetworkPortalDetector::CaptivePortalStatus new_captive_portal_status) {
  switch (new_captive_portal_status) {
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN:
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE:
      return false;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE:
      ShowErrorScreen(NetworkError::ERROR_STATE_OFFLINE);
      return true;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL:
      ShowErrorScreen(NetworkError::ERROR_STATE_PORTAL);
      if (captive_portal_status_ != new_captive_portal_status)
        error_screen_->FixCaptivePortal();
      return true;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED:
      ShowErrorScreen(NetworkError::ERROR_STATE_PROXY);
      return true;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT:
      NOTREACHED() << "Bad status: CAPTIVE_PORTAL_STATUS_COUNT";
      return false;
  }

  // Return is required to avoid compiler warning.
  NOTREACHED() << "Bad status " << new_captive_portal_status;
  return false;
}

bool AutoEnrollmentCheckScreen::UpdateAutoEnrollmentState(
    policy::AutoEnrollmentState new_auto_enrollment_state) {
  switch (new_auto_enrollment_state) {
    case policy::AUTO_ENROLLMENT_STATE_IDLE:
    case policy::AUTO_ENROLLMENT_STATE_PENDING:
    case policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT:
    case policy::AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH:
    case policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT:
    case policy::AUTO_ENROLLMENT_STATE_DISABLED:
      return false;
    case policy::AUTO_ENROLLMENT_STATE_SERVER_ERROR:
      if (!ShouldBlockOnServerError())
        return false;

      // Fall to the same behavior like any connection error if the device is
      // enrolled.
      FALLTHROUGH;
    case policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR:
      ShowErrorScreen(NetworkError::ERROR_STATE_OFFLINE);
      return true;
  }

  // Return is required to avoid compiler warning.
  NOTREACHED() << "bad state " << new_auto_enrollment_state;
  return false;
}

void AutoEnrollmentCheckScreen::ShowErrorScreen(
    NetworkError::ErrorState error_state) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  error_screen_->SetUIState(NetworkError::UI_STATE_AUTO_ENROLLMENT_ERROR);
  error_screen_->AllowGuestSignin(
      auto_enrollment_controller_->GetFRERequirement() !=
      AutoEnrollmentController::FRERequirement::kExplicitlyRequired);
  error_screen_->SetErrorState(error_state,
                               network ? network->name() : std::string());
  connect_request_subscription_ = error_screen_->RegisterConnectRequestCallback(
      base::Bind(&AutoEnrollmentCheckScreen::OnConnectRequested,
                 base::Unretained(this)));
  error_screen_->SetHideCallback(
      base::BindRepeating(&AutoEnrollmentCheckScreen::OnErrorScreenHidden,
                          weak_ptr_factory_.GetWeakPtr()));
  error_screen_->SetParentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  error_screen_->Show();
  histogram_helper_->OnErrorShow(error_state);
}

void AutoEnrollmentCheckScreen::OnErrorScreenHidden() {
  error_screen_->SetParentScreen(OobeScreen::SCREEN_UNKNOWN);
  Show();
}

void AutoEnrollmentCheckScreen::SignalCompletion() {
  network_portal_detector::GetInstance()->RemoveObserver(this);
  auto_enrollment_progress_subscription_.reset();
  connect_request_subscription_.reset();

  // Running exit callback can cause |this| destruction, so let other methods
  // finish their work before.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AutoEnrollmentCheckScreen::RunExitCallback,
                                weak_ptr_factory_.GetWeakPtr()));
}

bool AutoEnrollmentCheckScreen::IsCompleted() const {
  switch (auto_enrollment_controller_->state()) {
    case policy::AUTO_ENROLLMENT_STATE_IDLE:
    case policy::AUTO_ENROLLMENT_STATE_PENDING:
    case policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR:
      return false;
    case policy::AUTO_ENROLLMENT_STATE_SERVER_ERROR:
      // Server errors should block OOBE for enrolled devices.
      return !ShouldBlockOnServerError();
    case policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT:
    case policy::AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH:
    case policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT:
    case policy::AUTO_ENROLLMENT_STATE_DISABLED:
      // Decision made, ready to proceed.
      return true;
  }
  NOTREACHED();
  return false;
}

void AutoEnrollmentCheckScreen::OnConnectRequested() {
  auto_enrollment_controller_->Retry();
}

bool AutoEnrollmentCheckScreen::ShouldBlockOnServerError() const {
  switch (auto_enrollment_controller_->auto_enrollment_check_type()) {
    case AutoEnrollmentController::AutoEnrollmentCheckType::kFRE:
      // Only block on errors in FRE if FRE is expliclty required (i.e. the
      // device was enrolled before).
      return auto_enrollment_controller_->GetFRERequirement() ==
             AutoEnrollmentController::FRERequirement::kExplicitlyRequired;
    case AutoEnrollmentController::AutoEnrollmentCheckType::kInitialEnrollment:
      return true;
    case AutoEnrollmentController::AutoEnrollmentCheckType::kNone:
      NOTREACHED();
      return false;
  }
}

}  // namespace chromeos
