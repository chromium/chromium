// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/auto_enrollment_check_screen.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/login/error_screens_histogram_helper.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

// static
std::string AutoEnrollmentCheckScreen::GetResultString(Result result) {
  switch (result) {
    case Result::NEXT:
      return "Next";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

AutoEnrollmentCheckScreen::AutoEnrollmentCheckScreen(
    base::WeakPtr<AutoEnrollmentCheckScreenView> view,
    ErrorScreen* error_screen,
    const base::RepeatingCallback<void(Result result)>& exit_callback)
    : BaseScreen(AutoEnrollmentCheckScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      error_screen_(error_screen),
      exit_callback_(exit_callback),
      histogram_helper_(new ErrorScreensHistogramHelper(
          ErrorScreensHistogramHelper::ErrorParentScreen::kEnrollment)) {}

AutoEnrollmentCheckScreen::~AutoEnrollmentCheckScreen() {
  if (NetworkHandler::IsInitialized())
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
}

void AutoEnrollmentCheckScreen::ClearState() {
  auto_enrollment_progress_subscription_ = {};
  connect_request_subscription_ = {};
  NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);

  captive_portal_state_ = NetworkState::PortalState::kUnknown;
}

void AutoEnrollmentCheckScreen::ShowImpl() {
  // Start from a clean slate.
  ClearState();

  // Bring up the screen. It's important to do this before updating the UI,
  // because the latter may switch to the error screen, which needs to stay on
  // top.
  if (view_)
    view_->Show();
  histogram_helper_->OnScreenShow();

  // Set up state change observers.
  auto_enrollment_progress_subscription_ =
      auto_enrollment_controller_->RegisterProgressCallback(base::BindRepeating(
          &AutoEnrollmentCheckScreen::OnAutoEnrollmentCheckProgressed,
          base::Unretained(this)));

  NetworkState::PortalState new_captive_portal_state =
      NetworkState::PortalState::kUnknown;

  NetworkStateHandler* network_state_handler =
      NetworkHandler::Get()->network_state_handler();
  network_state_handler->AddObserver(this);
  const NetworkState* default_network = network_state_handler->DefaultNetwork();
  new_captive_portal_state = default_network
                                 ? default_network->GetPortalState()
                                 : NetworkState::PortalState::kUnknown;
  network_state_handler->RequestPortalDetection();

  // Perform an initial UI update.
  if (!UpdateCaptivePortalState(new_captive_portal_state))
    UpdateAutoEnrollmentState(auto_enrollment_controller_->state());

  captive_portal_state_ = new_captive_portal_state;

  // Make sure gears are in motion in the background.
  // Note that if a previous auto-enrollment check ended with a failure,
  // IsCompleted() would still return false, and Show would not report result
  // early. In that case auto-enrollment check should be retried.
  if (auto_enrollment_controller_->state() ==
          policy::AutoEnrollmentState::kConnectionError ||
      auto_enrollment_controller_->state() ==
          policy::AutoEnrollmentState::kServerError) {
    // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
    // in the logs.
    LOG(WARNING) << "AutoEnrollmentCheckScreen::ShowImpl() retrying enrollment"
                 << " check due to failure.";
    auto_enrollment_controller_->Retry();
  } else {
    auto_enrollment_controller_->Start();
  }
}

void AutoEnrollmentCheckScreen::HideImpl() {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
  }
}

bool AutoEnrollmentCheckScreen::MaybeSkip(WizardContext& context) {
  // If the decision got made already, don't show the screen at all.
  if (!policy::AutoEnrollmentTypeChecker::IsEnabled() || IsCompleted()) {
    RunExitCallback(Result::NOT_APPLICABLE);
    return true;
  }
  return false;
}

void AutoEnrollmentCheckScreen::PortalStateChanged(
    const NetworkState* /*default_network*/,
    const NetworkState::PortalState portal_state) {
  UpdateState(portal_state);
}

void AutoEnrollmentCheckScreen::OnShuttingDown() {
  NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
}

void AutoEnrollmentCheckScreen::OnAutoEnrollmentCheckProgressed(
    policy::AutoEnrollmentState state) {
  if (IsCompleted()) {
    SignalCompletion();
    return;
  }
  UpdateState(captive_portal_state_);
}

void AutoEnrollmentCheckScreen::UpdateState(
    NetworkState::PortalState new_captive_portal_state) {
  policy::AutoEnrollmentState new_auto_enrollment_state =
      auto_enrollment_controller_->state();

  // Configure the error screen to show the appropriate error message.
  if (!UpdateCaptivePortalState(new_captive_portal_state))
    UpdateAutoEnrollmentState(new_auto_enrollment_state);

  // Update the connecting indicator.
  error_screen_->ShowConnectingIndicator(new_auto_enrollment_state ==
                                         policy::AutoEnrollmentState::kPending);

  // Determine whether a retry is in order.
  bool retry =
      (new_captive_portal_state == NetworkState::PortalState::kOnline) &&
      (captive_portal_state_ != NetworkState::PortalState::kOnline);

  // Save the new state.
  captive_portal_state_ = new_captive_portal_state;

  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "AutoEnrollmentCheckScreen::UpdateState() retry = " << retry;

  // Retry if applicable. This is last so eventual callbacks find consistent
  // state.
  if (retry)
    auto_enrollment_controller_->Retry();
}

bool AutoEnrollmentCheckScreen::UpdateCaptivePortalState(
    NetworkState::PortalState new_captive_portal_state) {
  switch (new_captive_portal_state) {
    case NetworkState::PortalState::kUnknown:
      [[fallthrough]];
    case NetworkState::PortalState::kOnline:
      return false;
    case NetworkState::PortalState::kNoInternet:
      ShowErrorScreen(NetworkError::ERROR_STATE_OFFLINE);
      return true;
    case NetworkState::PortalState::kPortal:
      [[fallthrough]];
    case NetworkState::PortalState::kPortalSuspected:
      ShowErrorScreen(NetworkError::ERROR_STATE_PORTAL);
      if (captive_portal_state_ != new_captive_portal_state)
        error_screen_->FixCaptivePortal();
      return true;
    case NetworkState::PortalState::kProxyAuthRequired:
      ShowErrorScreen(NetworkError::ERROR_STATE_PROXY);
      return true;
  }
}

bool AutoEnrollmentCheckScreen::UpdateAutoEnrollmentState(
    policy::AutoEnrollmentState new_auto_enrollment_state) {
  switch (new_auto_enrollment_state) {
    case policy::AutoEnrollmentState::kIdle:
    case policy::AutoEnrollmentState::kPending:
    case policy::AutoEnrollmentState::kEnrollment:
    case policy::AutoEnrollmentState::kNoEnrollment:
    case policy::AutoEnrollmentState::kDisabled:
      return false;
    case policy::AutoEnrollmentState::kServerError:
      if (!ShouldBlockOnServerError())
        return false;

      // Fall to the same behavior like any connection error if the device is
      // enrolled.
      [[fallthrough]];
    case policy::AutoEnrollmentState::kConnectionError:
      ShowErrorScreen(NetworkError::ERROR_STATE_OFFLINE);
      return true;
  }
}

void AutoEnrollmentCheckScreen::ShowErrorScreen(
    NetworkError::ErrorState error_state) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  error_screen_->SetUIState(NetworkError::UI_STATE_AUTO_ENROLLMENT_ERROR);
  error_screen_->AllowGuestSignin(
      auto_enrollment_controller_->auto_enrollment_check_type() !=
      policy::AutoEnrollmentTypeChecker::CheckType::
          kForcedReEnrollmentExplicitlyRequired);

  error_screen_->SetErrorState(error_state,
                               network ? network->name() : std::string());
  connect_request_subscription_ = error_screen_->RegisterConnectRequestCallback(
      base::BindRepeating(&AutoEnrollmentCheckScreen::OnConnectRequested,
                          base::Unretained(this)));
  error_screen_->SetHideCallback(
      base::BindOnce(&AutoEnrollmentCheckScreen::OnErrorScreenHidden,
                     weak_ptr_factory_.GetWeakPtr()));
  error_screen_->SetParentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  error_screen_->Show(context());
  histogram_helper_->OnErrorShow(error_state);
}

