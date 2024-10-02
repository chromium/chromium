// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_tray.h"
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/focus_cycler.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/shell.h"
#include "ash/system/phonehub/multidevice_feature_opt_in_view.h"
#include "ash/system/phonehub/phone_hub_ui_controller.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/toast/anchored_nudge.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/fake_connection_scheduler.h"
#include "chromeos/ash/components/phonehub/fake_icon_decoder.h"
#include "chromeos/ash/components/phonehub/fake_multidevice_feature_access_manager.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/phone_model_test_util.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace ash {

namespace {

using multidevice_setup::mojom::Feature;
using AccessStatus = phonehub::MultideviceFeatureAccessManager::AccessStatus;
using AccessProhibitedReason =
    phonehub::MultideviceFeatureAccessManager::AccessProhibitedReason;

constexpr base::TimeDelta kConnectingViewGracePeriod = base::Seconds(40);
constexpr char kTrayBackgroundViewHistogramName[] =
    "Ash.StatusArea.TrayBackgroundView.Pressed";
const std::string kPhoneHubNudgeId = "PhoneHubNudge";

// A mock implementation of |NewWindowDelegate| for use in tests.
class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

}  // namespace

class PhoneHubTrayTest : public AshTestBase {
 public:
  PhoneHubTrayTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~PhoneHubTrayTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kPhoneHub,
                              features::kPhoneHubCameraRoll,
                              features::kEcheLauncher, features::kEcheSWA,
                              features::kEcheNetworkConnectionState},
        /*disabled_features=*/{});
    AshTestBase::SetUp();

    phone_hub_tray_ =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->phone_hub_tray();
    // Disable pulse animation so the tests will not hang.
    ui::ScopedAnimationDurationScaleMode duration_mode(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

    GetFeatureStatusProvider()->SetStatus(
        phonehub::FeatureStatus::kEnabledAndConnected);
    phone_hub_tray_->SetPhoneHubManager(&phone_hub_manager_);

    phone_hub_manager_.mutable_phone_model()->SetPhoneStatusModel(
        phonehub::CreateFakePhoneStatusModel());
    phone_hub_manager_.fake_recent_apps_interaction_handler()
        ->set_ui_state_for_testing(phonehub::RecentAppsInteractionHandler::
                                       RecentAppsUiState::ITEMS_VISIBLE);
  }

  void TearDown() override {
    AshTestBase::TearDown();
  }

  phonehub::FakeFeatureStatusProvider* GetFeatureStatusProvider() {
    return phone_hub_manager_.fake_feature_status_provider();
  }

  phonehub::FakeMultideviceFeatureAccessManager*
  GetMultideviceFeatureAccessManager() {
    return phone_hub_manager_.fake_multidevice_feature_access_manager();
  }

  phonehub::FakeConnectionScheduler* GetConnectionScheduler() {
    return phone_hub_manager_.fake_connection_scheduler();
  }

  phonehub::FakeOnboardingUiTracker* GetOnboardingUiTracker() {
    return phone_hub_manager_.fake_onboarding_ui_tracker();
  }

  phonehub::AppStreamLauncherDataModel* GetAppStreamLauncherDataModel() {
    return phone_hub_manager_.fake_app_stream_launcher_data_model();
  }

  void ClickTrayButton() { LeftClickOn(phone_hub_tray_); }

  // When first connecting, the connecting view is shown for 30 seconds when
  // disconnected, so in order to show the disconnecting view, we need to fast
  // forward time.
  void FastForwardByConnectingViewGracePeriod() {
    task_environment()->FastForwardBy(kConnectingViewGracePeriod);
  }

  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }

  views::View* bubble_view() { return phone_hub_tray_->GetBubbleView(); }

  views::View* content_view() {
    return phone_hub_tray_->content_view_for_testing();
  }

  PhoneHubTray* phone_hub_tray() { return phone_hub_tray_; }

  MultideviceFeatureOptInView* multidevice_feature_opt_in_view() {
    return static_cast<MultideviceFeatureOptInView*>(bubble_view()->GetViewByID(
        PhoneHubViewID::kMultideviceFeatureOptInView));
  }

  views::View* onboarding_main_view() {
    return bubble_view()->GetViewByID(PhoneHubViewID::kOnboardingMainView);
  }

  views::Button* onboarding_get_started_button() {
    return static_cast<views::Button*>(bubble_view()->GetViewByID(
        PhoneHubViewID::kOnboardingGetStartedButton));
  }

  views::Button* disconnected_refresh_button() {
    return static_cast<views::Button*>(
        bubble_view()->GetViewByID(PhoneHubViewID::kDisconnectedRefreshButton));
  }

  views::Button* disconnected_learn_more_button() {
    return static_cast<views::Button*>(bubble_view()->GetViewByID(
        PhoneHubViewID::kDisconnectedLearnMoreButton));
  }

  views::Button* bluetooth_disabled_learn_more_button() {
    return static_cast<views::Button*>(bubble_view()->GetViewByID(
        PhoneHubViewID::kBluetoothDisabledLearnMoreButton));
  }

  views::Button* notification_opt_in_set_up_button() {
    return static_cast<views::Button*>(bubble_view()->GetViewByID(
        PhoneHubViewID::kSubFeatureOptInConfirmButton));
  }

  views::Button* notification_opt_in_dismiss_button() {
    return static_cast<views::Button*>(bubble_view()->GetViewByID(
        PhoneHubViewID::kSubFeatureOptInDismissButton));
  }

 protected:
  raw_ptr<PhoneHubTray, DanglingUntriaged> phone_hub_tray_ = nullptr;
  phonehub::FakePhoneHubManager phone_hub_manager_;
  base::test::ScopedFeatureList feature_list_;
  MockNewWindowDelegate new_window_delegate_;
};

