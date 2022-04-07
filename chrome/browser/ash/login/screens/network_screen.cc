// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/network_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/location.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

constexpr base::TimeDelta kConnectionTimeout = base::Seconds(40);

constexpr char kUserActionBackButtonClicked[] = "back";
constexpr char kUserActionContinueButtonClicked[] = "continue";

}  // namespace

// static
std::string NetworkScreen::GetResultString(Result result) {
  switch (result) {
    case Result::CONNECTED_REGULAR:
    case Result::CONNECTED_DEMO:
    case Result::CONNECTED_REGULAR_CONSOLIDATED_CONSENT:
    case Result::CONNECTED_DEMO_CONSOLIDATED_CONSENT:
      return "Connected";
    case Result::OFFLINE_DEMO:
      return "OfflineDemoSetup";
    case Result::BACK_REGULAR:
    case Result::BACK_DEMO:
    case Result::BACK_OS_INSTALL:
      return "Back";
    case Result::NOT_APPLICABLE:
    case Result::NOT_APPLICABLE_CONSOLIDATED_CONSENT:
      return BaseScreen::kNotApplicable;
  }
}

NetworkScreen::NetworkScreen(NetworkScreenView* view,
                             const ScreenExitCallback& exit_callback)
    : BaseScreen(NetworkScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback),
      network_state_helper_(std::make_unique<login::NetworkStateHelper>()) {
  if (view_)
    view_->Bind(this);
}

NetworkScreen::~NetworkScreen() {
  if (view_)
    view_->Unbind();
  connection_timer_.Stop();
  UnsubscribeNetworkNotification();
}

void NetworkScreen::OnViewDestroyed(NetworkScreenView* view) {
  if (view_ == view) {
    view_ = nullptr;
    // Ownership of NetworkScreen is complicated; ensure that we remove
    // this as a NetworkStateHandler observer when the view is destroyed.
    UnsubscribeNetworkNotification();
  }
}

bool NetworkScreen::MaybeSkip(WizardContext* context) {
  // Skip this screen if the device is connected to Ethernet for the first time
  // in this session.
  return UpdateStatusIfConnectedToEthernet();
}

void NetworkScreen::ShowImpl() {
  Refresh();
  if (view_)
    view_->Show();
}

void NetworkScreen::HideImpl() {
  if (view_)
    view_->Hide();
  connection_timer_.Stop();
  UnsubscribeNetworkNotification();
}

void NetworkScreen::OnUserActionDeprecated(const std::string& action_id) {
  if (action_id == kUserActionContinueButtonClicked) {
    OnContinueButtonClicked();
  } else if (action_id == kUserActionBackButtonClicked) {
    OnBackButtonClicked();
  } else {
    BaseScreen::OnUserActionDeprecated(action_id);
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
    NetworkHandler::Get()->network_state_handler()->AddObserver(this,
                                                                FROM_HERE);
  }
}

void NetworkScreen::UnsubscribeNetworkNotification() {
  if (is_network_subscribed_) {
    is_network_subscribed_ = false;
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
}

void NetworkScreen::NotifyOnConnection() {
  if (DemoSetupController::IsOobeDemoSetupFlowInProgress()) {
    if (chromeos::features::IsOobeConsolidatedConsentEnabled())
      exit_callback_.Run(Result::CONNECTED_DEMO_CONSOLIDATED_CONSENT);
    else
      exit_callback_.Run(Result::CONNECTED_DEMO);
  } else {
    if (chromeos::features::IsOobeConsolidatedConsentEnabled())
      exit_callback_.Run(Result::CONNECTED_REGULAR_CONSOLIDATED_CONSENT);
    else
      exit_callback_.Run(Result::CONNECTED_REGULAR);
  }
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
  if (!view_)
    return;

  bool is_connected = network_state_helper_->IsConnected();
  if (is_connected)
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

  if (DemoSetupController::IsOobeDemoSetupFlowInProgress())
    exit_callback_.Run(Result::BACK_DEMO);
  else if (switches::IsOsInstallAllowed())
    exit_callback_.Run(Result::BACK_OS_INSTALL);
  else
    exit_callback_.Run(Result::BACK_REGULAR);
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

bool NetworkScreen::UpdateStatusIfConnectedToEthernet() {
  if (!features::IsOobeNetworkScreenSkipEnabled() ||
      switches::IsOOBENetworkScreenSkippingDisabledForTesting()) {
    return false;
  }

  if (!first_ethernet_connection_)
    return false;

  if (!network_state_helper_->IsConnectedToEthernet())
    return false;

  first_ethernet_connection_ = false;

  if (is_hidden()) {
    // Screen not shown yet: skipping it.
    if (chromeos::features::IsOobeConsolidatedConsentEnabled()) {
      exit_callback_.Run(Result::NOT_APPLICABLE_CONSOLIDATED_CONSENT);
    } else {
      exit_callback_.Run(Result::NOT_APPLICABLE);
    }
  } else {
    // Screen already shown: automatically continuing.
    if (chromeos::features::IsOobeConsolidatedConsentEnabled()) {
      exit_callback_.Run(Result::CONNECTED_REGULAR_CONSOLIDATED_CONSENT);
    } else {
      exit_callback_.Run(Result::CONNECTED_REGULAR);
    }
  }

  return true;
}

}  // namespace ash
