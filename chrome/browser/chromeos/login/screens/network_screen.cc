// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/network_screen.h"

#include "base/bind.h"
#include "base/location.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr base::TimeDelta kConnectionTimeout = base::TimeDelta::FromSeconds(40);

constexpr char kUserActionBackButtonClicked[] = "back";
constexpr char kUserActionContinueButtonClicked[] = "continue";
constexpr char kUserActionOfflineDemoSetup[] = "offline-demo-setup";

}  // namespace

namespace chromeos {

// static
NetworkScreen* NetworkScreen::Get(ScreenManager* manager) {
  return static_cast<NetworkScreen*>(
      manager->GetScreen(NetworkScreenView::kScreenId));
}

NetworkScreen::NetworkScreen(NetworkScreenView* view,
                             const ScreenExitCallback& exit_callback)
    : BaseScreen(NetworkScreenView::kScreenId),
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

void NetworkScreen::Show() {
  if (DemoSetupController::IsOobeDemoSetupFlowInProgress()) {
    // Check if preinstalled resources are available. If so, we can allow
    // offline Demo Mode during Demo Mode network selection.
    DemoSetupController* demo_setup_controller =
        WizardController::default_controller()->demo_setup_controller();
    demo_setup_controller->TryMountPreinstalledDemoResources(
        base::BindOnce(&NetworkScreen::OnHasPreinstalledDemoResources,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  Refresh();
  if (view_)
    view_->Show();
}

void NetworkScreen::Hide() {
  if (view_)
    view_->Hide();
}

void NetworkScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionContinueButtonClicked) {
    OnContinueButtonClicked();
  } else if (action_id == kUserActionBackButtonClicked) {
    OnBackButtonClicked();
  } else if (action_id == kUserActionOfflineDemoSetup) {
    OnOfflineDemoModeSetupSelected();
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void NetworkScreen::NetworkConnectionStateChanged(const NetworkState* network) {
  UpdateStatus();
}

void NetworkScreen::DefaultNetworkChanged(const NetworkState* network) {
  UpdateStatus();
}

void NetworkScreen::Refresh() {
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
  // TODO(nkostylev): Check network connectivity.
  UnsubscribeNetworkNotification();
  connection_timer_.Stop();
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
  if (!view_)
    return;

  bool is_connected = network_state_helper_->IsConnected();
  if (is_connected)
    view_->ClearErrors();

  base::string16 network_name = network_state_helper_->GetCurrentNetworkName();
  if (is_connected)
    StopWaitingForConnection(network_name);
  else if (network_state_helper_->IsConnecting())
    WaitForConnection(network_name);
  else
    StopWaitingForConnection(network_id_);
}

void NetworkScreen::StopWaitingForConnection(const base::string16& network_id) {
  bool is_connected = network_state_helper_->IsConnected();
  if (is_connected && continue_pressed_) {
    NotifyOnConnection();
    return;
  }

  continue_pressed_ = false;
  connection_timer_.Stop();

  network_id_ = network_id;
  if (view_)
    view_->ShowConnectingStatus(false, network_id_);

  // Automatically continue if we are using Hands-Off Enrollment.
  if (is_connected && continue_attempts_ == 0 &&
      WizardController::UsingHandsOffEnrollment()) {
    OnContinueButtonClicked();
  }
}

void NetworkScreen::WaitForConnection(const base::string16& network_id) {
  if (network_id_ != network_id || !connection_timer_.IsRunning()) {
    connection_timer_.Stop();
    connection_timer_.Start(FROM_HERE, kConnectionTimeout, this,
                            &NetworkScreen::OnConnectionTimeout);
  }

  network_id_ = network_id;
  if (view_)
    view_->ShowConnectingStatus(continue_pressed_, network_id_);
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
  } else {
    continue_pressed_ = true;
    WaitForConnection(network_id_);
  }
}

void NetworkScreen::OnHasPreinstalledDemoResources(
    bool has_preinstalled_demo_resources) {
  if (view_)
    view_->SetOfflineDemoModeEnabled(has_preinstalled_demo_resources);
}

void NetworkScreen::OnOfflineDemoModeSetupSelected() {
  DCHECK(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  if (view_)
    view_->ClearErrors();
  exit_callback_.Run(Result::OFFLINE_DEMO_SETUP);
}

}  // namespace chromeos