TEST_F(PhoneHubTrayTest, SetPhoneHubManager) {
  // Set a new manager.
  phonehub::FakePhoneHubManager new_manager;
  new_manager.fake_feature_status_provider()->SetStatus(
      phonehub::FeatureStatus::kEnabledAndConnected);
  phone_hub_tray_->SetPhoneHubManager(&new_manager);
  EXPECT_TRUE(phone_hub_tray_->GetVisible());

  // Changing the old manager should have no effect.
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kNotEligibleForFeature);
  EXPECT_TRUE(phone_hub_tray_->GetVisible());

  // Only the new manager should work.
  new_manager.fake_feature_status_provider()->SetStatus(
      phonehub::FeatureStatus::kNotEligibleForFeature);
  EXPECT_FALSE(phone_hub_tray_->GetVisible());

  // Set no manager.
  phone_hub_tray_->SetPhoneHubManager(nullptr);
  EXPECT_FALSE(phone_hub_tray_->GetVisible());
}

TEST_F(PhoneHubTrayTest, ClickTrayButton) {
  EXPECT_TRUE(phone_hub_tray_->GetVisible());

  EXPECT_FALSE(phone_hub_tray_->is_active());

  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());
  // We only want to focus the bubble widget when it's opened by keyboard.
  EXPECT_FALSE(phone_hub_tray_->GetBubbleView()->GetWidget()->IsActive());

  ClickTrayButton();
  EXPECT_FALSE(phone_hub_tray_->is_active());
}

TEST_F(PhoneHubTrayTest, FocusBubbleWhenOpenedByKeyboard) {
  EXPECT_TRUE(phone_hub_tray_->GetVisible());

  Shell::Get()->focus_cycler()->FocusWidget(phone_hub_tray_->GetWidget());
  phone_hub_tray_->RequestFocus();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);

  // Generate a tab key press.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);

  // The bubble widget should get focus when it's opened by keyboard and the tab
  // key is pressed.
  EXPECT_TRUE(phone_hub_tray_->is_active());
  EXPECT_TRUE(phone_hub_tray_->GetBubbleView()->GetWidget()->IsActive());
}

