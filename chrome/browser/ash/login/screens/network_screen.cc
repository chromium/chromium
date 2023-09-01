// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/network_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

constexpr base::TimeDelta kConnectionTimeout = base::Seconds(40);

constexpr char kUserActionBackButtonClicked[] = "back";
constexpr char kUserActionContinueButtonClicked[] = "continue";
constexpr char kUserActionQuickStartButtonClicked[] = "activateQuickStart";

}  // namespace

// static
std::string NetworkScreen::GetResultString(Result result) {
  switch (result) {
    case Result::CONNECTED:
      return "Connected";
    case Result::BACK:
      return "Back";
    case Result::QUICK_START:
      return "QuickStart";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

NetworkScreen::NetworkScreen(base::WeakPtr<NetworkScreenView> view,
                             const ScreenExitCallback& exit_callback)
    : BaseScreen(NetworkScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback),
      network_state_helper_(std::make_unique<login::NetworkStateHelper>()) {}

NetworkScreen::~NetworkScreen() {
  connection_timer_.Stop();
  UnsubscribeNetworkNotification();
}

bool NetworkScreen::MaybeSkip(WizardContext& context) {
  // Skip this screen if the device is connected to Ethernet for the first time
  // in this session.
  return UpdateStatusIfConnectedToEthernet();
}

void NetworkScreen::ShowImpl() {
  Refresh();
  if (view_)
    view_->Show();

  // QuickStart should not be enabled for Demo mode or OS Install flows
  if (features::IsOobeQuickStartEnabled() &&
      !DemoSetupController::IsOobeDemoSetupFlowInProgress() &&
      !switches::IsOsInstallAllowed()) {
    // Determine the QuickStart button visibility
    WizardController::default_controller()
        ->quick_start_controller()
        ->IsSupported(
            base::BindOnce(&NetworkScreen::SetQuickStartButtonVisibility,
                           weak_ptr_factory_.GetWeakPtr()));
  }
}

void NetworkScreen::HideImpl() {
  connection_timer_.Stop();

  UnsubscribeNetworkNotification();
}

void NetworkScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionQuickStartButtonClicked) {
    OnQuickStartButtonClicked();
  } else if (action_id == kUserActionContinueButtonClicked) {
    OnContinueButtonClicked();
  } else if (action_id == kUserActionBackButtonClicked) {
    OnBackButtonClicked();
  } else {
    BaseScreen::OnUserAction(args);
  }
}

bool NetworkScreen::HandleAccelerator(LoginAcceleratorAction action) {
  if (action == LoginAcceleratorAction::kStartEnrollment) {
    context()->enrollment_triggered_early = true;
    return true;
  }

  return false;
}

void NetworkScreen::NetworkConnectionStateChanged(const NetworkState* network) {
  UpdateStatus();
}

void NetworkScreen::DefaultNetworkChanged(const NetworkState* network) {
  UpdateStatus();
}

void NetworkScreen::Refresh() {
  continue_pressed_ = false;
  SubscribeNetworkNotification();
  UpdateStatus();
}

void NetworkScreen::SetNetworkStateHelperForTest(
    login::NetworkStateHelper* helper) {
  network_state_helper_.reset(helper);
}

void NetworkScreen::SubscribeNetworkNotification() {
  if (!is_network_subscribed_) {
    is_network_subscribed_ = true;
    network_state_handler_observer_.Observe(
        NetworkHandler::Get()->network_state_handler());
  }
}

void NetworkScreen::UnsubscribeNetworkNotification() {
  if (is_network_subscribed_) {
    is_network_subscribed_ = false;
    network_state_handler_observer_.Reset();
  }
}

void NetworkScreen::NotifyOnConnection() {
  exit_callback_.Run(Result::CONNECTED);
}

void NetworkScreen::OnConnectionTimeout() {
  StopWaitingForConnection(network_id_);
  if (!network_state_helper_->IsConnected() && view_) {
    // Show error bubble.
    view_->ShowError(l10n_util::GetStringFUTF16(
        IDS_NETWORK_SELECTION_ERROR,
        l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME), network_id_));
  }
}

void NetworkScreen::UpdateStatus() {
  bool is_connected = network_state_helper_->IsConnected();

  if (view_ && is_connected)
    view_->ClearErrors();

  std::u16string network_name = network_state_helper_->GetCurrentNetworkName();
  if (is_connected)
    StopWaitingForConnection(network_name);
  else if (network_state_helper_->IsConnecting())
    WaitForConnection(network_name);
  else
    StopWaitingForConnection(network_id_);
}

void NetworkScreen::StopWaitingForConnection(const std::u16string& network_id) {
  bool is_connected = network_state_helper_->IsConnected();
  if (is_connected && continue_pressed_) {
    NotifyOnConnection();
    return;
  }

  connection_timer_.Stop();

  network_id_ = network_id;

  // Automatically continue if the device is connected to Ethernet for the first
  // time in this session.
  if (UpdateStatusIfConnectedToEthernet())
    return;

  // Automatically continue if we are using Zero-Touch Hands-Off Enrollment.
  if (is_connected && continue_attempts_ == 0 &&
      WizardController::IsZeroTouchHandsOffOobeFlow()) {
    OnContinueButtonClicked();
  }
}

void NetworkScreen::WaitForConnection(const std::u16string& network_id) {
  if (network_id_ != network_id || !connection_timer_.IsRunning()) {
    connection_timer_.Stop();
    connection_timer_.Start(FROM_HERE, kConnectionTimeout, this,
                            &NetworkScreen::OnConnectionTimeout);
  }

  network_id_ = network_id;
}

void NetworkScreen::OnBackButtonClicked() {
  if (view_)
    view_->ClearErrors();

  exit_callback_.Run(Result::BACK);
}

void NetworkScreen::OnContinueButtonClicked() {
  ++continue_attempts_;
  if (view_)
    view_->ClearErrors();

  if (network_state_helper_->IsConnected()) {
    NotifyOnConnection();
    return;
  }
  continue_pressed_ = true;
  WaitForConnection(network_id_);
}

void NetworkScreen::OnQuickStartButtonClicked() {
  CHECK(context()->quick_start_enabled);
  exit_callback_.Run(Result::QUICK_START);
}

void NetworkScreen::SetQuickStartButtonVisibility(bool visible) {
  if (visible && view_) {
    view_->SetQuickStartEnabled();
  }
}

bool NetworkScreen::UpdateStatusIfConnectedToEthernet() {
  if (switches::IsOOBENetworkScreenSkippingDisabledForTesting()) {
    return false;
  }

  if (!first_ethernet_connection_)
    return false;

  if (!network_state_helper_->IsConnectedToEthernet())
    return false;

  first_ethernet_connection_ = false;

  if (is_hidden()) {
    // Screen not shown yet: skipping it.
    exit_callback_.Run(Result::NOT_APPLICABLE);
  } else {
    // Screen already shown: automatically continuing.
    exit_callback_.Run(Result::CONNECTED);
  }

  return true;
}

}  // namespace ash
