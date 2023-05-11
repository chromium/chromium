// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_TRAY_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_TRAY_H_

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/session/session_controller_impl.h"
#include "ash/system/phonehub/onboarding_view.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "ash/system/phonehub/phone_hub_nudge_controller.h"
#include "ash/system/phonehub/phone_hub_ui_controller.h"
#include "ash/system/phonehub/phone_status_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/phonehub/app_stream_manager.h"
#include "chromeos/ash/components/phonehub/icon_decoder.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/image_button.h"

namespace views {
class ImageButton;
}
namespace ash {

class EcheIconLoadingIndicatorView;
class PhoneHubContentView;
class TrayBubbleWrapper;
class SessionControllerImpl;

namespace phonehub {
namespace proto {
class AppStreamUpdate;
}  // namespace proto
class PhoneHubManager;
}

// This class represents the Phone Hub tray button in the status area and
// controls the bubble that is shown when the tray button is clicked.
class ASH_EXPORT PhoneHubTray : public TrayBackgroundView,
                                public OnboardingView::Delegate,
                                public PhoneStatusView::Delegate,
                                public PhoneHubUiController::Observer,
                                public SessionObserver,
                                public WindowTreeHostManager::Observer,
                                public phonehub::AppStreamManager::Observer {
 public:
  explicit PhoneHubTray(Shelf* shelf);
  PhoneHubTray(const PhoneHubTray&) = delete;
  ~PhoneHubTray() override;
  PhoneHubTray& operator=(const PhoneHubTray&) = delete;

  // Sets the PhoneHubManager that provides the data to drive the UI.
  void SetPhoneHubManager(phonehub::PhoneHubManager* phone_hub_manager);

  // TrayBackgroundView:
  void ClickedOutsideBubble() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void AnchorUpdated() override;
  void Initialize() override;
  void CloseBubble() override;
  void ShowBubble() override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  const char* GetClassName() const override;

  // PhoneStatusView::Delegate:
  bool CanOpenConnectedDeviceSettings() override;
  void OpenConnectedDevicesSettings() override;

  // OnboardingView::Delegate:
  void HideStatusHeaderView() override;

  // WindowTreeHostManager::Observer
  void OnDisplayConfigurationChanged() override;

  // AppStreamManager::Observer:
  void OnAppStreamUpdate(
      const phonehub::proto::AppStreamUpdate app_stream_update) override;

  void OnIconsDecoded(
      std::string visible_name,
      std::unique_ptr<std::vector<phonehub::IconDecoder::DecodingData>>
          decoding_data_list);

  // Provides the Eche icon and Eche loading indicator to
  // `EcheTray` in order to let `EcheTray` control the visibiliity
  // of them. Please note that these views are in control of 'EcheTray'
  // and the phone hub area is "borrowed" by `EcheTray` for the
  // purpose of grouping the icons together.
  views::ImageButton* eche_icon_view() { return eche_icon_; }
  EcheIconLoadingIndicatorView* eche_loading_indicator() {
    return eche_loading_indicator_;
  }

  // Sets a callback that will be called when eche icon is activated.
  void SetEcheIconActivationCallback(
      base::RepeatingCallback<bool(const ui::Event&)> callback);

  views::View* content_view_for_testing() { return content_view_; }

  PhoneHubUiController* ui_controller_for_testing() {
    return ui_controller_.get();
  }

  PhoneHubNudgeController* phone_hub_nudge_controller_for_testing() {
    return phone_hub_nudge_controller_.get();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(PhoneHubTrayTest, SafeAccessToHeaderView);

  // TrayBubbleView::Delegate:
  std::u16string GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // PhoneHubUiController::Observer:
  void OnPhoneHubUiStateChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // Updates the visibility of the tray in the shelf based on the feature is
  // enabled.
  void UpdateVisibility();
  void UpdateHeaderVisibility();

  // Disables the animation and enables it back after a 5s delay. This tray's
  // visibility can be updated when the connection is complete. After a session
  // has started (login/unlock/user-switch), a duration is added here to delay
  // the animation being enabled, since it would take a few seconds to get
  // connected.
  void TemporarilyDisableAnimation();

  // Button click/press handlers for main phone hub icon and secondary
  // Eche icon.
  void EcheIconActivated(const ui::Event& event);
  void PhoneHubIconActivated(const ui::Event& event);

  views::View* GetPhoneStatusView();

  // Icon of the tray. Unowned.
  raw_ptr<views::ImageButton, ExperimentalAsh> icon_;

  // Icon for Eche. Unowned.
  raw_ptr<views::ImageButton, ExperimentalAsh> eche_icon_ = nullptr;

  // The loading indicator, showing a throbber animation on top of the icon.
  raw_ptr<EcheIconLoadingIndicatorView, ExperimentalAsh>
      eche_loading_indicator_ = nullptr;

  // This callback is called when the Eche icon is activated.
  base::RepeatingCallback<bool(const ui::Event&)> eche_icon_callback_ =
      base::BindRepeating([](const ui::Event&) { return true; });

  // Controls the main content view displayed in the bubble based on the current
  // PhoneHub state.
  std::unique_ptr<PhoneHubUiController> ui_controller_;

  // Controls the behavior of a nudge shown to eligible users.
  std::unique_ptr<PhoneHubNudgeController> phone_hub_nudge_controller_;

  // The bubble that appears after clicking the tray button.
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  // The header status view on top of the bubble.
  // IMPORTANT: This is not owned, always access through GetPhoneStatusView
  raw_ptr<views::View, ExperimentalAsh> phone_status_view_dont_use_ = nullptr;

  // The main content view of the bubble, which changes depending on the state.
  // Unowned.
  raw_ptr<PhoneHubContentView, DanglingUntriaged | ExperimentalAsh>
      content_view_ = nullptr;

  raw_ptr<phonehub::PhoneHubManager, ExperimentalAsh> phone_hub_manager_ =
      nullptr;

  base::ScopedObservation<PhoneHubUiController, PhoneHubUiController::Observer>
      observed_phone_hub_ui_controller_{this};
  base::ScopedObservation<SessionControllerImpl, SessionObserver>
      observed_session_{this};

  base::WeakPtrFactory<PhoneHubTray> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_TRAY_H_
