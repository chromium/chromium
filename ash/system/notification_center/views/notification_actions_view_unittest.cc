// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/notification_actions_view.h"

#include "ash/style/icon_button.h"
#include "ash/style/system_textfield.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/test/ash_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/event.h"
#include "ui/events/test/test_event.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

// A test base class that helps to verify notification view features.
class NotificationActionsViewTest : public AshTestBase {
 public:
  NotificationActionsViewTest() = default;
  NotificationActionsViewTest(const NotificationActionsViewTest&) = delete;
  NotificationActionsViewTest& operator=(const NotificationActionsViewTest&) =
      delete;
  ~NotificationActionsViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // `widget_` owns `actions_view_`.
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

    actions_view_ = widget_->GetContentsView()->AddChildView(
        std::make_unique<NotificationActionsView>());
  }

  void TearDown() override {
    actions_view_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<message_center::Notification>
  CreateNotificationWithInlineReply() {
    auto notification = std::make_unique<message_center::Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, "id", u"title",
        u"test message", ui::ImageModel(), /*display_source=*/std::u16string(),
        GURL(), message_center::NotifierId(),
        message_center::RichNotificationData(), /*delegate=*/nullptr);

    auto reply_button = message_center::ButtonInfo(u"Reply");
    reply_button.placeholder = std::make_optional(u"Placeholder");
    std::vector<message_center::ButtonInfo> buttons;
    buttons.push_back(reply_button);
    notification->set_buttons(buttons);
    return notification;
  }

  void TypeTestResponse() {
    ui::KeyboardCode keycodes[] = {ui::VKEY_T, ui::VKEY_E, ui::VKEY_S,
                                   ui::VKEY_T};
    for (ui::KeyboardCode keycode : keycodes) {
      PressAndReleaseKey(keycode, ui::EF_NONE);
    }
  }

  void ClickActionButton(unsigned int index) {
    CHECK_LT(index, buttons_container()->children().size());
    views::test::ButtonTestApi(
        static_cast<views::Button*>(buttons_container()->children()[index]))
        .NotifyClick(ui::test::TestEvent());
  }

  void set_send_reply_callback(base::RepeatingCallback<void()> callback) {
    actions_view_->send_reply_callback_ = callback;
  }

  testing::MockFunction<void()> CreateMockCallback() {
    return testing::MockFunction<void()>();
  }

  views::View* buttons_container() { return actions_view_->buttons_container_; }
  views::View* inline_reply_container() {
    return actions_view_->inline_reply_container_;
  }
  SystemTextfield* textfield() { return actions_view_->textfield_; }
  IconButton* send_button() { return actions_view_->send_button_; }
  NotificationActionsView* actions_view() { return actions_view_.get(); }

 private:
  raw_ptr<NotificationActionsView> actions_view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(NotificationActionsViewTest, TextfieldFocusedAfterButtonPress) {
  auto notification = CreateNotificationWithInlineReply();

  actions_view()->UpdateWithNotification(*notification);
  EXPECT_FALSE(inline_reply_container()->GetVisible());

  // The inline reply container should be visible and textfield focused when the
  // reply button is pressed.
  ClickActionButton(0);
  EXPECT_TRUE(inline_reply_container()->GetVisible());
  EXPECT_TRUE(textfield()->HasFocus());
}

TEST_F(NotificationActionsViewTest, SendButtonStateUpdated) {
  auto notification = CreateNotificationWithInlineReply();

  actions_view()->UpdateWithNotification(*notification);

  ClickActionButton(0);
  EXPECT_TRUE(textfield()->GetText().empty());
  EXPECT_FALSE(send_button()->GetEnabled());

  // The `send_button_` should be enabled only after some text is typed.
  TypeTestResponse();
  EXPECT_TRUE(send_button()->GetEnabled());
}

TEST_F(NotificationActionsViewTest, TestInlineReplySent) {
  auto notification = CreateNotificationWithInlineReply();
  actions_view()->UpdateWithNotification(*notification);
  ClickActionButton(0);
  TypeTestResponse();

  // Setup a mock callback to run if a response is sent.
  auto mock_send_reply_callback = CreateMockCallback();
  set_send_reply_callback(
      base::BindRepeating(&testing::MockFunction<void()>::Call,
                          base::Unretained(&mock_send_reply_callback)));
  // The callback should be called when return key is pressed.
  EXPECT_CALL(mock_send_reply_callback, Call());
  // Submit by typing RETURN key.
  PressAndReleaseKey(ui::VKEY_RETURN, ui::EF_NONE);

  // The callback should be called when the send button is pressed.
  EXPECT_CALL(mock_send_reply_callback, Call());
  // Submit by typing RETURN key.
  views::test::ButtonTestApi(send_button()).NotifyClick(ui::test::TestEvent());
}

}  // namespace ash