void AutoEnrollmentCheckScreen::OnErrorScreenHidden() {
  error_screen_->SetParentScreen(OOBE_SCREEN_UNKNOWN);
  Show(context());
}

void AutoEnrollmentCheckScreen::SignalCompletion() {
  VLOG(1) << "AutoEnrollmentCheckScreen::SignalCompletion()";

  if (NetworkHandler::IsInitialized())
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
  error_screen_->SetHideCallback(base::OnceClosure());
  error_screen_->SetParentScreen(OOBE_SCREEN_UNKNOWN);
  auto_enrollment_progress_subscription_ = {};
  connect_request_subscription_ = {};

  // Running exit callback can cause `this` destruction, so let other methods
  // finish their work before.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AutoEnrollmentCheckScreen::RunExitCallback,
                                weak_ptr_factory_.GetWeakPtr(), Result::NEXT));
}

bool AutoEnrollmentCheckScreen::IsCompleted() const {
  switch (auto_enrollment_controller_->state()) {
    case policy::AutoEnrollmentState::kIdle:
    case policy::AutoEnrollmentState::kPending:
    case policy::AutoEnrollmentState::kConnectionError:
      return false;
    case policy::AutoEnrollmentState::kServerError:
      // Server errors should block OOBE for enrolled devices.
      return !ShouldBlockOnServerError();
    case policy::AutoEnrollmentState::kEnrollment:
    case policy::AutoEnrollmentState::kNoEnrollment:
    case policy::AutoEnrollmentState::kDisabled:
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
  using CheckType = policy::AutoEnrollmentTypeChecker::CheckType;
  switch (auto_enrollment_controller_->auto_enrollment_check_type()) {
    case CheckType::kForcedReEnrollmentImplicitlyRequired:
      // Auto-enrollment is implicitly required so we don't block in server
      // errors.
      return false;
    case CheckType::kForcedReEnrollmentExplicitlyRequired:
    case CheckType::kInitialStateDetermination:
      // Auto-enrollment is explicitly required so we block on server errors.
      return true;
    case CheckType::kUnknownDueToMissingSystemClockSync:
    case CheckType::kNone:
      NOTREACHED();
      return false;
  }
}

}  // namespace ash
