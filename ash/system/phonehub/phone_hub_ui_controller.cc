// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_ui_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/system/phonehub/app_stream_launcher_view.h"
#include "ash/system/phonehub/bluetooth_disabled_view.h"
#include "ash/system/phonehub/onboarding_view.h"
#include "ash/system/phonehub/phone_connected_view.h"
#include "ash/system/phonehub/phone_connecting_view.h"
#include "ash/system/phonehub/phone_disconnected_view.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "ash/system/phonehub/tether_connection_pending_view.h"
#include "ash/system/status_area_widget.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/browser_tabs_model_provider.h"
#include "chromeos/ash/components/phonehub/connection_scheduler.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"
#include "chromeos/ash/components/phonehub/phone_hub_ui_readiness_recorder.h"
#include "chromeos/ash/components/phonehub/tether_controller.h"
#include "chromeos/ash/components/phonehub/user_action_recorder.h"

namespace ash {

namespace {

using FeatureStatus = phonehub::FeatureStatus;
using TetherStatus = phonehub::TetherController::Status;

constexpr base::TimeDelta kConnectingViewGracePeriod = base::Seconds(40);

phone_hub_metrics::Screen GetMetricsScreen(
    PhoneHubUiController::UiState ui_state) {
  switch (ui_state) {
    case PhoneHubUiController::UiState::kOnboardingWithoutPhone:
      return phone_hub_metrics::Screen::kOnboardingNewMultideviceUser;

    case PhoneHubUiController::UiState::kOnboardingWithPhone:
      return phone_hub_metrics::Screen::kOnboardingExistingMultideviceUser;

    case PhoneHubUiController::UiState::kPhoneConnected:
      return phone_hub_metrics::Screen::kPhoneConnected;

    case PhoneHubUiController::UiState::kPhoneDisconnected:
      return phone_hub_metrics::Screen::kPhoneDisconnected;

    case PhoneHubUiController::UiState::kPhoneConnecting:
      return phone_hub_metrics::Screen::kPhoneConnecting;

    case PhoneHubUiController::UiState::kBluetoothDisabled:
      return phone_hub_metrics::Screen::kBluetoothOrWifiDisabled;

    case PhoneHubUiController::UiState::kTetherConnectionPending:
      return phone_hub_metrics::Screen::kTetherConnectionPending;

    case PhoneHubUiController::UiState::kMiniLauncher:
      return phone_hub_metrics::Screen::kMiniLauncher;

    case PhoneHubUiController::UiState::kHidden:
      return phone_hub_metrics::Screen::kInvalid;
  }
}

std::string PhoneHubUIStateToString(PhoneHubUiController::UiState ui_state) {
  switch (ui_state) {
    case PhoneHubUiController::UiState::kOnboardingWithoutPhone:
      return "[kOnboardingWithoutPhone]";

    case PhoneHubUiController::UiState::kOnboardingWithPhone:
      return "[kOnboardingWithPhone]";

    case PhoneHubUiController::UiState::kPhoneConnected:
      return "[kPhoneConnected]";

    case PhoneHubUiController::UiState::kPhoneDisconnected:
      return "[kPhoneDisconnected]";

    case PhoneHubUiController::UiState::kPhoneConnecting:
      return "[kPhoneConnecting]";

    case PhoneHubUiController::UiState::kBluetoothDisabled:
      return "[kBluetoothDisabled]";

    case PhoneHubUiController::UiState::kTetherConnectionPending:
      return "[kTetherConnectionPending]";

    case PhoneHubUiController::UiState::kMiniLauncher:
      return "[kMiniLauncher]";

    case PhoneHubUiController::UiState::kHidden:
      return "[kHidden]";
  }
}

std::string FeatureStatusToString(FeatureStatus feature_status) {
  switch (feature_status) {
    case FeatureStatus::kNotEligibleForFeature:
      return "[kNotEligibleForFeature]";
    case FeatureStatus::kEligiblePhoneButNotSetUp:
      return "[kEligiblePhoneButNotSetUp]";
    case FeatureStatus::kPhoneSelectedAndPendingSetup:
      return "[kPhoneSelectedAndPendingSetup]";
    case FeatureStatus::kDisabled:
      return "[kDisabled]";
    case FeatureStatus::kUnavailableBluetoothOff:
      return "[kUnavailableBluetoothOff]";
    case FeatureStatus::kEnabledButDisconnected:
      return "[kEnabledButDisconnected]";
    case FeatureStatus::kEnabledAndConnecting:
      return "[kEnabledAndConnecting]";
    case FeatureStatus::kEnabledAndConnected:
      return "[kEnabledAndConnected]";
    case FeatureStatus::kLockOrSuspended:
      return "[kLockOrSuspended]";
  }
}

}  // namespace

PhoneHubUiController::PhoneHubUiController() {
  // ash::Shell may not exist in tests.
  if (ash::Shell::HasInstance())
    Shell::Get()->session_controller()->AddObserver(this);
}

PhoneHubUiController::~PhoneHubUiController() {
  // ash::Shell may not exist in tests.
  if (ash::Shell::HasInstance())
    Shell::Get()->session_controller()->RemoveObserver(this);
  CleanUpPhoneHubManager();
}

void PhoneHubUiController::SetPhoneHubManager(
    phonehub::PhoneHubManager* phone_hub_manager) {
  if (phone_hub_manager == phone_hub_manager_)
    return;

  CleanUpPhoneHubManager();

  phone_hub_manager_ = phone_hub_manager;
  if (phone_hub_manager_) {
    phone_hub_manager_->GetFeatureStatusProvider()->AddObserver(this);
    phone_hub_manager_->GetOnboardingUiTracker()->AddObserver(this);
    if (features::IsEcheLauncherEnabled())
      phone_hub_manager_->GetAppStreamLauncherDataModel()->AddObserver(this);
    phone_hub_manager_->GetPhoneModel()->AddObserver(this);
  }

  UpdateUiState(GetUiStateFromPhoneHubManager());
}

std::unique_ptr<views::View> PhoneHubUiController::CreateStatusHeaderView(
    PhoneStatusView::Delegate* delegate) {
  if (!phone_hub_manager_)
    return nullptr;
  return std::make_unique<PhoneStatusView>(phone_hub_manager_->GetPhoneModel(),
                                           delegate);
}

std::unique_ptr<PhoneHubContentView> PhoneHubUiController::CreateContentView(
    OnboardingView::Delegate* delegate) {
  PA_LOG(VERBOSE) << __func__
                  << ": ui state = " << PhoneHubUIStateToString(ui_state_);

  switch (ui_state_) {
    case UiState::kHidden:
      return nullptr;
    case UiState::kOnboardingWithoutPhone:
      return std::make_unique<OnboardingView>(
          phone_hub_manager_->GetOnboardingUiTracker(), delegate,
          OnboardingView::kNewMultideviceUser);
    case UiState::kOnboardingWithPhone:
      return std::make_unique<OnboardingView>(
          phone_hub_manager_->GetOnboardingUiTracker(), delegate,
          OnboardingView::kExistingMultideviceUser);
    case UiState::kBluetoothDisabled:
      return std::make_unique<BluetoothDisabledView>();
    case UiState::kPhoneConnecting:
      return std::make_unique<PhoneConnectingView>();
    case UiState::kTetherConnectionPending:
      return std::make_unique<TetherConnectionPendingView>();
    case UiState::kPhoneDisconnected:
      if (connecting_view_grace_period_timer_.IsRunning())
        return std::make_unique<PhoneConnectingView>();
      return std::make_unique<PhoneDisconnectedView>(
          phone_hub_manager_->GetConnectionScheduler());
    case UiState::kPhoneConnected:
      return std::make_unique<PhoneConnectedView>(phone_hub_manager_);
    case UiState::kMiniLauncher:
      return std::make_unique<AppStreamLauncherView>(phone_hub_manager_);
  }
}

void PhoneHubUiController::HandleBubbleOpened() {
  // Make sure Eche window is not shown.
  if (features::IsEcheSWAEnabled()) {
    EcheTray* eche_tray = Shell::GetPrimaryRootWindowController()
                              ->GetStatusAreaWidget()
                              ->eche_tray();
    if (eche_tray)
      eche_tray->CloseBubble();
  }

  if (!phone_hub_manager_)
    return;

  auto feature_status =
      phone_hub_manager_->GetFeatureStatusProvider()->GetStatus();
  if (feature_status == FeatureStatus::kEnabledButDisconnected)
    phone_hub_manager_->GetConnectionScheduler()->ScheduleConnectionNow(
        phonehub::DiscoveryEntryPoint::kPhoneHubBubbleOpen);

  if (features::IsEcheNetworkConnectionStateEnabled() &&
      feature_status == FeatureStatus::kEnabledAndConnected) {
    if (phone_hub_manager_->GetEcheConnectionStatusHandler()) {
      phone_hub_manager_->GetEcheConnectionStatusHandler()
          ->CheckConnectionStatusForUi();
    }
  }

  phone_hub_manager_->GetBrowserTabsModelProvider()->TriggerRefresh();
  RecordStatusOnBubbleOpened();

  bool is_feature_enabled =
      feature_status == FeatureStatus::kEnabledAndConnected ||
      feature_status == FeatureStatus::kEnabledButDisconnected ||
      feature_status == FeatureStatus::kEnabledAndConnected;

  if (!is_feature_enabled) {
    PA_LOG(VERBOSE) << __func__ << ": feature is not enabled. Feature status = "
                    << FeatureStatusToString(feature_status);
    return;
  }

  PA_LOG(VERBOSE) << __func__ << ": feature is enabled. Feature status = "
                  << FeatureStatusToString(feature_status);

  if (!has_requested_tether_scan_during_session_ &&
      phone_hub_manager_->GetTetherController()->GetStatus() ==
          TetherStatus::kConnectionUnavailable) {
    phone_hub_manager_->GetTetherController()->ScanForAvailableConnection();
    has_requested_tether_scan_during_session_ = true;
  }
}

void PhoneHubUiController::RecordStatusOnBubbleOpened() {
  switch (ui_state_) {
    case UiState::kHidden:
    case UiState::kOnboardingWithoutPhone:
    case UiState::kOnboardingWithPhone:
    case UiState::kBluetoothDisabled:
    case UiState::kTetherConnectionPending:
      return;

    case UiState::kMiniLauncher:
    case UiState::kPhoneConnected:
      base::UmaHistogramEnumeration("PhoneHub.BubbleOpened.Connectable.Page",
                                    phone_hub_metrics::Screen::kPhoneConnected);
      return;

    case UiState::kPhoneDisconnected:
    case UiState::kPhoneConnecting:
      phone_hub_manager_->GetHostLastSeenTimestamp(
          base::BindOnce(&PhoneHubUiController::OnGetHostLastSeenTimestamp,
                         weak_ptr_factory_.GetWeakPtr(), ui_state_));
      return;
  }
}

void PhoneHubUiController::OnGetHostLastSeenTimestamp(
    UiState ui_state_when_opened,
    std::optional<base::Time> timestamp) {
  if (timestamp) {
    base::UmaHistogramLongTimes(
        "PhoneHub.BubbleOpened.Connectable.Failed.HostLastSeen",
        base::Time::Now() - timestamp.value());
  }

  // Only log when we've seen the host phone within the last 2 minutes.
  if (!timestamp || base::Time::Now() - timestamp.value() > base::Minutes(2)) {
    return;
  }

  base::UmaHistogramEnumeration("PhoneHub.BubbleOpened.Connectable.Page",
                                GetMetricsScreen(ui_state_when_opened));
}

void PhoneHubUiController::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void PhoneHubUiController::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void PhoneHubUiController::OnFeatureStatusChanged() {
  UpdateUiState(GetUiStateFromPhoneHubManager());
}

void PhoneHubUiController::OnShouldShowOnboardingUiChanged() {
  UpdateUiState(GetUiStateFromPhoneHubManager());
}

void PhoneHubUiController::OnShouldShowMiniLauncherChanged() {
  if (!features::IsEcheLauncherEnabled())
    return;
  UpdateUiState(GetUiStateFromPhoneHubManager());
}

void PhoneHubUiController::OnModelChanged() {
  PA_LOG(INFO) << "Updating UI status as Phone Model has changed";
  UpdateUiState(GetUiStateFromPhoneHubManager());
}

void PhoneHubUiController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  UpdateUiState(GetUiStateFromPhoneHubManager());
}