TEST_F(PhoneHubTrayTest, ShowOptInViewWhenNotificationAccessNotGranted) {
  GetMultideviceFeatureAccessManager()->SetNotificationAccessStatusInternal(
      AccessStatus::kAvailableButNotGranted, AccessProhibitedReason::kUnknown);
  GetMultideviceFeatureAccessManager()->SetCameraRollAccessStatusInternal(
      AccessStatus::kAccessGranted);

  ClickTrayButton();

  EXPECT_TRUE(multidevice_feature_opt_in_view());
  EXPECT_TRUE(multidevice_feature_opt_in_view()->GetVisible());

  // Simulate a click on "Dismiss" button.
  LeftClickOn(notification_opt_in_dismiss_button());

  // Clicking on "Dismiss" should hide the view and also disable the ability to
  // show it again.
  EXPECT_FALSE(multidevice_feature_opt_in_view()->GetVisible());
  EXPECT_TRUE(GetMultideviceFeatureAccessManager()
                  ->HasMultideviceFeatureSetupUiBeenDismissed());
}

TEST_F(PhoneHubTrayTest, ShowOptInViewWhenCameraRollAccessNotGranted) {
  GetMultideviceFeatureAccessManager()->SetNotificationAccessStatusInternal(
      AccessStatus::kAccessGranted, AccessProhibitedReason::kUnknown);
  GetMultideviceFeatureAccessManager()->SetCameraRollAccessStatusInternal(
      AccessStatus::kAvailableButNotGranted);
  GetMultideviceFeatureAccessManager()->SetFeatureReadyForAccess(
      Feature::kPhoneHubCameraRoll);

  ClickTrayButton();

  EXPECT_TRUE(multidevice_feature_opt_in_view());
  EXPECT_TRUE(multidevice_feature_opt_in_view()->GetVisible());

  // Simulate a click on "Dismiss" button.
  LeftClickOn(notification_opt_in_dismiss_button());

  // Clicking on "Dismiss" should hide the view and also disable the ability to
  // show it again.
  EXPECT_FALSE(multidevice_feature_opt_in_view()->GetVisible());
  EXPECT_TRUE(GetMultideviceFeatureAccessManager()
                  ->HasMultideviceFeatureSetupUiBeenDismissed());
}

TEST_F(PhoneHubTrayTest, HideOptInViewWhenAllFeatureAccessHasBeenGranted) {
  GetMultideviceFeatureAccessManager()->SetNotificationAccessStatusInternal(
      AccessStatus::kAccessGranted, AccessProhibitedReason::kUnknown);
  GetMultideviceFeatureAccessManager()->SetCameraRollAccessStatusInternal(
      AccessStatus::kAccessGranted);

  ClickTrayButton();

  EXPECT_TRUE(multidevice_feature_opt_in_view());
  EXPECT_FALSE(multidevice_feature_opt_in_view()->GetVisible());
}

TEST_F(
    PhoneHubTrayTest,
    HideOptInViewWhenNotificationAccessIsProhibitedAndCameraRollAccessIsGranted) {
  GetMultideviceFeatureAccessManager()->SetNotificationAccessStatusInternal(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  GetMultideviceFeatureAccessManager()->SetCameraRollAccessStatusInternal(
      AccessStatus::kAccessGranted);

  ClickTrayButton();

  EXPECT_TRUE(multidevice_feature_opt_in_view());
  EXPECT_FALSE(multidevice_feature_opt_in_view()->GetVisible());
}

TEST_F(PhoneHubTrayTest, StartMultideviceFeatureSetUpFlow) {
  GetMultideviceFeatureAccessManager()->SetNotificationAccessStatusInternal(
      AccessStatus::kAvailableButNotGranted, AccessProhibitedReason::kUnknown);
  GetMultideviceFeatureAccessManager()->SetCameraRollAccessStatusInternal(
      AccessStatus::kAvailableButNotGranted);
  GetMultideviceFeatureAccessManager()->SetFeatureReadyForAccess(
      Feature::kPhoneHubCameraRoll);

  ClickTrayButton();
  EXPECT_TRUE(multidevice_feature_opt_in_view());
  EXPECT_TRUE(multidevice_feature_opt_in_view()->GetVisible());

  // Clicking on the set up button should open the corresponding settings page
  // for the notification set up flow.
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL("chrome://os-settings/multidevice/"
                           "features?showPhonePermissionSetupDialog&mode=5"),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));

  LeftClickOn(notification_opt_in_set_up_button());

  // Simulate that notification access has been granted.
  GetMultideviceFeatureAccessManager()->SetNotificationAccessStatusInternal(
      AccessStatus::kAccessGranted, AccessProhibitedReason::kUnknown);
  // Simulate that camera roll access has been granted.
  GetMultideviceFeatureAccessManager()->SetCameraRollAccessStatusInternal(
      AccessStatus::kAccessGranted);

  // Bubble has been dismissed, opening again.
  ClickTrayButton();

  // This view should be dismissed.
  EXPECT_FALSE(multidevice_feature_opt_in_view()->GetVisible());

  // Simulate that notification access has been revoked by the phone.
  GetMultideviceFeatureAccessManager()->SetNotificationAccessStatusInternal(
      AccessStatus::kAvailableButNotGranted, AccessProhibitedReason::kUnknown);

  // This view should show up again.
  EXPECT_TRUE(multidevice_feature_opt_in_view()->GetVisible());
}

