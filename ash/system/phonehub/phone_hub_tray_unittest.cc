// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_tray.h"

#include "ash/components/phonehub/fake_connection_scheduler.h"
#include "ash/components/phonehub/fake_notification_access_manager.h"
#include "ash/components/phonehub/fake_phone_hub_manager.h"
#include "ash/components/phonehub/phone_model_test_util.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/shell.h"
#include "ash/system/phonehub/notification_opt_in_view.h"
#include "ash/system/phonehub/phone_hub_ui_controller.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/button.h"

namespace ash {

namespace {

constexpr base::TimeDelta kConnectingViewGracePeriod = base::Seconds(40);

// A mock implementation of |NewWindowDelegate| for use in tests.
class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, bool from_user_interaction),
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
    feature_list_.InitAndEnableFeature(chromeos::features::kPhoneHub);
    auto delegate = std::make_unique<MockNewWindowDelegate>();
    new_window_delegate_ = delegate.get();
    delegate_provider_ =
        std::make_unique<TestNewWindowDelegateProvider>(std::move(delegate));
    AshTestBase::SetUp();

    phone_hub_tray_ =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->phone_hub_tray();

    GetFeatureStatusProvider()->SetStatus(
        phonehub::FeatureStatus::kEnabledAndConnected);
    phone_hub_tray_->SetPhoneHubManager(&phone_hub_manager_);

    phone_hub_manager_.mutable_phone_model()->SetPhoneStatusModel(
        phonehub::CreateFakePhoneStatusModel());
  }

  phonehub::FakeFeatureStatusProvider* GetFeatureStatusProvider() {
    return phone_hub_manager_.fake_feature_status_provider();
  }

  phonehub::FakeNotificationAccessManager* GetNotificationAccessManager() {
    return phone_hub_manager_.fake_notification_access_manager();
  }

  phonehub::FakeConnectionScheduler* GetConnectionScheduler() {
    return phone_hub_manager_.fake_connection_scheduler();
  }

  phonehub::FakeOnboardingUiTracker* GetOnboardingUiTracker() {
    return phone_hub_manager_.fake_onboarding_ui_tracker();
  }

  void PressReturnKeyOnTrayButton() {
    const ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_RETURN,
                                 ui::EF_NONE);
    phone_hub_tray_->PerformAction(key_event);
  }

  void ClickTrayButton() {
    SimulateMouseClickAt(GetEventGenerator(), phone_hub_tray_);
  }

  // When first connecting, the connecting view is shown for 30 seconds when
  // disconnected, so in order to show the disconnecting view, we need to fast
  // forward time.
  void FastForwardByConnectingViewGracePeriod() {
    task_environment()->FastForwardBy(kConnectingViewGracePeriod);
  }

  MockNewWindowDelegate& new_window_delegate() { return *new_window_delegate_; }

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
        PhoneHubViewID::kSubFeatureOptInConfirmButton));
  }

  views::Button* notification_opt_in_dismiss_button() {
    return static_cast<views::Button*>(bubble_view()->GetViewByID(
        PhoneHubViewID::kSubFeatureOptInDismissButton));
  }

 protected:
  PhoneHubTray* phone_hub_tray_ = nullptr;
  phonehub::FakePhoneHubManager phone_hub_manager_;
  base::test::ScopedFeatureList feature_list_;
  MockNewWindowDelegate* new_window_delegate_;
  std::unique_ptr<TestNewWindowDelegateProvider> delegate_provider_;
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
  PressReturnKeyOnTrayButton();

  // Generate a tab key press.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EventFlags::EF_NONE);

  // The bubble widget should get focus when it's opened by keyboard and the tab
  // key is pressed.
  EXPECT_TRUE(phone_hub_tray_->is_active());
  EXPECT_TRUE(phone_hub_tray_->GetBubbleView()->GetWidget()->IsActive());
}

TEST_F(PhoneHubTrayTest, ShowNotificationOptInViewWhenAccessNotGranted) {
  GetNotificationAccessManager()->SetAccessStatusInternal(
      phonehub::NotificationAccessManager::AccessStatus::
          kAvailableButNotGranted);

  ClickTrayButton();

  EXPECT_TRUE(notification_opt_in_view());
  EXPECT_TRUE(notification_opt_in_view()->GetVisible());

  // Simulate a click on "Dismiss" button.
  SimulateMouseClickAt(GetEventGenerator(),
                       notification_opt_in_dismiss_button());

  // Clicking on "Dismiss" should hide the view and also disable the ability to
  // show it again.
  EXPECT_FALSE(notification_opt_in_view()->GetVisible());
  EXPECT_TRUE(
      GetNotificationAccessManager()->HasNotificationSetupUiBeenDismissed());
}

TEST_F(PhoneHubTrayTest, HideNotificationOptInViewWhenAccessHasBeenGranted) {
  GetNotificationAccessManager()->SetAccessStatusInternal(
      phonehub::NotificationAccessManager::AccessStatus::kAccessGranted);

  ClickTrayButton();

  EXPECT_TRUE(notification_opt_in_view());
  EXPECT_FALSE(notification_opt_in_view()->GetVisible());
}

TEST_F(PhoneHubTrayTest, HideNotificationOptInViewWhenAccessIsProhibited) {
  GetNotificationAccessManager()->SetAccessStatusInternal(
      phonehub::NotificationAccessManager::AccessStatus::kProhibited);

  ClickTrayButton();

  EXPECT_TRUE(notification_opt_in_view());
  EXPECT_FALSE(notification_opt_in_view()->GetVisible());
}

