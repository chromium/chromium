// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/conversation_notification_view.h"

#include "ash/test/ash_test_base.h"
#include "ui/events/test/test_event.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/widget/widget.h"

namespace ash {

class ConversationNotificationViewTest : public AshTestBase {
 public:
  ConversationNotificationViewTest() = default;
  ConversationNotificationViewTest(const ConversationNotificationViewTest&) =
      delete;
  ConversationNotificationViewTest& operator=(
      const ConversationNotificationViewTest&) = delete;
  ~ConversationNotificationViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    notification_ = CreateConversationNotification();

    // `widget_` owns `notification_view_`.
    widget_ = CreateTestWidget();
    notification_view_ = widget_->GetContentsView()->AddChildView(
        std::make_unique<ConversationNotificationView>(*notification_.get()));
  }

  void TearDown() override {
    notification_view_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<message_center::Notification>
  CreateConversationNotification() {
    std::vector<message_center::NotificationItem> items = {
        {u"title", u"message"}, {u"title", u"message"}};
    message_center::RichNotificationData rich_data;
    rich_data.items = items;
    rich_data.settings_button_handler =
        message_center::SettingsButtonHandler::INLINE;

    return std::make_unique<message_center::Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, "id", u"title",
        u"test message", ui::ImageModel(), /*display_source=*/std::u16string(),
        GURL(), message_center::NotifierId(), rich_data, /*delegate=*/nullptr);
  }

  ConversationNotificationView* notification_view() {
    return notification_view_.get();
  }

  views::View* collapsed_preview_container() {
    return notification_view_->collapsed_preview_container_;
  }

  views::View* conversation_container() {
    return notification_view_->conversations_container_;
  }

  views::View* inline_settings_view() {
    return notification_view_->inline_settings_view_;
  }

  views::View* right_controls_container() {
    return notification_view_->right_controls_container_;
  }

 private:
  raw_ptr<ConversationNotificationView> notification_view_;
  std::unique_ptr<Notification> notification_;
  std::unique_ptr<views::Widget> widget_;
};

// Make sure expanding and collapsing the notification updates the appropriate
// view visibilities.
TEST_F(ConversationNotificationViewTest, ExpandCollapse) {
  ASSERT_TRUE(notification_view()->IsExpanded());

  auto* conversations_container = notification_view()->GetViewByID(
      ConversationNotificationView::ViewId::kConversationContainer);
  auto* collapsed_preview_container = notification_view()->GetViewByID(
      ConversationNotificationView::ViewId::kCollapsedPreviewContainer);

  notification_view()->ToggleExpand();
  EXPECT_FALSE(notification_view()->IsExpanded());
  EXPECT_TRUE(collapsed_preview_container->GetVisible());
  EXPECT_FALSE(conversations_container->GetVisible());

  notification_view()->ToggleExpand();
  EXPECT_TRUE(notification_view()->IsExpanded());
  EXPECT_FALSE(collapsed_preview_container->GetVisible());
  EXPECT_TRUE(conversations_container->GetVisible());
}

TEST_F(ConversationNotificationViewTest, ToggleInlineSettings) {
  ASSERT_FALSE(inline_settings_view()->GetVisible());
  ASSERT_TRUE(notification_view()->IsExpanded());

  // Test toggling inline settings when the notification is expanded.
  notification_view()->ToggleInlineSettings(ui::test::TestEvent());
  EXPECT_TRUE(inline_settings_view()->GetVisible());
  EXPECT_FALSE(conversation_container()->GetVisible());
  EXPECT_FALSE(collapsed_preview_container()->GetVisible());
  EXPECT_FALSE(right_controls_container()->GetVisible());

  notification_view()->ToggleInlineSettings(ui::test::TestEvent());
  EXPECT_FALSE(inline_settings_view()->GetVisible());
  EXPECT_TRUE(conversation_container()->GetVisible());
  EXPECT_FALSE(collapsed_preview_container()->GetVisible());
  EXPECT_TRUE(right_controls_container()->GetVisible());

  // Test toggling inline settings when the notification is collapsed.
  notification_view()->ToggleExpand();
  ASSERT_FALSE(notification_view()->IsExpanded());

  notification_view()->ToggleInlineSettings(ui::test::TestEvent());
  EXPECT_TRUE(inline_settings_view()->GetVisible());
  EXPECT_FALSE(conversation_container()->GetVisible());
  EXPECT_FALSE(collapsed_preview_container()->GetVisible());
  EXPECT_FALSE(right_controls_container()->GetVisible());

  notification_view()->ToggleInlineSettings(ui::test::TestEvent());
  EXPECT_FALSE(inline_settings_view()->GetVisible());
  EXPECT_FALSE(conversation_container()->GetVisible());
  EXPECT_TRUE(collapsed_preview_container()->GetVisible());
  EXPECT_TRUE(right_controls_container()->GetVisible());
}

}  // namespace ash
