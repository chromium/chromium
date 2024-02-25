// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_UI_CONTROLLER_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_UI_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/phonehub/onboarding_view.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "ash/system/phonehub/phone_status_view.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "chromeos/ash/components/phonehub/feature_status_provider.h"
#include "chromeos/ash/components/phonehub/onboarding_ui_tracker.h"
#include "chromeos/ash/components/phonehub/phone_model.h"

namespace views {
class View;
}  // namespace views

namespace ash {

namespace phonehub {
class PhoneHubManager;
}

// This controller translates the state received from PhoneHubManager into the
// corresponding main content view to be displayed in the tray bubble.
class ASH_EXPORT PhoneHubUiController
    : public phonehub::FeatureStatusProvider::Observer,
      public phonehub::OnboardingUiTracker::Observer,
      public phonehub::PhoneModel::Observer,
      public phonehub::AppStreamLauncherDataModel::Observer,
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
    kMiniLauncher,
    kMaxValue = kMiniLauncher
  };

  PhoneHubUiController();
  PhoneHubUiController(const PhoneHubUiController&) = delete;
  ~PhoneHubUiController() override;
  PhoneHubUiController& operator=(const PhoneHubUiController&) = delete;

  // Sets the PhoneHubManager that provides the data to drive the UI.
  void SetPhoneHubManager(phonehub::PhoneHubManager* phone_hub_manager);

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
  // phonehub::FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // phonehub::OnboardingUiTracker::Observer:
  void OnShouldShowOnboardingUiChanged() override;

  // phonehub::AppStreamLauncherDataModel::Observer:
  void OnShouldShowMiniLauncherChanged() override;

  // phonehub::PhoneModel::Observer:
  void OnModelChanged() override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // Updates the current UI state and notifies observers.
  void UpdateUiState(PhoneHubUiController::UiState new_state);

  // Returns the UiState from the PhoneHubManager.
  UiState GetUiStateFromPhoneHubManager();

  // Returns the UiState from the PhoneHubManager.
  UiState GetUiStateFromPhoneHubManagerInternal();

  // Cleans up |phone_hub_manager_| by removing all observers.
  void CleanUpPhoneHubManager();

  // When |connecting_view_grace_period_timer_| ends, triggers a change in
  // the content view to show a disconnected view.
  void OnConnectingViewTimerEnd();

  void RecordStatusOnBubbleOpened();
  void OnGetHostLastSeenTimestamp(UiState ui_state_when_opened,
                                  std::optional<base::Time> timestamp);

  // The PhoneHubManager that provides data for the UI.
  raw_ptr<phonehub::PhoneHubManager> phone_hub_manager_ = nullptr;

  // The current UI state.
  UiState ui_state_ = UiState::kHidden;

  // This value becomes true the first time the user opens the PhoneHub UI
  // when the feature is in the enabled state, and a tether scan request is
  // made.
  bool has_requested_tether_scan_during_session_ = false;

  // Registered observers.
  base::ObserverList<Observer> observer_list_;

  // The timer that dictates how long to show |kConnecting| after disconnect
  // so when the connection fails on the first attempt and retries, it is not
  // confusing to users when it shows disconnecting view, rather, it will show
  // connecting view on this occasion.
  base::OneShotTimer connecting_view_grace_period_timer_;

  base::WeakPtrFactory<PhoneHubUiController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_UI_CONTROLLER_H_