TEST_F(PhoneHubTrayTest, StartAllPermissionSetUpFlow) {
  GetMultideviceFeatureAccessManager()->SetNotificationAccessStatusInternal(
      AccessStatus::kAvailableButNotGranted, AccessProhibitedReason::kUnknown);
  GetMultideviceFeatureAccessManager()->SetCameraRollAccessStatusInternal(
      AccessStatus::kAvailableButNotGranted);
  GetMultideviceFeatureAccessManager()->SetAppsAccessStatusInternal(
      AccessStatus::kAvailableButNotGranted);
  GetMultideviceFeatureAccessManager()->SetFeatureReadyForAccess(
      Feature::kEche);
  GetMultideviceFeatureAccessManager()->SetFeatureReadyForAccess(
      Feature::kPhoneHubCameraRoll);

  ClickTrayButton();
  EXPECT_TRUE(multidevice_feature_opt_in_view());
  EXPECT_TRUE(multidevice_feature_opt_in_view()->GetVisible());

  // Clicking on the set up button should open the corresponding settings page
  // for the notification set up flow.
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL("chrome://os-settings/multidevice/"
                           "features?showPhonePermissionSetupDialog&mode=7"),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));

  LeftClickOn(notification_opt_in_set_up_button());
}

TEST_F(PhoneHubTrayTest, StartNotificationAndAppSetUpFlow) {
  GetMultideviceFeatureAccessManager()->SetNotificationAccessStatusInternal(
      AccessStatus::kAvailableButNotGranted, AccessProhibitedReason::kUnknown);
  GetMultideviceFeatureAccessManager()->SetAppsAccessStatusInternal(
      AccessStatus::kAvailableButNotGranted);
  GetMultideviceFeatureAccessManager()->SetCameraRollAccessStatusInternal(
      AccessStatus::kAccessGranted);
  GetMultideviceFeatureAccessManager()->SetFeatureReadyForAccess(
      Feature::kEche);

  ClickTrayButton();
  EXPECT_TRUE(multidevice_feature_opt_in_view());
  EXPECT_TRUE(multidevice_feature_opt_in_view()->GetVisible());

  // Clicking on the set up button should open the corresponding settings page
  // for the notification set up flow.
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL("chrome://os-settings/multidevice/"
                           "features?showPhonePermissionSetupDialog&mode=4"),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));

  LeftClickOn(notification_opt_in_set_up_button());
}

TEST_F(PhoneHubTrayTest, StartNotificationAccessOnlySetUpFlow) {
  GetMultideviceFeatureAccessManager()->SetNotificationAccessStatusInternal(
      AccessStatus::kAvailableButNotGranted, AccessProhibitedReason::kUnknown);
  GetMultideviceFeatureAccessManager()->SetAppsAccessStatusInternal(
      AccessStatus::kAccessGranted);
  GetMultideviceFeatureAccessManager()->SetCameraRollAccessStatusInternal(
      AccessStatus::kAccessGranted);

  ClickTrayButton();
  EXPECT_TRUE(multidevice_feature_opt_in_view());
  EXPECT_TRUE(multidevice_feature_opt_in_view()->GetVisible());

  // Clicking on the set up button should open the corresponding settings page
  // for the notification set up flow.
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL("chrome://os-settings/multidevice/"
                           "features?showPhonePermissionSetupDialog&mode=1"),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));

  LeftClickOn(notification_opt_in_set_up_button());
}

