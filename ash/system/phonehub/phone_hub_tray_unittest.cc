// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_tray.h"

#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/system/phonehub/notification_opt_in_view.h"
#include "ash/system/phonehub/phone_hub_ui_controller.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/phonehub/fake_connection_scheduler.h"
#include "chromeos/components/phonehub/fake_notification_access_manager.h"
#include "chromeos/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/components/phonehub/phone_model_test_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/button.h"

namespace ash {

namespace {

// A mock implementation of |NewWindowDelegate| for use in tests.
class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              NewTabWithUrl,
              (const GURL& url, bool from_user_interaction),
              (override));
};

}  // namespace

class PhoneHubTrayTest : public AshTestBase {
 public:
  PhoneHubTrayTest() = default;
  ~PhoneHubTrayTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(chromeos::features::kPhoneHub);
    AshTestBase::SetUp();

    phone_hub_tray_ =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->phone_hub_tray();

    GetFeatureStatusProvider()->SetStatus(
        chromeos::phonehub::FeatureStatus::kEnabledAndConnected);
    phone_hub_tray_->SetPhoneHubManager(&phone_hub_manager_);

    phone_hub_manager_.mutable_phone_model()->SetPhoneStatusModel(
        chromeos::phonehub::CreateFakePhoneStatusModel());
  }

  chromeos::phonehub::FakeFeatureStatusProvider* GetFeatureStatusProvider() {
    return phone_hub_manager_.fake_feature_status_provider();
  }

  chromeos::phonehub::FakeNotificationAccessManager*
  GetNotificationAccessManager() {
    return phone_hub_manager_.fake_notification_access_manager();
  }

  chromeos::phonehub::FakeConnectionScheduler* GetConnectionScheduler() {
    return phone_hub_manager_.fake_connection_scheduler();
  }

  chromeos::phonehub::FakeOnboardingUiTracker* GetOnboardingUiTracker() {
    return phone_hub_manager_.fake_onboarding_ui_tracker();
  }

  // Simulate a mouse click on the given view.
  // Waits for the event to be processed.
  void ClickOnAndWait(const views::View* view) {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
    generator->ClickLeftButton();

    base::RunLoop().RunUntilIdle();
  }

  void PressReturnKeyOnTrayButton() {
    const ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_RETURN,
                                 ui::EF_NONE);
    phone_hub_tray_->PerformAction(key_event);
  }

  void ClickTrayButton() { ClickOnAndWait(phone_hub_tray_); }

  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }

  views::View* bubble_view() { return phone_hub_tray_->GetBubbleView(); }

  views::View* content_view() {
    return phone_hub_tray_->content_view_for_testing();
  }

  NotificationOptInView* notification_opt_in_view() {
    return static_cast<NotificationOptInView*>(
        bubble_view()->GetViewByID(PhoneHubViewID::kNotificationOptInView));
  }

  views::View* onboarding_main_view() {
    return bubble_view()->GetViewByID(PhoneHubViewID::kOnboardingMainView);
  }

  views::View* onboarding_dismiss_prompt_view() {
    return bubble_view()->GetViewByID(
        PhoneHubViewID::kOnboardingDismissPromptView);
  }

  views::Button* onboarding_get_started_button() {
    return static_cast<views::Button*>(bubble_view()->GetViewByID(
        PhoneHubViewID::kOnboardingGetStartedButton));
  }

  views::Button* onboarding_dismiss_button() {
    return static_cast<views::Button*>(
        bubble_view()->GetViewByID(PhoneHubViewID::kOnboardingDismissButton));
  }

  views::Button* onboarding_dismiss_ack_button() {
    return static_cast<views::Button*>(bubble_view()->GetViewByID(
        PhoneHubViewID::kOnboardingDismissAckButton));
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
        PhoneHubViewID::kNotificationOptInSetUpButton));
  }

  views::Button* notification_opt_in_dismiss_button() {
    return static_cast<views::Button*>(bubble_view()->GetViewByID(
        PhoneHubViewID::kNotificationOptInDismissButton));
  }

 protected:
  PhoneHubTray* phone_hub_tray_ = nullptr;
  chromeos::phonehub::FakePhoneHubManager phone_hub_manager_;
  base::test::ScopedFeatureList feature_list_;
  MockNewWindowDelegate new_window_delegate_;
};