TEST_F(PhoneHubTrayTest, StartNotificationSetUpFlow) {
  GetNotificationAccessManager()->SetAccessStatusInternal(
      phonehub::NotificationAccessManager::AccessStatus::
          kAvailableButNotGranted);

  ClickTrayButton();
  EXPECT_TRUE(notification_opt_in_view());
  EXPECT_TRUE(notification_opt_in_view()->GetVisible());

  // Clicking on the set up button should open the corresponding settings page
  // for the notification set up flow.
  EXPECT_CALL(new_window_delegate(), OpenUrl)
      .WillOnce([](const GURL& url, bool from_user_interaction) {
        EXPECT_EQ(GURL("chrome://os-settings/multidevice/"
                       "features?showNotificationAccessSetupDialog"),
                  url);
        EXPECT_TRUE(from_user_interaction);
      });

  SimulateMouseClickAt(GetEventGenerator(),
                       notification_opt_in_set_up_button());

  // Simulate that notification access has been granted.
  GetNotificationAccessManager()->SetAccessStatusInternal(
      phonehub::NotificationAccessManager::AccessStatus::kAccessGranted);

  // This view should be dismissed.
  EXPECT_FALSE(notification_opt_in_view()->GetVisible());

  // Simulate that notification access has been revoked by the phone.
  GetNotificationAccessManager()->SetAccessStatusInternal(
      phonehub::NotificationAccessManager::AccessStatus::
          kAvailableButNotGranted);

  // This view should show up again.
  EXPECT_TRUE(notification_opt_in_view()->GetVisible());
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
  SimulateMouseClickAt(GetEventGenerator(), onboarding_get_started_button());
  // It should invoke the |HandleGetStarted| call.
  EXPECT_EQ(1u, GetOnboardingUiTracker()->handle_get_started_call_count());
}

TEST_F(PhoneHubTrayTest, DismissOnboardingFlowByClickingAckButton) {
  // Simulate a pending setup state to show the onboarding screen.
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kEligiblePhoneButNotSetUp);
  GetOnboardingUiTracker()->SetShouldShowOnboardingUi(true);

  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());
  EXPECT_EQ(PhoneHubViewID::kOnboardingView, content_view()->GetID());
  // It should display the onboarding main view at first.
  EXPECT_TRUE(onboarding_main_view());

  // Simulate a click on the "Dismiss" button.
  SimulateMouseClickAt(GetEventGenerator(), onboarding_dismiss_button());

  // It should transit to show the dismiss prompt.
  EXPECT_TRUE(onboarding_dismiss_prompt_view());
  EXPECT_TRUE(onboarding_dismiss_prompt_view()->GetVisible());

  // Simulate a click on the "OK, got it" button to ack.
  SimulateMouseClickAt(GetEventGenerator(), onboarding_dismiss_ack_button());

  // Clicking "Ok, got it" button should dismiss the bubble, hide the tray icon,
  // and disable the ability to show onboarding UI again.
  EXPECT_FALSE(phone_hub_tray_->GetBubbleView());
  EXPECT_FALSE(phone_hub_tray_->GetVisible());
  EXPECT_FALSE(GetOnboardingUiTracker()->ShouldShowOnboardingUi());
}

TEST_F(PhoneHubTrayTest, DismissOnboardingFlowByClickingOutside) {
  // Simulate a pending setup state to show the onboarding screen.
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kEligiblePhoneButNotSetUp);
  GetOnboardingUiTracker()->SetShouldShowOnboardingUi(true);

  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());
  EXPECT_EQ(PhoneHubViewID::kOnboardingView, content_view()->GetID());
  // It should display the onboarding main view at first.
  EXPECT_TRUE(onboarding_main_view());

  // Simulate a click on the "Dismiss" button.
  SimulateMouseClickAt(GetEventGenerator(), onboarding_dismiss_button());

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
  SimulateMouseClickAt(GetEventGenerator(), disconnected_refresh_button());

  // Clicking "Refresh" button should schedule a connection attempt.
  EXPECT_EQ(2u, GetConnectionScheduler()->num_schedule_connection_now_calls());

  // Clicking "Learn More" button should open the corresponding help center
  // article in a browser tab.
  EXPECT_CALL(new_window_delegate(), OpenUrl)
      .WillOnce([](const GURL& url, bool from_user_interaction) {
        EXPECT_EQ(GURL("https://support.google.com/chromebook?p=phone_hub"),
                  url);
        EXPECT_TRUE(from_user_interaction);
      });

  // Simulates a click on the "Learn more" button.
  SimulateMouseClickAt(GetEventGenerator(), disconnected_learn_more_button());
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
  EXPECT_CALL(new_window_delegate(), OpenUrl)
      .WillOnce([](const GURL& url, bool from_user_interaction) {
        EXPECT_EQ(GURL("https://support.google.com/chromebook?p=phone_hub"),
                  url);
        EXPECT_TRUE(from_user_interaction);
      });
  // Simulate a click on "Learn more" button.
  SimulateMouseClickAt(GetEventGenerator(),
                       bluetooth_disabled_learn_more_button());
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
  task_environment()->FastForwardBy(base::Seconds(3));
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
    task_environment()->FastForwardBy(base::Seconds(1));
    EXPECT_FALSE(phone_hub_tray_->GetVisible());
    GetFeatureStatusProvider()->SetStatus(
        phonehub::FeatureStatus::kEnabledAndConnected);
  }
  EXPECT_FALSE(phone_hub_tray_->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(phone_hub_tray_->GetVisible());

  // Animation is enabled after 5 seconds. We already fast forwarded 3 second in
  // the above loop. So here we are forwarding 2 more seconds.
  task_environment()->FastForwardBy(base::Seconds(2));
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kNotEligibleForFeature);
  GetFeatureStatusProvider()->SetStatus(
      phonehub::FeatureStatus::kEnabledAndConnected);
  EXPECT_TRUE(phone_hub_tray_->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(phone_hub_tray_->GetVisible());
}

}  // namespace ash
