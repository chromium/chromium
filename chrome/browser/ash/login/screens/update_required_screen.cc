// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/update_required_screen.h"

#include <algorithm>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/system_tray.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/time/default_clock.h"
#include "chrome/browser/ash/login/error_screens_histogram_helper.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/update_required_screen_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/chromeos/devicetype_utils.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

namespace {

constexpr char kUserActionSelectNetworkButtonClicked[] = "select-network";
constexpr char kUserActionUpdateButtonClicked[] = "update";
constexpr char kUserActionAcceptUpdateOverCellular[] = "update-accept-cellular";
constexpr char kUserActionRejectUpdateOverCellular[] = "update-reject-cellular";
constexpr char kUserActionConfirmDeleteUsersData[] = "confirm-delete-users";

// Delay before showing error message if captive portal is detected.
// We wait for this delay to let captive portal to perform redirect and show
// its login page before error message appears.
constexpr const base::TimeDelta kDelayErrorMessage = base::Seconds(10);

}  // namespace

UpdateRequiredScreen::UpdateRequiredScreen(
    base::WeakPtr<UpdateRequiredView> view,
    ErrorScreen* error_screen,
    base::RepeatingClosure exit_callback)
    : BaseScreen(UpdateRequiredView::kScreenId,
                 OobeScreenPriority::SCREEN_UPDATE_REQUIRED),
      view_(std::move(view)),
      error_screen_(error_screen),
      exit_callback_(std::move(exit_callback)),
      histogram_helper_(std::make_unique<ErrorScreensHistogramHelper>(
          ErrorScreensHistogramHelper::ErrorParentScreen::kUpdateRequired)),
      version_updater_(std::make_unique<VersionUpdater>(this)),
      clock_(base::DefaultClock::GetInstance()) {
  error_message_delay_ = kDelayErrorMessage;

  eol_message_subscription_ = CrosSettings::Get()->AddSettingsObserver(
      kDeviceMinimumVersionAueMessage,
      base::BindRepeating(&UpdateRequiredScreen::OnEolMessageChanged,
                          weak_factory_.GetWeakPtr()));
}

UpdateRequiredScreen::~UpdateRequiredScreen() {
  StopObservingNetworkState();
}

void UpdateRequiredScreen::ShowImpl() {
  LoginScreen::Get()->SetAllowLoginAsGuest(false);
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (view_) {
    view_->SetEnterpriseAndDeviceName(connector->GetEnterpriseDomainManager(),
                                      ui::GetChromeOSDeviceName());
  }

  is_shown_ = true;

  if (first_time_shown_) {
    first_time_shown_ = false;
    if (view_) {
      view_->SetUIState(UpdateRequiredView::UPDATE_REQUIRED_MESSAGE);
      view_->Show();
    }
  }
  // Check network state to set initial screen UI.
  RefreshNetworkState();
  // Fire it once so we're sure we get an invocation on startup.
  OnEolMessageChanged();

  version_updater_->GetEolInfo(base::BindOnce(
      &UpdateRequiredScreen::OnGetEolInfo, weak_factory_.GetWeakPtr()));
}

void UpdateRequiredScreen::OnGetEolInfo(
    const UpdateEngineClient::EolInfo& info) {
  //  TODO(crbug.com/40105880) : Handle if the device is left on this screen
  //  for long enough to reach Eol.
  if (switches::IsAueReachedForUpdateRequiredForTest() ||
      (!info.eol_date.is_null() && info.eol_date <= clock_->Now())) {
    EnsureScreenIsShown();
    if (view_) {
      view_->SetUIState(UpdateRequiredView::EOL_REACHED);
      view_->SetIsUserDataPresent(
          !user_manager::UserManager::Get()->GetUsers().empty());
    }
  } else {
    // UI state does not change for EOL devices.
    // Subscribe to network state change notifications to adapt the UI as
    // network changes till update is started.
    ObserveNetworkState();
  }
}

void UpdateRequiredScreen::OnEolMessageChanged() {
  CrosSettingsProvider::TrustedStatus status =
      CrosSettings::Get()->PrepareTrustedValues(
          base::BindOnce(&UpdateRequiredScreen::OnEolMessageChanged,
                         weak_factory_.GetWeakPtr()));
  if (status != CrosSettingsProvider::TRUSTED)
    return;

  std::string eol_message;
  if (view_ && CrosSettings::Get()->GetString(kDeviceMinimumVersionAueMessage,
                                              &eol_message)) {
    view_->SetEolMessage(eol_message);
  }
}

void UpdateRequiredScreen::HideImpl() {
  is_shown_ = false;
  StopObservingNetworkState();
}

void UpdateRequiredScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionSelectNetworkButtonClicked) {
    OnSelectNetworkButtonClicked();
  } else if (action_id == kUserActionUpdateButtonClicked) {
    OnUpdateButtonClicked();
  } else if (action_id == kUserActionAcceptUpdateOverCellular) {
    if (version_updater_->update_info().status.current_operation() ==
        update_engine::Operation::NEED_PERMISSION_TO_UPDATE) {
      version_updater_->SetUpdateOverCellularOneTimePermission();
    } else {
      // This is to handle the case when metered network screen is shown at the
      // start and user accepts update over it.
      metered_network_update_permission = true;
      StopObservingNetworkState();
      version_updater_->StartNetworkCheck();
    }
  } else if (action_id == kUserActionRejectUpdateOverCellular) {
    version_updater_->RejectUpdateOverCellular();
    version_updater_->StartExitUpdate(VersionUpdater::Result::UPDATE_ERROR);
  } else if (action_id == kUserActionConfirmDeleteUsersData) {
    DeleteUsersData();
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void UpdateRequiredScreen::DefaultNetworkChanged(const NetworkState* network) {
  // Refresh the screen UI to reflect network state.
  RefreshNetworkState();
}

void UpdateRequiredScreen::RefreshNetworkState() {
  // Do not refresh the UI if the update process has started. This can be
  // encountered if error screen is shown and later hidden due to captive portal
  // after starting the update process.
  if (!view_ || is_updating_now_)
    return;

  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  const NetworkState* network = handler->DefaultNetwork();
  // Set the UI state as per the current network state.
  // If device was connected to a good network at the start, we are already
  // showing (by default) the update required screen. If network switches from
  // one good network to another, it has no change in the UI state. This is only
  // till the update process is triggered. Post that, update engine status
  // drives the UI state.
  if (!network || !network->IsConnectedState()) {
    // No network is available for the update process to start.
    view_->SetUIState(UpdateRequiredView::UPDATE_NO_NETWORK);
    waiting_for_connection_ = false;
  } else if (handler->default_network_is_metered()) {
    // The device is either connected to a metered network at the start or has
    // switched to one.
    view_->SetUIState(UpdateRequiredView::UPDATE_NEED_PERMISSION);
    waiting_for_connection_ = true;
  } else if (waiting_for_connection_) {
    // The device has switched from a metered network to a good network. Start
    // the update process automatically and unsubscribe from the network change
    // notifications as any change in network state is reflected in the update
    // engine result.
    waiting_for_connection_ = false;
    is_updating_now_ = true;
    view_->SetUIState(UpdateRequiredView::UPDATE_PROCESS);
    StopObservingNetworkState();
    version_updater_->StartNetworkCheck();
  } else {
    // The device is either connected to a good network at the start or has
    // switched from no network to good network.
    view_->SetUIState(UpdateRequiredView::UPDATE_REQUIRED_MESSAGE);
  }
}

void UpdateRequiredScreen::RefreshView(
    const VersionUpdater::UpdateInfo& update_info) {
  if (!view_)
    return;

  if (update_info.requires_permission_for_cellular) {
    view_->SetUIState(UpdateRequiredView::UPDATE_NEED_PERMISSION);
    waiting_for_connection_ = true;
  } else if (waiting_for_connection_) {
    // Return UI state after getting permission.
    view_->SetUIState(UpdateRequiredView::UPDATE_PROCESS);
    waiting_for_connection_ = false;
  }

  view_->SetUpdateProgressUnavailable(update_info.progress_unavailable);
  view_->SetUpdateProgressValue(update_info.progress);
  view_->SetUpdateProgressMessage(update_info.progress_message);
  view_->SetEstimatedTimeLeftVisible(update_info.show_estimated_time_left);
  view_->SetEstimatedTimeLeft(update_info.estimated_time_left_in_secs);
}

void UpdateRequiredScreen::ObserveNetworkState() {
  if (!is_network_subscribed_) {
    is_network_subscribed_ = true;
    network_state_handler_observer_.Observe(
        NetworkHandler::Get()->network_state_handler());
  }
}

void UpdateRequiredScreen::StopObservingNetworkState() {
  if (is_network_subscribed_) {
    is_network_subscribed_ = false;
    network_state_handler_observer_.Reset();
  }
}

void UpdateRequiredScreen::OnSelectNetworkButtonClicked() {
  SystemTray::Get()->ShowNetworkDetailedViewBubble();
}

void UpdateRequiredScreen::OnUpdateButtonClicked() {
  if (is_updating_now_)
    return;
  is_updating_now_ = true;
  if (view_)
    view_->SetUIState(UpdateRequiredView::UPDATE_PROCESS);

  // Do not need network notification now as UI state depends on the result
  // received from the update engine.
  StopObservingNetworkState();
  version_updater_->StartNetworkCheck();
}

void UpdateRequiredScreen::OnWaitForRebootTimeElapsed() {
  LOG(ERROR) << "Unable to reboot - asking for a manual reboot.";
  EnsureScreenIsShown();
  if (view_)
    view_->SetUIState(UpdateRequiredView::UPDATE_COMPLETED_NEED_REBOOT);
}

void UpdateRequiredScreen::PrepareForUpdateCheck() {
  VLOG(1) << "Update check started.";
  error_message_timer_.Stop();
  error_screen_->HideCaptivePortal();

  connect_request_subscription_ = {};
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
  error_screen_->SetHideCallback(base::BindOnce(
      &UpdateRequiredScreen::OnErrorScreenHidden, weak_factory_.GetWeakPtr()));
  error_screen_->SetIsPersistentError(true /* is_persistent */);
  error_screen_->Show(nullptr);
  histogram_helper_->OnErrorShow(error_screen_->GetErrorState());
}

void UpdateRequiredScreen::UpdateErrorMessage(
    NetworkState::PortalState state,
    NetworkError::ErrorState error_state,
    const std::string& network_name) {
  error_screen_->SetErrorState(error_state, network_name);
  if (state == NetworkState::PortalState::kPortal ||
      state == NetworkState::PortalState::kPortalSuspected) {
    if (is_first_portal_notification_) {
      is_first_portal_notification_ = false;
      error_screen_->FixCaptivePortal();
    }
  }
}

void UpdateRequiredScreen::DelayErrorMessage() {
  if (error_message_timer_.IsRunning())
    return;
  error_message_timer_.Start(FROM_HERE, error_message_delay_, this,
                             &UpdateRequiredScreen::ShowErrorMessage);
}

void UpdateRequiredScreen::SetErrorMessageDelayForTesting(
    const base::TimeDelta& delay) {
  error_message_delay_ = delay;
}

void UpdateRequiredScreen::UpdateInfoChanged(
    const VersionUpdater::UpdateInfo& update_info) {
  switch (update_info.status.current_operation()) {
    case update_engine::Operation::CHECKING_FOR_UPDATE:
    case update_engine::Operation::CLEANUP_PREVIOUS_UPDATE:
    case update_engine::Operation::ATTEMPTING_ROLLBACK:
    case update_engine::Operation::DISABLED:
    case update_engine::Operation::IDLE:
    case update_engine::Operation::UPDATED_BUT_DEFERRED:
      break;
    case update_engine::Operation::UPDATE_AVAILABLE:
    case update_engine::Operation::DOWNLOADING:
    case update_engine::Operation::VERIFYING:
    case update_engine::Operation::FINALIZING:
      EnsureScreenIsShown();
      break;
    case update_engine::Operation::NEED_PERMISSION_TO_UPDATE:
      VLOG(1) << "Need permission to update.";
      EnsureScreenIsShown();
      if (metered_network_update_permission) {
        version_updater_->SetUpdateOverCellularOneTimePermission();
        return;
      }
      break;
    case update_engine::Operation::UPDATED_NEED_REBOOT:
      VLOG(1) << "Update completed successfully.";
      EnsureScreenIsShown();
      waiting_for_reboot_ = true;
      version_updater_->RebootAfterUpdate();
      break;
    case update_engine::Operation::ERROR:
    case update_engine::Operation::REPORTING_ERROR_EVENT:
      LOG(ERROR) << "Exiting update due to error.";
      version_updater_->StartExitUpdate(VersionUpdater::Result::UPDATE_ERROR);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
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

void UpdateRequiredScreen::Exit() {
  DCHECK(!is_hidden());
  exit_callback_.Run();
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
  error_screen_->SetParentScreen(OOBE_SCREEN_UNKNOWN);
  // Return to the default state.
  error_screen_->SetIsPersistentError(false /* is_persistent */);
  Show(context());
}

void UpdateRequiredScreen::DeleteUsersData() {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  // Make a copy of the list since we'll be removing users and the list would
  // change underneath.
  const user_manager::UserList user_list = user_manager->GetUsers();
  for (user_manager::User* user : user_list) {
    user_manager->RemoveUser(user->GetAccountId(),
                             user_manager::UserRemovalReason::
                                 LOCAL_USER_INITIATED_ON_REQUIRED_UPDATE);
  }

  // TODO(b/277159583): Here we check the user list, but the exact
  // condition we should check is whether actual user data are successfully
  // removed.
  if (user_manager->GetUsers().empty()) {
    view_->SetIsUserDataPresent(false);
  }
}

}  // namespace ash
