// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_UI_CONTROLLER_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_UI_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/components/phonehub/feature_status_provider.h"
#include "chromeos/components/phonehub/onboarding_ui_tracker.h"

namespace chromeos {
namespace phonehub {
class PhoneHubManager;
}  // namespace phonehub
}  // namespace chromeos

namespace views {
class View;
}  // namespace views

namespace ash {

class TrayBubbleView;

// This controller translates the state received from PhoneHubManager into the
// corresponding main content view to be displayed in the tray bubble.
class ASH_EXPORT PhoneHubUiController
    : public chromeos::phonehub::FeatureStatusProvider::Observer,
      public chromeos::phonehub::OnboardingUiTracker::Observer {
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
    kInitialConnecting,
    kPhoneConnecting,
    kConnectionError,
    kPhoneConnected,
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
  std::unique_ptr<views::View> CreateContentView(TrayBubbleView* bubble_view);

  // Creates the header view displaying the phone status.
  std::unique_ptr<views::View> CreateStatusHeaderView();

  // Observer functions.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  UiState ui_state() const { return ui_state_; }

 private:
  // chromeos::phonehub::FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // chromeos::phonehub::OnboardingUiTracker::Observer:
  void OnShouldShowOnboardingUiChanged() override;

  // Updates the current UI state and notifies observers.
  void UpdateUiState();

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
