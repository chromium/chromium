// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_UI_CONTROLLER_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_UI_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/phonehub/onboarding_view.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "ash/system/phonehub/phone_status_view.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/components/phonehub/feature_status_provider.h"
#include "chromeos/components/phonehub/onboarding_ui_tracker.h"
#include "chromeos/components/phonehub/phone_model.h"

namespace chromeos {
namespace phonehub {
class PhoneHubManager;
}  // namespace phonehub
}  // namespace chromeos

namespace views {
class View;
}  // namespace views

namespace ash {

// This controller translates the state received from PhoneHubManager into the
// corresponding main content view to be displayed in the tray bubble.
class ASH_EXPORT PhoneHubUiController
    : public chromeos::phonehub::FeatureStatusProvider::Observer,
      public chromeos::phonehub::OnboardingUiTracker::Observer,
      public chromeos::phonehub::PhoneModel::Observer,
      public SessionObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnPhoneHubUiStateChanged() = 0;
  };

  // All the possible states that the main content view can be in. Each state
  // has a corresponding view class.
  enum class UiState {
    kHidden = 0,
    kOnboardingWithoutPhone,
    kOnboardingWithPhone,
    kBluetoothDisabled,
    kPhoneConnecting,
    kPhoneDisconnected,
    kPhoneConnected,
    kTetherConnectionPending,
  };

  PhoneHubUiController();
  PhoneHubUiController(const PhoneHubUiController&) = delete;
  ~PhoneHubUiController() override;
  PhoneHubUiController& operator=(const PhoneHubUiController&) = delete;

  // Sets the PhoneHubManager that provides the data to drive the UI.
  void SetPhoneHubManager(
      chromeos::phonehub::PhoneHubManager* phone_hub_manager);

  // Creates the corresponding content view for the current UI state.
  // |bubble_view| will be the parent the created content view.
  std::unique_ptr<PhoneHubContentView> CreateContentView(
      OnboardingView::Delegate* delegate);

  // Creates the header view displaying the phone status.
  std::unique_ptr<views::View> CreateStatusHeaderView(
      PhoneStatusView::Delegate* delegate);

  // Handler for when the bubble is opened. Requests a connection to the phone
  // if there is no current connection, and records metrics.
  void HandleBubbleOpened();

  // Observer functions.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  UiState ui_state() const { return ui_state_; }

 private:
  // chromeos::phonehub::FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // chromeos::phonehub::OnboardingUiTracker::Observer:
  void OnShouldShowOnboardingUiChanged() override;

  // chromeos::phonehub::PhoneModel::Observer:
  void OnModelChanged() override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // Updates the current UI state and notifies observers.
  void UpdateUiState(PhoneHubUiController::UiState new_state);

  // Returns the UiState from the PhoneHubManager.
  UiState GetUiStateFromPhoneHubManager();

  // Cleans up |phone_hub_manager_| by removing all observers.
  void CleanUpPhoneHubManager();

  // The PhoneHubManager that provides data for the UI.
  chromeos::phonehub::PhoneHubManager* phone_hub_manager_ = nullptr;

  // The current UI state.
  UiState ui_state_ = UiState::kHidden;

  // Registered observers.
  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_UI_CONTROLLER_H_