TEST_F(PhoneHubTrayTest, SetPhoneHubManager) {
  // Set a new manager.
  chromeos::phonehub::FakePhoneHubManager new_manager;
  new_manager.fake_feature_status_provider()->SetStatus(
      chromeos::phonehub::FeatureStatus::kEnabledAndConnected);
  phone_hub_tray_->SetPhoneHubManager(&new_manager);
  EXPECT_TRUE(phone_hub_tray_->GetVisible());

  // Changing the old manager should have no effect.
  GetFeatureStatusProvider()->SetStatus(
      chromeos::phonehub::FeatureStatus::kNotEligibleForFeature);
  EXPECT_TRUE(phone_hub_tray_->GetVisible());

  // Only the new manager should work.
  new_manager.fake_feature_status_provider()->SetStatus(
      chromeos::phonehub::FeatureStatus::kNotEligibleForFeature);
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
  PressReturnKeyOnTrayButton();

  // The bubble widget should get focus when it's opened by keyboard.
  EXPECT_TRUE(phone_hub_tray_->is_active());
  EXPECT_TRUE(phone_hub_tray_->GetBubbleView()->GetWidget()->IsActive());
}

TEST_F(PhoneHubTrayTest, ShowNotificationOptInViewWhenAccessNotGranted) {
  GetNotificationAccessManager()->SetAccessStatusInternal(
      chromeos::phonehub::NotificationAccessManager::AccessStatus::
          kAvailableButNotGranted);

  ClickTrayButton();

  EXPECT_TRUE(notification_opt_in_view());
  EXPECT_TRUE(notification_opt_in_view()->GetVisible());

  // Simulate a click on "Dismiss" button.
  ClickOnAndWait(notification_opt_in_dismiss_button());

  // Clicking on "Dismiss" should hide the view and also disable the ability to
  // show it again.
  EXPECT_FALSE(notification_opt_in_view()->GetVisible());
  EXPECT_TRUE(
      GetNotificationAccessManager()->HasNotificationSetupUiBeenDismissed());
}

TEST_F(PhoneHubTrayTest, HideNotificationOptInViewWhenAccessHasBeenGranted) {
  GetNotificationAccessManager()->SetAccessStatusInternal(
      chromeos::phonehub::NotificationAccessManager::AccessStatus::
          kAccessGranted);

  ClickTrayButton();

  EXPECT_TRUE(notification_opt_in_view());
  EXPECT_FALSE(notification_opt_in_view()->GetVisible());
}

TEST_F(PhoneHubTrayTest, HideNotificationOptInViewWhenAccessIsProhibited) {
  GetNotificationAccessManager()->SetAccessStatusInternal(
      chromeos::phonehub::NotificationAccessManager::AccessStatus::kProhibited);

  ClickTrayButton();

  EXPECT_TRUE(notification_opt_in_view());
  EXPECT_FALSE(notification_opt_in_view()->GetVisible());
}

