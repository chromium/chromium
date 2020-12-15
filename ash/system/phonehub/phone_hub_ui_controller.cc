// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_ui_controller.h"

#include <memory>

#include "ash/system/phonehub/bluetooth_disabled_view.h"
#include "ash/system/phonehub/connection_error_view.h"
#include "ash/system/phonehub/initial_connecting_view.h"
#include "ash/system/phonehub/onboarding_view.h"
#include "ash/system/phonehub/phone_connected_view.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "base/logging.h"
#include "chromeos/components/phonehub/connection_scheduler.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"
#include "chromeos/components/phonehub/user_action_recorder.h"

using FeatureStatus = chromeos::phonehub::FeatureStatus;

namespace ash {

PhoneHubUiController::PhoneHubUiController() = default;

PhoneHubUiController::~PhoneHubUiController() {
  CleanUpPhoneHubManager();
}

void PhoneHubUiController::SetPhoneHubManager(
    chromeos::phonehub::PhoneHubManager* phone_hub_manager) {
  if (phone_hub_manager == phone_hub_manager_)
    return;

  CleanUpPhoneHubManager();

  phone_hub_manager_ = phone_hub_manager;
  if (phone_hub_manager_) {
    phone_hub_manager_->GetFeatureStatusProvider()->AddObserver(this);
    phone_hub_manager_->GetOnboardingUiTracker()->AddObserver(this);
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
    case UiState::kInitialConnecting:
      return std::make_unique<InitialConnectingView>();
    case UiState::kPhoneConnecting:
      return std::make_unique<ConnectionErrorView>(
          ConnectionErrorView::ErrorStatus::kReconnecting,
          phone_hub_manager_->GetConnectionScheduler());
    case UiState::kConnectionError:
      return std::make_unique<ConnectionErrorView>(
          ConnectionErrorView::ErrorStatus::kDisconnected,
          phone_hub_manager_->GetConnectionScheduler());
    case UiState::kPhoneConnected:
      return std::make_unique<PhoneConnectedView>(phone_hub_manager_);
  }
}

void PhoneHubUiController::HandleBubbleOpened() {
  if (!phone_hub_manager_)
    return;

  auto feature_status =
      phone_hub_manager_->GetFeatureStatusProvider()->GetStatus();
  if (feature_status == FeatureStatus::kEnabledButDisconnected)
    phone_hub_manager_->GetConnectionScheduler()->ScheduleConnectionNow();

  phone_hub_manager_->GetUserActionRecorder()->RecordUiOpened();
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

void PhoneHubUiController::UpdateUiState(
    PhoneHubUiController::UiState new_state) {
  if (new_state == ui_state_)
    return;

  ui_state_ = new_state;
  for (auto& observer : observer_list_)
    observer.OnPhoneHubUiStateChanged();
}

PhoneHubUiController::UiState
PhoneHubUiController::GetUiStateFromPhoneHubManager() {
  if (!phone_hub_manager_)
    return UiState::kHidden;

  auto feature_status =
      phone_hub_manager_->GetFeatureStatusProvider()->GetStatus();

  auto* tracker = phone_hub_manager_->GetOnboardingUiTracker();
  bool should_show_onboarding_ui = tracker->ShouldShowOnboardingUi();

  switch (feature_status) {
    case FeatureStatus::kNotEligibleForFeature:
      return UiState::kHidden;
    case FeatureStatus::kEligiblePhoneButNotSetUp:
      return should_show_onboarding_ui ? UiState::kOnboardingWithPhone
                                       : UiState::kHidden;
    case FeatureStatus::kDisabled:
      return should_show_onboarding_ui ? UiState::kOnboardingWithoutPhone
                                       : UiState::kHidden;
    case FeatureStatus::kPhoneSelectedAndPendingSetup:
      return UiState::kInitialConnecting;
    case FeatureStatus::kUnavailableBluetoothOff:
      return UiState::kBluetoothDisabled;
    case FeatureStatus::kEnabledButDisconnected:
      return UiState::kConnectionError;
    case FeatureStatus::kEnabledAndConnecting:
      return UiState::kPhoneConnecting;
    case FeatureStatus::kEnabledAndConnected:
      return UiState::kPhoneConnected;
    case FeatureStatus::kLockOrSuspended:
      return UiState::kHidden;
  }
}

void PhoneHubUiController::CleanUpPhoneHubManager() {
  if (!phone_hub_manager_)
    return;

  phone_hub_manager_->GetFeatureStatusProvider()->RemoveObserver(this);
  phone_hub_manager_->GetOnboardingUiTracker()->RemoveObserver(this);
}

}  // namespace ash
