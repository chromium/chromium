// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_tray.h"

#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/system/phonehub/notification_opt_in_view.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/phonehub/fake_notification_access_manager.h"
#include "chromeos/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/event.h"

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
  }

  chromeos::phonehub::FakeFeatureStatusProvider* GetFeatureStatusProvider() {
    return phone_hub_manager_.fake_feature_status_provider();
  }

  chromeos::phonehub::FakeNotificationAccessManager*
  GetNotificationAccessManager() {
    return phone_hub_manager_.fake_notification_access_manager();
  }

  void ClickTrayButton() {
    ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                         ui::GestureEventDetails(ui::ET_GESTURE_TAP));
    phone_hub_tray_->PerformAction(tap);
  }

  // Simulate a mouse click on the given view.
  // Waits for the event to be processed.
  void ClickOnAndWait(const views::View* view) {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
    generator->ClickLeftButton();

    base::RunLoop().RunUntilIdle();
  }

  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }

  NotificationOptInView* notification_opt_in_view() {
    return static_cast<NotificationOptInView*>(
        phone_hub_tray_->GetBubbleView()->GetViewByID(
            PhoneHubViewID::kNotificationOptInView));
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

  ClickTrayButton();
  EXPECT_FALSE(phone_hub_tray_->is_active());
}

TEST_F(PhoneHubTrayTest, ShowNotificationOptInViewWhenAccessNotGranted) {
  GetNotificationAccessManager()->SetHasAccessBeenGrantedInternal(false);

  ClickTrayButton();

  EXPECT_TRUE(notification_opt_in_view());
  EXPECT_TRUE(notification_opt_in_view()->GetVisible());

  // Simulate a click on the dimiss button.
  ClickOnAndWait(notification_opt_in_view()->dismiss_button_for_testing());

  // The view should be dismissed on button clicked.
  EXPECT_FALSE(notification_opt_in_view()->GetVisible());
}

TEST_F(PhoneHubTrayTest, HideNotificationOptInViewWhenAccessHasBeenGranted) {
  GetNotificationAccessManager()->SetHasAccessBeenGrantedInternal(true);

  ClickTrayButton();

  EXPECT_FALSE(notification_opt_in_view());
}

TEST_F(PhoneHubTrayTest, StartNotificationSetUpFlow) {
  GetNotificationAccessManager()->SetHasAccessBeenGrantedInternal(false);

  ClickTrayButton();
  EXPECT_TRUE(notification_opt_in_view());
  EXPECT_TRUE(notification_opt_in_view()->GetVisible());

  // Clicking on the set up button should open the corresponding settings page
  // for the notification set up flow.
  EXPECT_CALL(new_window_delegate(), NewTabWithUrl)
      .WillOnce([](const GURL& url, bool from_user_interaction) {
        EXPECT_EQ(GURL("chrome://os-settings/multidevice/features"), url);
        EXPECT_TRUE(from_user_interaction);
      });

  ClickOnAndWait(notification_opt_in_view()->set_up_button_for_testing());
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

  auto* content_view = phone_hub_tray_->content_view_for_testing();
  EXPECT_TRUE(content_view);
  // TODO(tengs) Test the actual view id.
  EXPECT_EQ(0, content_view->GetID());

  GetFeatureStatusProvider()->SetStatus(
      chromeos::phonehub::FeatureStatus::kEnabledButDisconnected);

  content_view = phone_hub_tray_->content_view_for_testing();
  EXPECT_TRUE(content_view);
  // TODO(tengs) Test the actual view id.
  EXPECT_EQ(0, content_view->GetID());
}

}  // namespace ash
