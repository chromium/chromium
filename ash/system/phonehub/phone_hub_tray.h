// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_TRAY_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_TRAY_H_

#include "ash/ash_export.h"
#include "ash/session/session_controller_impl.h"
#include "ash/system/phonehub/onboarding_view.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
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
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/image_button.h"

namespace views {
class ImageButton;
}
namespace ash {

class EcheIconLoadingIndicatorView;
class OnboardingNudgeController;
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
                                public ui::SimpleMenuModel::Delegate,
                                public SessionObserver,
                                public display::DisplayManagerObserver,
                                public phonehub::AppStreamManager::Observer {
  METADATA_HEADER(PhoneHubTray, TrayBackgroundView)

 public:
  explicit PhoneHubTray(Shelf* shelf);
  PhoneHubTray(const PhoneHubTray&) = delete;
  PhoneHubTray& operator=(const PhoneHubTray&) = delete;
  ~PhoneHubTray() override;

  // Sets the PhoneHubManager that provides the data to drive the UI.
  void SetPhoneHubManager(phonehub::PhoneHubManager* phone_hub_manager);

  // TrayBackgroundView:
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override;
  void UpdateTrayItemColor(bool is_active) override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void AnchorUpdated() override;
  void Initialize() override;
  void CloseBubbleInternal() override;
  void ShowBubble() override;
  std::unique_ptr<ui::SimpleMenuModel> CreateContextMenuModel() override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;

  // PhoneStatusView::Delegate:
  bool CanOpenConnectedDeviceSettings() override;
  void OpenConnectedDevicesSettings() override;

  // OnboardingView::Delegate:
  void HideStatusHeaderView() override;
  bool IsPhoneHubIconClickedWhenNudgeVisible() override;

  // display::DisplayManagerObserver
  void OnDidApplyDisplayChanges() override;

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
  void SetEcheIconActivationCallback(base::RepeatingCallback<void()> callback);

  views::View* content_view_for_testing() { return content_view_; }

  PhoneHubUiController* ui_controller() { return ui_controller_.get(); }

  OnboardingNudgeController* onboarding_nudge_controller_for_testing() {
    return onboarding_nudge_controller_.get();
  }

 protected:
  // TrayBackgroundView:
  void OnVisibilityAnimationFinished(bool should_log_visible_pod_count,
                                     bool aborted) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PhoneHubTrayTest, EcheIconActivatesCallback);
  FRIEND_TEST_ALL_PREFIXES(PhoneHubTrayTest, SafeAccessToHeaderView);
  FRIEND_TEST_ALL_PREFIXES(PhoneHubTrayTest, TrayPressedMetrics);

  // TrayBubbleView::Delegate:
  std::u16string GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // PhoneHubUiController::Observer:
  void OnPhoneHubUiStateChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // Ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

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

  // Checks if nudge should be shown based on user login time.
  bool IsInsideUnlockWindow();

  bool IsInPhoneHubNudgeExperimentGroup();

  bool is_icon_clicked_when_setup_notification_visible_ = false;

  bool is_icon_clicked_when_nudge_visible_ = false;

  // Icon of the tray. Unowned.
  raw_ptr<views::ImageButton> icon_;

  // Icon for Eche. Unowned.
  raw_ptr<views::ImageButton> eche_icon_ = nullptr;

  // The loading indicator, showing a throbber animation on top of the icon.
  raw_ptr<EcheIconLoadingIndicatorView> eche_loading_indicator_ = nullptr;

  // This callback is called when the Eche icon is activated.
  base::RepeatingCallback<void()> eche_icon_callback_ = base::DoNothing();

  // Controls the main content view displayed in the bubble based on the current
  // PhoneHub state.
  std::unique_ptr<PhoneHubUiController> ui_controller_;

  // Controls the behavior of a nudge shown to eligible users.
  std::unique_ptr<OnboardingNudgeController> onboarding_nudge_controller_;

  // The bubble that appears after clicking the tray button.
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  // The header status view on top of the bubble.
  // IMPORTANT: This is not owned, always access through GetPhoneStatusView
  raw_ptr<views::View> phone_status_view_dont_use_ = nullptr;

  // The main content view of the bubble, which changes depending on the state.
  // Unowned.
  raw_ptr<PhoneHubContentView, DanglingUntriaged> content_view_ = nullptr;

  raw_ptr<phonehub::PhoneHubManager> phone_hub_manager_ = nullptr;

  base::Time last_unlocked_timestamp_;

  base::ScopedObservation<PhoneHubUiController, PhoneHubUiController::Observer>
      observed_phone_hub_ui_controller_{this};
  base::ScopedObservation<SessionControllerImpl, SessionObserver>
      observed_session_{this};

  base::WeakPtrFactory<PhoneHubTray> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_TRAY_H_