TEST_F(PhoneHubTrayTest, StartAppsAccessOnlySetUpFlow) {
  GetMultideviceFeatureAccessManager()->SetNotificationAccessStatusInternal(
      AccessStatus::kAccessGranted, AccessProhibitedReason::kUnknown);
  GetMultideviceFeatureAccessManager()->SetCameraRollAccessStatusInternal(
      AccessStatus::kAccessGranted);
  GetMultideviceFeatureAccessManager()->SetAppsAccessStatusInternal(
      AccessStatus::kAvailableButNotGranted);
  GetMultideviceFeatureAccessManager()->SetFeatureReadyForAccess(
      Feature::kEche);

  ClickTrayButton();
  EXPECT_TRUE(multidevice_feature_opt_in_view());
  EXPECT_TRUE(multidevice_feature_opt_in_view()->GetVisible());

  // Clicking on the set up button should open the corresponding settings page
  // for the notification set up flow.
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL("chrome://os-settings/multidevice/"
                           "features?showPhonePermissionSetupDialog&mode=2"),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));

  LeftClickOn(notification_opt_in_set_up_button());
}

TEST_F(PhoneHubTrayTest, DoNotShowAppsAccessSetUpFlowIfFeatureIsNotReady) {
  GetMultideviceFeatureAccessManager()->SetNotificationAccessStatusInternal(
      AccessStatus::kAccessGranted, AccessProhibitedReason::kUnknown);
  GetMultideviceFeatureAccessManager()->SetCameraRollAccessStatusInternal(
      AccessStatus::kAccessGranted);
  GetMultideviceFeatureAccessManager()->SetAppsAccessStatusInternal(
      AccessStatus::kAvailableButNotGranted);

  ClickTrayButton();
  EXPECT_TRUE(multidevice_feature_opt_in_view());
  EXPECT_FALSE(multidevice_feature_opt_in_view()->GetVisible());
}

TEST_F(PhoneHubTrayTest, StartCameraRollOnlySetUpFlow) {
  GetMultideviceFeatureAccessManager()->SetNotificationAccessStatusInternal(
      AccessStatus::kAccessGranted, AccessProhibitedReason::kUnknown);
  GetMultideviceFeatureAccessManager()->SetCameraRollAccessStatusInternal(
      AccessStatus::kAvailableButNotGranted);
  GetMultideviceFeatureAccessManager()->SetAppsAccessStatusInternal(
      AccessStatus::kAccessGranted);
  GetMultideviceFeatureAccessManager()->SetFeatureReadyForAccess(
      Feature::kPhoneHubCameraRoll);

  ClickTrayButton();
  EXPECT_TRUE(multidevice_feature_opt_in_view());
  EXPECT_TRUE(multidevice_feature_opt_in_view()->GetVisible());

  // Clicking on the set up button should open the corresponding settings page
  // for the notification set up flow.
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL("chrome://os-settings/multidevice/"
                           "features?showPhonePermissionSetupDialog&mode=3"),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));

  LeftClickOn(notification_opt_in_set_up_button());
}

TEST_F(PhoneHubTrayTest, HideTrayItemOnUiStateChange) {
  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());

  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kNotEligibleForFeature);

  EXPECT_FALSE(phone_hub_tray_->is_active());
  EXPECT_FALSE(phone_hub_tray_->GetVisible());
}

TEST_F(PhoneHubTrayTest, TransitionContentView) {
  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());

  EXPECT_TRUE(content_view());
  EXPECT_EQ(PhoneHubViewID::kPhoneConnectedView, content_view()->GetID());

  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kEnabledButDisconnected);
  FastForwardByConnectingViewGracePeriod();

  EXPECT_TRUE(content_view());
  EXPECT_EQ(PhoneHubViewID::kDisconnectedView, content_view()->GetID());
}

