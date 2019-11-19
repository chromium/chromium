// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/update_required_screen.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/system_tray.h"
#include "base/bind.h"
#include "base/time/default_clock.h"
#include "chrome/browser/chromeos/login/error_screens_histogram_helper.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/update_required_screen_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"

namespace {
constexpr char kUserActionSelectNetworkButtonClicked[] = "select-network";
constexpr char kUserActionUpdateButtonClicked[] = "update";
constexpr char kUserActionAcceptUpdateOverCellular[] = "update-accept-cellular";
constexpr char kUserActionRejectUpdateOverCellular[] = "update-reject-cellular";

// Delay before showing error message if captive portal is detected.
// We wait for this delay to let captive portal to perform redirect and show
// its login page before error message appears.
constexpr const base::TimeDelta kDelayErrorMessage =
    base::TimeDelta::FromSeconds(10);
}  // namespace

namespace chromeos {

UpdateRequiredScreen::UpdateRequiredScreen(UpdateRequiredView* view,
                                           ErrorScreen* error_screen)
    : BaseScreen(UpdateRequiredView::kScreenId),
      view_(view),
      error_screen_(error_screen),
      histogram_helper_(
          std::make_unique<ErrorScreensHistogramHelper>("UpdateRequired")),
      version_updater_(std::make_unique<VersionUpdater>(this)),
      network_state_helper_(std::make_unique<login::NetworkStateHelper>()),
      clock_(base::DefaultClock::GetInstance()) {
  if (view_)
    view_->Bind(this);
}

UpdateRequiredScreen::~UpdateRequiredScreen() {
  UnsubscribeNetworkNotification();
  if (view_)
    view_->Unbind();
}

void UpdateRequiredScreen::OnViewDestroyed(UpdateRequiredView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void UpdateRequiredScreen::Show() {
  ash::LoginScreen::Get()->SetAllowLoginAsGuest(false);
  RefreshNetworkState();
  SubscribeNetworkNotification();

  is_shown_ = true;

  if (first_time_shown_) {
    first_time_shown_ = false;
    if (view_) {
      view_->SetUIState(UpdateRequiredView::UPDATE_REQUIRED_MESSAGE);
      view_->Show();
    }
  }
  version_updater_->GetEolInfo(base::BindOnce(
      &UpdateRequiredScreen::OnGetEolInfo, weak_factory_.GetWeakPtr()));
}

void UpdateRequiredScreen::OnGetEolInfo(
    const chromeos::UpdateEngineClient::EolInfo& info) {
  //  TODO(crbug.com/1020616) : Handle if the device is left on this screen
  //  for long enough to reach Eol.
  if (!info.eol_date.is_null() && info.eol_date <= clock_->Now()) {
    EnsureScreenIsShown();
    if (view_)
      view_->SetUIState(UpdateRequiredView::EOL);
  }
}

void UpdateRequiredScreen::Hide() {
  if (view_)
    view_->Hide();
  is_shown_ = false;
  UnsubscribeNetworkNotification();
}

void UpdateRequiredScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionSelectNetworkButtonClicked) {
    OnSelectNetworkButtonClicked();
  } else if (action_id == kUserActionUpdateButtonClicked) {
    OnUpdateButtonClicked();
  } else if (action_id == kUserActionAcceptUpdateOverCellular) {
    version_updater_->SetUpdateOverCellularOneTimePermission();
  } else if (action_id == kUserActionRejectUpdateOverCellular) {
    version_updater_->RejectUpdateOverCellular();
    version_updater_->StartExitUpdate(VersionUpdater::Result::UPDATE_ERROR);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void UpdateRequiredScreen::NetworkConnectionStateChanged(
    const NetworkState* network) {
  RefreshNetworkState();
}

void UpdateRequiredScreen::DefaultNetworkChanged(const NetworkState* network) {
  RefreshNetworkState();
}

void UpdateRequiredScreen::RefreshNetworkState() {
  if (!view_)
    return;

  view_->SetIsConnected(network_state_helper_->IsConnected());
}

void UpdateRequiredScreen::RefreshView(
    const VersionUpdater::UpdateInfo& update_info) {
  if (!view_)
    return;

  if (update_info.requires_permission_for_cellular) {
    view_->SetUIState(UpdateRequiredView::UPDATE_NEED_PERMISSION);
    waiting_for_permission_ = true;
  } else if (waiting_for_permission_) {
    // Return UI state after getting permission.
    view_->SetUIState(UpdateRequiredView::UPDATE_PROCESS);
    waiting_for_permission_ = false;
  }

  view_->SetUpdateProgressUnavailable(update_info.progress_unavailable);
  view_->SetUpdateProgressValue(update_info.progress);
  view_->SetUpdateProgressMessage(update_info.progress_message);
  view_->SetEstimatedTimeLeftVisible(update_info.show_estimated_time_left);
  view_->SetEstimatedTimeLeft(update_info.estimated_time_left_in_secs);
}

void UpdateRequiredScreen::SubscribeNetworkNotification() {
  if (!is_network_subscribed_) {
    is_network_subscribed_ = true;
    NetworkHandler::Get()->network_state_handler()->AddObserver(this,
                                                                FROM_HERE);
  }
}

void UpdateRequiredScreen::UnsubscribeNetworkNotification() {
  if (is_network_subscribed_) {
    is_network_subscribed_ = false;
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
}

void UpdateRequiredScreen::OnSelectNetworkButtonClicked() {
  ash::SystemTray::Get()->ShowNetworkDetailedViewBubble(
      true /* show_by_click */);
}

void UpdateRequiredScreen::OnUpdateButtonClicked() {
  if (is_updating_now_)
    return;
  is_updating_now_ = true;
  if (view_)
    view_->SetUIState(UpdateRequiredView::UPDATE_PROCESS);

  version_updater_->StartNetworkCheck();
}

void UpdateRequiredScreen::OnWaitForRebootTimeElapsed() {
  EnsureScreenIsShown();
  if (view_)
    view_->SetUIState(UpdateRequiredView::UPDATE_COMPLETED_NEED_REBOOT);
}

void UpdateRequiredScreen::PrepareForUpdateCheck() {
  error_message_timer_.Stop();
  error_screen_->HideCaptivePortal();

  connect_request_subscription_.reset();
  if (version_updater_->update_info().state ==
      VersionUpdater::State::STATE_ERROR)
    HideErrorMessage();
}

void UpdateRequiredScreen::ShowErrorMessage() {
  error_message_timer_.Stop();

  is_shown_ = false;

  connect_request_subscription_ = error_screen_->RegisterConnectRequestCallback(
      base::BindRepeating(&UpdateRequiredScreen::OnConnectRequested,
                          weak_factory_.GetWeakPtr()));
  error_screen_->SetUIState(NetworkError::UI_STATE_UPDATE);
  error_screen_->SetParentScreen(UpdateRequiredView::kScreenId);
  error_screen_->SetHideCallback(base::BindRepeating(
      &UpdateRequiredScreen::OnErrorScreenHidden, weak_factory_.GetWeakPtr()));
  error_screen_->SetIsPersistentError(true /* is_persistent */);
  error_screen_->Show();
  histogram_helper_->OnErrorShow(error_screen_->GetErrorState());
}

void UpdateRequiredScreen::UpdateErrorMessage(
    const NetworkPortalDetector::CaptivePortalStatus status,
    const NetworkError::ErrorState& error_state,
    const std::string& network_name) {
  error_screen_->SetErrorState(error_state, network_name);
  if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL) {
    if (is_first_portal_notification_) {
      is_first_portal_notification_ = false;
      error_screen_->FixCaptivePortal();
    }
  }
}

void UpdateRequiredScreen::DelayErrorMessage() {
  if (error_message_timer_.IsRunning())
    return;

  error_message_timer_.Start(FROM_HERE, kDelayErrorMessage, this,
                             &UpdateRequiredScreen::ShowErrorMessage);
}

void UpdateRequiredScreen::UpdateInfoChanged(
    const VersionUpdater::UpdateInfo& update_info) {
  switch (update_info.status.current_operation()) {
    case update_engine::Operation::CHECKING_FOR_UPDATE:
    case update_engine::Operation::ATTEMPTING_ROLLBACK:
    case update_engine::Operation::DISABLED:
    case update_engine::Operation::IDLE:
      break;
    case update_engine::Operation::UPDATE_AVAILABLE:
    case update_engine::Operation::DOWNLOADING:
    case update_engine::Operation::VERIFYING:
    case update_engine::Operation::FINALIZING:
    case update_engine::Operation::NEED_PERMISSION_TO_UPDATE:
      EnsureScreenIsShown();
      break;
    case update_engine::Operation::UPDATED_NEED_REBOOT:
      EnsureScreenIsShown();
      waiting_for_reboot_ = true;
      version_updater_->RebootAfterUpdate();
      break;
    case update_engine::Operation::ERROR:
    case update_engine::Operation::REPORTING_ERROR_EVENT:
      version_updater_->StartExitUpdate(VersionUpdater::Result::UPDATE_ERROR);
      break;
    default:
      NOTREACHED();
  }
  RefreshView(update_info);
}

void UpdateRequiredScreen::FinishExitUpdate(VersionUpdater::Result result) {
  if (waiting_for_reboot_)
    return;

  is_updating_now_ = false;
  if (view_)
    view_->SetUIState(UpdateRequiredView::UPDATE_ERROR);
}

VersionUpdater* UpdateRequiredScreen::GetVersionUpdaterForTesting() {
  return version_updater_.get();
}

void UpdateRequiredScreen::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void UpdateRequiredScreen::EnsureScreenIsShown() {
  if (is_shown_ || !view_)
    return;

  is_shown_ = true;
  histogram_helper_->OnScreenShow();

  view_->Show();
}

void UpdateRequiredScreen::HideErrorMessage() {
  error_screen_->Hide();
  if (view_)
    view_->Show();
  histogram_helper_->OnErrorHide();
}

void UpdateRequiredScreen::OnConnectRequested() {
  if (version_updater_->update_info().state ==
      VersionUpdater::State::STATE_ERROR) {
    LOG(WARNING) << "Hiding error message since AP was reselected";
    version_updater_->StartUpdateCheck();
  }
}

void UpdateRequiredScreen::OnErrorScreenHidden() {
  error_screen_->SetParentScreen(OobeScreen::SCREEN_UNKNOWN);
  // Return to the default state.
  error_screen_->SetIsPersistentError(false /* is_persistent */);
  Show();
}

}  // namespace chromeos