TEST_F(PhoneHubTrayTest, StartNotificationSetUpFlow) {
  GetNotificationAccessManager()->SetAccessStatusInternal(
      chromeos::phonehub::NotificationAccessManager::AccessStatus::
          kAvailableButNotGranted);

  ClickTrayButton();
  EXPECT_TRUE(notification_opt_in_view());
  EXPECT_TRUE(notification_opt_in_view()->GetVisible());

  // Clicking on the set up button should open the corresponding settings page
  // for the notification set up flow.
  EXPECT_CALL(new_window_delegate(), NewTabWithUrl)
      .WillOnce([](const GURL& url, bool from_user_interaction) {
        EXPECT_EQ(GURL("chrome://os-settings/multidevice/"
                       "features?showNotificationAccessSetupDialog"),
                  url);
        EXPECT_TRUE(from_user_interaction);
      });

  ClickOnAndWait(notification_opt_in_set_up_button());

  // Simulate that notification access has been granted.
  GetNotificationAccessManager()->SetAccessStatusInternal(
      chromeos::phonehub::NotificationAccessManager::AccessStatus::
          kAccessGranted);

  // This view should be dismissed.
  EXPECT_FALSE(notification_opt_in_view()->GetVisible());

  // Simulate that notification access has been revoked by the phone.
  GetNotificationAccessManager()->SetAccessStatusInternal(
      chromeos::phonehub::NotificationAccessManager::AccessStatus::
          kAvailableButNotGranted);

  // This view should show up again.
  EXPECT_TRUE(notification_opt_in_view()->GetVisible());
}

TEST_F(PhoneHubTrayTest, HideTrayItemOnUiStateChange) {
  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());

  GetFeatureStatusProvider()->SetStatus(
      chromeos::phonehub::FeatureStatus::kNotEligibleForFeature);

  EXPECT_FALSE(phone_hub_tray_->is_active());
  EXPECT_FALSE(phone_hub_tray_->GetVisible());
}

TEST_F(PhoneHubTrayTest, TransitionContentView) {
  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());

  EXPECT_TRUE(content_view());
  EXPECT_EQ(PhoneHubViewID::kPhoneConnectedView, content_view()->GetID());

  GetFeatureStatusProvider()->SetStatus(
      chromeos::phonehub::FeatureStatus::kEnabledButDisconnected);

  EXPECT_TRUE(content_view());
  EXPECT_EQ(PhoneHubViewID::kDisconnectedView, content_view()->GetID());
}

TEST_F(PhoneHubTrayTest, StartOnboardingFlow) {
  // Simulate a pending setup state to show the onboarding screen.
  GetFeatureStatusProvider()->SetStatus(
      chromeos::phonehub::FeatureStatus::kEligiblePhoneButNotSetUp);
  GetOnboardingUiTracker()->SetShouldShowOnboardingUi(true);

  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());
  EXPECT_EQ(PhoneHubViewID::kOnboardingView, content_view()->GetID());
  // It should display the onboarding main view.
  EXPECT_TRUE(onboarding_main_view());
  EXPECT_TRUE(onboarding_main_view()->GetVisible());
  EXPECT_EQ(0u, GetOnboardingUiTracker()->handle_get_started_call_count());

  // Simulate a click on the "Get started" button.
  ClickOnAndWait(onboarding_get_started_button());
  // It should invoke the |HandleGetStarted| call.
  EXPECT_EQ(1u, GetOnboardingUiTracker()->handle_get_started_call_count());
}

TEST_F(PhoneHubTrayTest, DismissOnboardingFlowByClickingAckButton) {
  // Simulate a pending setup state to show the onboarding screen.
  GetFeatureStatusProvider()->SetStatus(
      chromeos::phonehub::FeatureStatus::kEligiblePhoneButNotSetUp);
  GetOnboardingUiTracker()->SetShouldShowOnboardingUi(true);

  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());
  EXPECT_EQ(PhoneHubViewID::kOnboardingView, content_view()->GetID());
  // It should display the onboarding main view at first.
  EXPECT_TRUE(onboarding_main_view());

  // Simulate a click on the "Dismiss" button.
  ClickOnAndWait(onboarding_dismiss_button());

  // It should transit to show the dismiss prompt.
  EXPECT_TRUE(onboarding_dismiss_prompt_view());
  EXPECT_TRUE(onboarding_dismiss_prompt_view()->GetVisible());

  // Simulate a click on the "OK, got it" button to ack.
  ClickOnAndWait(onboarding_dismiss_ack_button());

  // Clicking "Ok, got it" button should dismiss the bubble, hide the tray icon,
  // and disable the ability to show onboarding UI again.
  EXPECT_FALSE(phone_hub_tray_->GetBubbleView());
  EXPECT_FALSE(phone_hub_tray_->GetVisible());
  EXPECT_FALSE(GetOnboardingUiTracker()->ShouldShowOnboardingUi());
}