TEST_F(PhoneHubTrayTest, StartOnboardingFlow) {
  // Simulate a pending setup state to show the onboarding screen.
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kEligiblePhoneButNotSetUp);
  GetOnboardingUiTracker()->SetShouldShowOnboardingUi(true);

  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());
  EXPECT_EQ(PhoneHubViewID::kOnboardingView, content_view()->GetID());
  // It should display the onboarding main view.
  EXPECT_TRUE(onboarding_main_view());
  EXPECT_TRUE(onboarding_main_view()->GetVisible());
  EXPECT_EQ(0u, GetOnboardingUiTracker()->handle_get_started_call_count());

  // Simulate a click on the "Get started" button.
  LeftClickOn(onboarding_get_started_button());
  // It should invoke the |HandleGetStarted| call.
  EXPECT_EQ(1u, GetOnboardingUiTracker()->handle_get_started_call_count());
}

TEST_F(PhoneHubTrayTest, DismissOnboardingFlowByRightClickIcon) {
  // Simulate a pending setup state to show the onboarding screen.
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kEligiblePhoneButNotSetUp);
  GetOnboardingUiTracker()->SetShouldShowOnboardingUi(true);

  RightClickOn(phone_hub_tray_);
  EXPECT_TRUE(views::MenuController::GetActiveInstance());
  views::MenuItemView* menu_item_view =
      views::MenuController::GetActiveInstance()
          ->GetSelectedMenuItem()
          ->GetMenuItemByID(/*kHidePhoneHubIconCommandId*/ 1);
  LeftClickOn(menu_item_view);

  // Clicking "Ok, got it" button should dismiss the bubble, hide the tray icon,
  // and disable the ability to show onboarding UI again.
  EXPECT_FALSE(phone_hub_tray_->GetBubbleView());
  EXPECT_FALSE(phone_hub_tray_->GetVisible());
  EXPECT_FALSE(GetOnboardingUiTracker()->ShouldShowOnboardingUi());
}

TEST_F(PhoneHubTrayTest, ShouldNotShowMiniLauncherOnCloseBubble) {
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kEnabledAndConnected);

  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());

  // Simulate showing the app stream mini launcher
  GetAppStreamLauncherDataModel()->SetShouldShowMiniLauncher(true);
  EXPECT_TRUE(GetAppStreamLauncherDataModel()->GetShouldShowMiniLauncher());

  // Simulate a click outside the bubble.
  const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  phone_hub_tray_->ClickedOutsideBubble(event);

  // Clicking outside should dismiss the bubble and should not show the app
  // stream mini launcher.
  EXPECT_FALSE(phone_hub_tray_->GetBubbleView());
  EXPECT_TRUE(phone_hub_tray_->GetVisible());
  EXPECT_FALSE(GetAppStreamLauncherDataModel()->GetShouldShowMiniLauncher());

  // Opening the bubble again should still have the app stream mini launcher
  // not shown.
  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->GetBubbleView());
  EXPECT_TRUE(phone_hub_tray_->GetVisible());
  EXPECT_FALSE(GetAppStreamLauncherDataModel()->GetShouldShowMiniLauncher());
}

TEST_F(PhoneHubTrayTest, ClickButtonsOnDisconnectedView) {
  // Simulates a phone disconnected error state to show the disconnected view.
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kEnabledButDisconnected);
  FastForwardByConnectingViewGracePeriod();

  EXPECT_EQ(0u, GetConnectionScheduler()->num_schedule_connection_now_calls());

  // In error state, clicking the tray button should schedule a connection
  // attempt.
  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());
  EXPECT_EQ(1u, GetConnectionScheduler()->num_schedule_connection_now_calls());

  // Make sure the testing environment is in disconnected view.
  EXPECT_TRUE(content_view());
  EXPECT_EQ(PhoneHubViewID::kDisconnectedView, content_view()->GetID());

  // Simulates a click on the "Refresh" button.
  LeftClickOn(disconnected_refresh_button());

  // Clicking "Refresh" button should schedule a connection attempt.
  EXPECT_EQ(2u, GetConnectionScheduler()->num_schedule_connection_now_calls());

  // Clicking "Learn More" button should open the corresponding help center
  // article in a browser tab.
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL("https://support.google.com/chromebook?p=phone_hub"),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));

  // Simulates a click on the "Learn more" button.
  LeftClickOn(disconnected_learn_more_button());
}

