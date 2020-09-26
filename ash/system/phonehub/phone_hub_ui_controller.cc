// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_ui_controller.h"

#include "ash/system/phonehub/bluetooth_disabled_view.h"
#include "ash/system/phonehub/connection_error_view.h"
#include "ash/system/phonehub/initial_connecting_view.h"
#include "ash/system/phonehub/onboarding_view.h"
#include "ash/system/phonehub/phone_connected_view.h"
#include "ash/system/phonehub/phone_status_view.h"
#include "base/logging.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"

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

  UpdateUiState();
}

std::unique_ptr<views::View> PhoneHubUiController::CreateStatusHeaderView() {
  if (!phone_hub_manager_)
    return nullptr;
  return std::make_unique<PhoneStatusView>(phone_hub_manager_->GetPhoneModel());
}

std::unique_ptr<views::View> PhoneHubUiController::CreateContentView(
    TrayBubbleView* bubble_view) {
  switch (ui_state_) {
    case UiState::kHidden:
      return nullptr;
    case UiState::kOnboardingWithoutPhone:
      // TODO(tengs): distinguish this onboarding with phone state.
      FALLTHROUGH;
    case UiState::kOnboardingWithPhone:
      return std::make_unique<OnboardingView>();
    case UiState::kBluetoothDisabled:
      return std::make_unique<BluetoothDisabledView>();
    case UiState::kInitialConnecting:
      return std::make_unique<InitialConnectingView>();
    case UiState::kPhoneConnecting:
      return std::make_unique<ConnectionErrorView>(
          ConnectionErrorView::ErrorStatus::kReconnecting);
    case UiState::kConnectionError:
      return std::make_unique<ConnectionErrorView>(
          ConnectionErrorView::ErrorStatus::kDisconnected);
    case UiState::kPhoneConnected:
      return std::make_unique<PhoneConnectedView>(bubble_view,
                                                  phone_hub_manager_);
  }
}

void PhoneHubUiController::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void PhoneHubUiController::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void PhoneHubUiController::OnFeatureStatusChanged() {
  UpdateUiState();
}

void PhoneHubUiController::OnShouldShowOnboardingUiChanged() {
  UpdateUiState();
}

void PhoneHubUiController::UpdateUiState() {
  auto new_state = GetUiStateFromPhoneHubManager();
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
  }
}

void PhoneHubUiController::CleanUpPhoneHubManager() {
  if (!phone_hub_manager_)
    return;

  phone_hub_manager_->GetFeatureStatusProvider()->RemoveObserver(this);
  phone_hub_manager_->GetOnboardingUiTracker()->RemoveObserver(this);
}

}  // namespace ash
