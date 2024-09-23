// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/notification_swipe_control_view.h"

#include <memory>
#include <string>

#include "ash/system/notification_center/message_center_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/lock_screen/fake_lock_screen_controller.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/views/test/button_test_api.h"
#include "url/gurl.h"

namespace {

class MockMessageView : public message_center::MessageView {
 public:
  explicit MockMessageView(const message_center::Notification& notification)
      : message_center::MessageView(notification),
        buttons_view_(
            std::make_unique<message_center::NotificationControlButtonsView>(
                this)) {
    buttons_view_->ShowSettingsButton(
        notification.should_show_settings_button());
    buttons_view_->ShowSnoozeButton(notification.should_show_snooze_button());
  }
  ~MockMessageView() override = default;

  message_center::NotificationControlButtonsView* GetControlButtonsView()
      const override {
    return buttons_view_.get();
  }

  float GetSlideAmount() const override {
    return slide_amount_.value_or(
        message_center::MessageView::GetSlideAmount());
  }

  void ResetSlideAmount() { slide_amount_.reset(); }

  void set_slide_amount(float slide_amount) { slide_amount_ = slide_amount; }

  MOCK_METHOD(void,
              OnSettingsButtonPressed,
              (const ui::Event& event),
              (override));

 private:
  std::unique_ptr<message_center::NotificationControlButtonsView> buttons_view_;
  std::optional<float> slide_amount_;
};

}  // namespace

namespace ash {

class NotificationSwipeControlViewTest : public testing::Test {
 public:
  NotificationSwipeControlViewTest() = default;
  ~NotificationSwipeControlViewTest() override = default;

  void SetUp() override {
    message_center::MessageCenter::Initialize(
        std::make_unique<message_center::FakeLockScreenController>());

    message_center::RichNotificationData rich_data;
    rich_data.settings_button_handler =
        message_center::SettingsButtonHandler::DELEGATE;
    rich_data.should_show_snooze_button = true;
    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, "id", u"title", u"id",
        ui::ImageModel(), std::u16string(), GURL(),
        message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                   "notifier_id"),
        rich_data, nullptr);

    message_view_ = std::make_unique<MockMessageView>(notification);
  }

  void TearDown() override { message_center::MessageCenter::Shutdown(); }

 protected:
  MockMessageView* message_view() { return message_view_.get(); }

 private:
  std::unique_ptr<MockMessageView> message_view_;
};

TEST_F(NotificationSwipeControlViewTest, DeleteOnSettingsButtonPressed) {
  auto swipe_control_view =
      std::make_unique<NotificationSwipeControlView>(message_view());

  EXPECT_CALL(*message_view(), OnSettingsButtonPressed(testing::_))
      .WillOnce(testing::DoDefault())
      .WillOnce(
          testing::InvokeWithoutArgs([&]() { swipe_control_view.reset(); }));

  ui::MouseEvent press(ui::EventType::kMousePressed, gfx::PointF(),
                       gfx::PointF(), ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_NONE);

  // First click will do nothing, expect that to work.
  swipe_control_view->ShowButtons(
      NotificationSwipeControlView::ButtonPosition::LEFT,
      /*show_settings=*/true);
  swipe_control_view->settings_button_->SetHasInkDropActionOnClick(false);

  views::test::ButtonTestApi(swipe_control_view->settings_button_)
      .NotifyClick(press);
  EXPECT_TRUE(swipe_control_view);

  // Second click deletes |swipe_control_view| in the handler.
  swipe_control_view->ShowButtons(
      NotificationSwipeControlView::ButtonPosition::LEFT,
      /*show_settings=*/true);
  swipe_control_view->settings_button_->SetHasInkDropActionOnClick(false);

  views::test::ButtonTestApi(swipe_control_view->settings_button_)
      .NotifyClick(press);
  EXPECT_FALSE(swipe_control_view);
}

TEST_F(NotificationSwipeControlViewTest, SettingsButtonVisibility) {
  auto swipe_control_view =
      std::make_unique<NotificationSwipeControlView>(message_view());
  int available_space =
      kNotificationSwipeControlPadding.left() +
      swipe_control_view->settings_button_->GetPreferredSize().width();

  // The settings button should not be visible if there's not enough space.
  message_view()->set_slide_amount(available_space - 10);
  swipe_control_view->UpdateButtonsVisibility();
  EXPECT_FALSE(swipe_control_view->settings_button_->GetVisible());

  // Should be visible if have enough space.
  message_view()->set_slide_amount(available_space);
  swipe_control_view->UpdateButtonsVisibility();
  EXPECT_TRUE(swipe_control_view->settings_button_->GetVisible());

  message_view()->set_slide_amount(available_space + 10);
  swipe_control_view->UpdateButtonsVisibility();
  EXPECT_TRUE(swipe_control_view->settings_button_->GetVisible());

  message_view()->ResetSlideAmount();
}

}  // namespace ash