void PhoneHubUiController::UpdateUiState(
    PhoneHubUiController::UiState new_state) {
  if (new_state == ui_state_)
    return;

  PA_LOG(VERBOSE) << __func__
                  << ": old ui = " << PhoneHubUIStateToString(ui_state_)
                  << ", new ui = " << PhoneHubUIStateToString(new_state);
  ui_state_ = new_state;

  for (auto& observer : observer_list_) {
    observer.OnPhoneHubUiStateChanged();
  }

  switch (ui_state_) {
    case UiState::kBluetoothDisabled:
      [[fallthrough]];
    case UiState::kPhoneDisconnected:
      if (phone_hub_manager_->GetPhoneHubStructuredMetricsLogger()) {
        phone_hub_manager_->GetPhoneHubStructuredMetricsLogger()
            ->LogPhoneHubUiStateUpdated(
                phonehub::PhoneHubUiState::kDisconnected);
        phone_hub_manager_->GetPhoneHubStructuredMetricsLogger()
            ->ResetSessionId();
      }
      break;
    case UiState::kPhoneConnecting:
      if (phone_hub_manager_->GetPhoneHubStructuredMetricsLogger()) {
        phone_hub_manager_->GetPhoneHubStructuredMetricsLogger()
            ->LogPhoneHubUiStateUpdated(phonehub::PhoneHubUiState::kConnecting);
      }
      break;
    case UiState::kPhoneConnected:
      if (phone_hub_manager_->GetPhoneHubUiReadinessRecorder()) {
        phone_hub_manager_->GetPhoneHubUiReadinessRecorder()
            ->RecordPhoneHubUiConnected();
      }
      if (phone_hub_manager_->GetPhoneHubStructuredMetricsLogger()) {
        phone_hub_manager_->GetPhoneHubStructuredMetricsLogger()
            ->LogPhoneHubUiStateUpdated(phonehub::PhoneHubUiState::kConnected);
      }
      break;
    default:
      break;
  }
}