TEST_F(PhoneHubTrayTest, DismissOnboardingFlowByClickingOutside) {
  // Simulate a pending setup state to show the onboarding screen.
  GetFeatureStatusProvider()->SetStatus(
      chromeos::phonehub::FeatureStatus::kEligiblePhoneButNotSetUp);
  GetOnboardingUiTracker()->SetShouldShowOnboardingUi(true);

  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());
  EXPECT_EQ(PhoneHubViewID::kOnboardingView, content_view()->GetID());
  // It should display the onboarding main view at first.
  EXPECT_TRUE(onboarding_main_view());

  // Simulate a click on the "Dismiss" button.
  ClickOnAndWait(onboarding_dismiss_button());

  // It should transit to show the dismiss prompt.
  EXPECT_TRUE(onboarding_dismiss_prompt_view());
  EXPECT_TRUE(onboarding_dismiss_prompt_view()->GetVisible());

  // Simulate a click outside the bubble.
  phone_hub_tray_->ClickedOutsideBubble();

  // Clicking outside should dismiss the bubble, hide the tray icon, and disable
  // the ability to show onboarding UI again.
  EXPECT_FALSE(phone_hub_tray_->GetBubbleView());
  EXPECT_FALSE(phone_hub_tray_->GetVisible());
  EXPECT_FALSE(GetOnboardingUiTracker()->ShouldShowOnboardingUi());
}

TEST_F(PhoneHubTrayTest, ClickButtonsOnDisconnectedView) {
  // Simulates a phone disconnected error state to show the disconnected view.
  GetFeatureStatusProvider()->SetStatus(
      chromeos::phonehub::FeatureStatus::kEnabledButDisconnected);

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
  ClickOnAndWait(disconnected_refresh_button());

  // Clicking "Refresh" button should schedule a connection attempt.
  EXPECT_EQ(2u, GetConnectionScheduler()->num_schedule_connection_now_calls());

  // Clicking "Learn More" button should open the corresponding help center
  // article in a browser tab.
  EXPECT_CALL(new_window_delegate(), NewTabWithUrl)
      .WillOnce([](const GURL& url, bool from_user_interaction) {
        EXPECT_EQ(GURL("https://support.google.com/chromebook?p=phone_hub"),
                  url);
        EXPECT_TRUE(from_user_interaction);
      });

  // Simulates a click on the "Learn more" button.
  ClickOnAndWait(disconnected_learn_more_button());
}

TEST_F(PhoneHubTrayTest, ClickButtonOnBluetoothDisabledView) {
  // Simulate a Bluetooth unavailable state.
  GetFeatureStatusProvider()->SetStatus(
      chromeos::phonehub::FeatureStatus::kUnavailableBluetoothOff);
  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());
  EXPECT_EQ(PhoneHubViewID::kBluetoothDisabledView, content_view()->GetID());

  // Clicking "Learn more" button should open the corresponding help center
  // article in a browser tab.
  EXPECT_CALL(new_window_delegate(), NewTabWithUrl)
      .WillOnce([](const GURL& url, bool from_user_interaction) {
        EXPECT_EQ(GURL("https://support.google.com/chromebook?p=phone_hub"),
                  url);
        EXPECT_TRUE(from_user_interaction);
      });
  // Simulate a click on "Learn more" button.
  ClickOnAndWait(bluetooth_disabled_learn_more_button());
}

}  // namespace ash