TEST_F(PhoneHubTrayTest, ClickButtonOnBluetoothDisabledView) {
  // Simulate a Bluetooth unavailable state.
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kUnavailableBluetoothOff);
  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());
  EXPECT_EQ(PhoneHubViewID::kBluetoothDisabledView, content_view()->GetID());

  // Clicking "Learn more" button should open the corresponding help center
  // article in a browser tab.
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL("https://support.google.com/chromebook?p=phone_hub"),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));
  // Simulate a click on "Learn more" button.
  LeftClickOn(bluetooth_disabled_learn_more_button());
}

TEST_F(PhoneHubTrayTest, CloseBubbleWhileShowingSameView) {
  // Simulate the views returned to PhoneHubTray are the same and open and
  // close tray.
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kEnabledAndConnecting);
  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());
  EXPECT_EQ(PhoneHubViewID::kPhoneConnectingView, content_view()->GetID());
  ClickTrayButton();
  EXPECT_FALSE(phone_hub_tray_->is_active());
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kEnabledButDisconnected);
  EXPECT_FALSE(content_view());
}

TEST_F(PhoneHubTrayTest, OnSessionChanged) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Disable the tray first.
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kNotEligibleForFeature);
  FastForwardByConnectingViewGracePeriod();
  EXPECT_FALSE(phone_hub_tray_->GetVisible());

  // Enable it to let it visible.
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kEnabledAndConnected);
  task_environment()->FastForwardBy(base::Seconds(3));
  EXPECT_TRUE(phone_hub_tray_->GetVisible());
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  EXPECT_TRUE(phone_hub_tray_->GetVisible());

  // Animation is disabled for 5 seconds after the session state get changed.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  // Gives it a small duration to let the session get changed. This duration is
  // way smaller than the animation duration, so that the animation will not
  // finish when this duration ends. The same for the other places below.
  task_environment()->FastForwardBy(base::Milliseconds(20));
  for (int i = 0; i < 3; i++) {
    SCOPED_TRACE(::testing::Message() << "iteration=" << i);
    EXPECT_FALSE(phone_hub_tray_->layer()->GetAnimator()->is_animating());
    EXPECT_TRUE(phone_hub_tray_->GetVisible());
    GetFeatureStatusProvider()->SetStatus(
        phonehub::FeatureStatus::kNotEligibleForFeature);
    FastForwardByConnectingViewGracePeriod();
    EXPECT_FALSE(phone_hub_tray_->GetVisible());
    GetFeatureStatusProvider()->SetStatus(
        phonehub::FeatureStatus::kEnabledAndConnected);
    task_environment()->FastForwardBy(base::Seconds(3));
  }
  EXPECT_FALSE(phone_hub_tray_->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(phone_hub_tray_->GetVisible());

  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kNotEligibleForFeature);
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kEnabledAndConnected);
  EXPECT_TRUE(phone_hub_tray_->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(phone_hub_tray_->GetVisible());
}

// This is a test to check for use-after-free error on accessing
// a possible dangling reference to `phone_status_view`.
TEST_F(PhoneHubTrayTest, SafeAccessToHeaderView) {
  phone_hub_tray_->ShowBubble();

  // Bubble is closed w/o calling `phone_hub_tray_->CloseBubble()`
  phone_hub_tray_->GetBubbleWidget()->CloseNow();

  // Make sure it does not cause a UAF error.This is caught by ASAN (go/asan)
  phone_hub_tray_->UpdateHeaderVisibility();
}