PhoneHubUiController::UiState
PhoneHubUiController::GetUiStateFromPhoneHubManager() {
  PhoneHubUiController::UiState ui_state =
      GetUiStateFromPhoneHubManagerInternal();
  if (features::IsEcheLauncherEnabled() &&
      (ui_state != PhoneHubUiController::UiState::kMiniLauncher) &&
      phone_hub_manager_ &&
      phone_hub_manager_->GetAppStreamLauncherDataModel()) {
    // Make sure the next time we go back to the "Phone Connected" state
    // we do not show the Mini Launcher.
    phone_hub_manager_->GetAppStreamLauncherDataModel()->ResetState();
  }
  return ui_state;
}

PhoneHubUiController::UiState
PhoneHubUiController::GetUiStateFromPhoneHubManagerInternal() {
  if (!Shell::Get()->session_controller()->IsUserPrimary() ||
      !phone_hub_manager_)
    return UiState::kHidden;

  auto feature_status =
      phone_hub_manager_->GetFeatureStatusProvider()->GetStatus();

  auto* tracker = phone_hub_manager_->GetOnboardingUiTracker();
  auto* phone_model = phone_hub_manager_->GetPhoneModel();
  bool should_show_onboarding_ui = tracker->ShouldShowOnboardingUi();
  bool is_tether_connecting =
      phone_hub_manager_->GetTetherController()->GetStatus() ==
      TetherStatus::kConnecting;

  switch (feature_status) {
    case FeatureStatus::kPhoneSelectedAndPendingSetup:
      [[fallthrough]];
    case FeatureStatus::kNotEligibleForFeature:
      return UiState::kHidden;

    case FeatureStatus::kEligiblePhoneButNotSetUp:
      return should_show_onboarding_ui ? UiState::kOnboardingWithoutPhone
                                       : UiState::kHidden;

    case FeatureStatus::kDisabled:
      return should_show_onboarding_ui ? UiState::kOnboardingWithPhone
                                       : UiState::kHidden;

    case FeatureStatus::kUnavailableBluetoothOff:
      return UiState::kBluetoothDisabled;

    case FeatureStatus::kEnabledButDisconnected:
      return UiState::kPhoneDisconnected;

    case FeatureStatus::kEnabledAndConnecting:
      connecting_view_grace_period_timer_.Start(
          FROM_HERE, kConnectingViewGracePeriod,
          base::BindOnce(&PhoneHubUiController::OnConnectingViewTimerEnd,
                         base::Unretained(this)));

      // If a tether network is being connected to, or the |ui_state_|
      // was UiState::kTetherConnectionPending, continue returning
      // the UiState::kTetherConnectionPending state.
      return is_tether_connecting ||
                     ui_state_ == UiState::kTetherConnectionPending
                 ? UiState::kTetherConnectionPending
                 : UiState::kPhoneConnecting;

    case FeatureStatus::kEnabledAndConnected:
      // If the timer is running, reset the timer so if we disconnect, we will
      // show the connecting view instead of the disconnecting view.
      if (connecting_view_grace_period_timer_.IsRunning())
        connecting_view_grace_period_timer_.Reset();

      // Delay displaying the connected view until the phone model is ready.
      if (phone_model->phone_status_model().has_value()) {
        // Decide to show the Mini Launcher or the main connected phone view.
        return phone_hub_manager_->GetAppStreamLauncherDataModel()
                           ->GetShouldShowMiniLauncher() &&
                       features::IsEcheSWAEnabled() &&
                       features::IsEcheLauncherEnabled()
                   ? UiState::kMiniLauncher
                   : UiState::kPhoneConnected;
      }

      // If the the |ui_state_| was UiState::kTetherConnectionPending, continue
      // returning the UiState::kTetherConnectionPending state.
      if (ui_state_ == UiState::kTetherConnectionPending)
        return UiState::kTetherConnectionPending;

      return UiState::kPhoneConnecting;

    case FeatureStatus::kLockOrSuspended:
      return UiState::kHidden;
  }
}

void PhoneHubUiController::OnConnectingViewTimerEnd() {
  // Update the UI state if the UI state has changed.
  if (ui_state_ != UiState::kPhoneDisconnected) {
    UpdateUiState(GetUiStateFromPhoneHubManager());
    return;
  }

  // If we are still disconnected, force the observation. We cannot call
  // |GetUiStateFromPhoneHubManager()| in this case because it will reset the
  // timer, and thus the disconnected view will never be shown. This way, the
  // disconnected view will be shown.
  for (auto& observer : observer_list_)
    observer.OnPhoneHubUiStateChanged();
}

void PhoneHubUiController::CleanUpPhoneHubManager() {
  if (!phone_hub_manager_)
    return;

  phone_hub_manager_->GetFeatureStatusProvider()->RemoveObserver(this);
  phone_hub_manager_->GetOnboardingUiTracker()->RemoveObserver(this);
  if (features::IsEcheSWAEnabled())
    phone_hub_manager_->GetAppStreamLauncherDataModel()->RemoveObserver(this);
  phone_hub_manager_->GetPhoneModel()->RemoveObserver(this);
}

}  // namespace ash