TEST_F(PhoneHubTrayTest, MultiDisplay) {
  // Connect a second display, make sure the phone hub tray is shown still.
  UpdateDisplay("500x400,500x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2U, root_windows.size());

  auto* secondary_phone_hub_tray =
      StatusAreaWidgetTestHelper::GetSecondaryStatusAreaWidget()
          ->phone_hub_tray();
  secondary_phone_hub_tray->SetPhoneHubManager(&phone_hub_manager_);

  EXPECT_TRUE(phone_hub_tray_->GetVisible());
  EXPECT_TRUE(secondary_phone_hub_tray->GetVisible());
}

TEST_F(PhoneHubTrayTest,
       PhoneHubNotShownOnMoreThanFiveMinutesAfterSessionStartTime) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kNotEligibleForFeature);

  // Set time to fifteen minutes after session start time.
  task_environment()->AdvanceClock(base::TimeDelta(base::Minutes(15)));
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kEligiblePhoneButNotSetUp);
  GetOnboardingUiTracker()->SetShouldShowOnboardingUi(true);
  EXPECT_FALSE(phone_hub_tray_->GetVisible());

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  EXPECT_FALSE(phone_hub_tray_->GetVisible());

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(phone_hub_tray_->GetVisible());
}

TEST_F(PhoneHubTrayTest, ShowPhoneHubOnlyUpToFiveMinutesAfterSessionStartTime) {
  // Reset session start time.
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kNotEligibleForFeature);
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Set time to three minutes after session start time.
  task_environment()->AdvanceClock(base::TimeDelta(base::Minutes(3)));
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kEligiblePhoneButNotSetUp);
  GetOnboardingUiTracker()->SetShouldShowOnboardingUi(true);
  EXPECT_TRUE(phone_hub_tray_->GetVisible());
}

TEST_F(PhoneHubTrayTest, ShowAndHideNudge) {
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kEligiblePhoneButNotSetUp);
  GetOnboardingUiTracker()->SetShouldShowOnboardingUi(true);
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  EXPECT_TRUE(
      Shell::Get()->anchored_nudge_manager()->IsNudgeShown(kPhoneHubNudgeId));

  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());
  EXPECT_EQ(PhoneHubViewID::kOnboardingView, content_view()->GetID());
  // It should display the onboarding main view.
  EXPECT_TRUE(onboarding_main_view());
  EXPECT_TRUE(onboarding_main_view()->GetVisible());
  EXPECT_EQ(0u, GetOnboardingUiTracker()->handle_get_started_call_count());

  // Simulate a click on the "Get started" button.
  LeftClickOn(onboarding_get_started_button());
  // It should invoke the |HandleGetStarted| call.
  EXPECT_EQ(1u, GetOnboardingUiTracker()->handle_get_started_call_count());
  EXPECT_TRUE(GetOnboardingUiTracker()->is_icon_clicked_when_nudge_visible());
  EXPECT_FALSE(
      Shell::Get()->anchored_nudge_manager()->IsNudgeShown(kPhoneHubNudgeId));
}

TEST_F(PhoneHubTrayTest, EcheIconActivatesCallback) {
  bool launched_app_window = false;
  phone_hub_tray_->SetEcheIconActivationCallback(
      base::BindLambdaForTesting([&]() { launched_app_window = true; }));
  phone_hub_tray_->OnAppStreamUpdate(phonehub::proto::AppStreamUpdate());
  phone_hub_manager_.fake_icon_decoder()->FinishLastCall();

  LeftClickOn(phone_hub_tray_->eche_icon_);

  EXPECT_TRUE(launched_app_window);
}

// Makes sure metrics are recorded for the phone hub tray or any nested button
// being pressed.
TEST_F(PhoneHubTrayTest, TrayPressedMetrics) {
  base::HistogramTester histogram_tester;

  LeftClickOn(phone_hub_tray());
  histogram_tester.ExpectTotalCount(kTrayBackgroundViewHistogramName, 1);

  LeftClickOn(phone_hub_tray()->icon_);
  histogram_tester.ExpectTotalCount(kTrayBackgroundViewHistogramName, 2);

  LeftClickOn(phone_hub_tray()->eche_icon_);
  histogram_tester.ExpectTotalCount(kTrayBackgroundViewHistogramName, 3);
}

}  // namespace ash
