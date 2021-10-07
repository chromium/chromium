// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/ash_notification_view.h"

#include "ash/system/message_center/message_center_style.h"
#include "ash/test/ash_test_base.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"

using message_center::Notification;
using message_center::NotificationHeaderView;
using message_center::NotificationView;

namespace ash {

namespace {

constexpr char kDefaultNotificationId[] = "ash notification id";

const gfx::Image CreateTestImage(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorGREEN);
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

// Create a test notification that is used in the view.
std::unique_ptr<Notification> CreateTestNotification(bool has_image = false) {
  message_center::RichNotificationData data;
  data.settings_button_handler = message_center::SettingsButtonHandler::INLINE;

  std::unique_ptr<Notification> notification = std::make_unique<Notification>(
      message_center::NOTIFICATION_TYPE_BASE_FORMAT,
      std::string(kDefaultNotificationId), u"title", u"message",
      CreateTestImage(80, 80), u"display source", GURL(),
      message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                 "extension_id"),
      data, nullptr /* delegate */);
  notification->set_small_image(CreateTestImage(16, 16));

  if (has_image)
    notification->set_image(CreateTestImage(320, 240));

  return notification;
}

}  // namespace

class AshNotificationViewTest : public AshTestBase, public views::ViewObserver {
 public:
  AshNotificationViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  AshNotificationViewTest(const AshNotificationViewTest&) = delete;
  AshNotificationViewTest& operator=(const AshNotificationViewTest&) = delete;
  ~AshNotificationViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    auto notification = CreateTestNotification();
    notification_view_ = std::make_unique<AshNotificationView>(
        *notification, /*is_popup=*/false);
  }

  void TearDown() override {
    notification_view_.reset();
    AshTestBase::TearDown();
  }

  void UpdateTimestamp(base::Time timestamp) {
    notification_view()->title_row_->UpdateTimestamp(timestamp);
  }

  void AdvanceClock(base::TimeDelta time_delta) {
    // Note that AdvanceClock() is used here instead of FastForwardBy() to
    // prevent long run time during an ash test session.
    task_environment()->AdvanceClock(time_delta);
    task_environment()->RunUntilIdle();
  }

 protected:
  AshNotificationView* GetFirstGroupedChildNotificationView() {
    if (!notification_view_->grouped_notifications_container_->children()
             .size()) {
      return nullptr;
    }

    return static_cast<AshNotificationView*>(
        notification_view_->grouped_notifications_container_->children()
            .front());
  }
  views::View* GetCollapsedSummaryViewForNotificationView(
      AshNotificationView* view) {
    return view->collapsed_summary_view_;
  }
  views::View* GetMainViewForNotificationView(AshNotificationView* view) {
    return view->main_view_;
  }
  AshNotificationView* notification_view() { return notification_view_.get(); }
  NotificationHeaderView* header_row() {
    return notification_view_->header_row();
  }
  views::View* left_content() { return notification_view_->left_content(); }
  views::View* title_row() { return notification_view_->title_row_; }
  views::Label* title_view() {
    return notification_view_->title_row_->title_view_;
  }
  views::View* title_row_divider() {
    return notification_view_->title_row_->title_row_divider_;
  }
  views::Label* timestamp_in_collapsed_view() {
    return notification_view_->title_row_->timestamp_in_collapsed_view_;
  }
  views::Label* message_view() { return notification_view_->message_view(); }
  AshNotificationView::ExpandButton* expand_button() {
    return notification_view_->expand_button_;
  }

 private:
  std::unique_ptr<AshNotificationView> notification_view_;
};

TEST_F(AshNotificationViewTest, UpdateViewsOrderingTest) {
  EXPECT_NE(nullptr, title_row());
  EXPECT_NE(nullptr, message_view());
  EXPECT_EQ(0, left_content()->GetIndexOf(title_row()));
  EXPECT_EQ(1, left_content()->GetIndexOf(message_view()));

  std::unique_ptr<Notification> notification = CreateTestNotification();
  notification->set_title(std::u16string());

  notification_view()->UpdateWithNotification(*notification);

  EXPECT_EQ(nullptr, title_row());
  EXPECT_NE(nullptr, message_view());
  EXPECT_EQ(0, left_content()->GetIndexOf(message_view()));

  notification->set_title(u"title");

  notification_view()->UpdateWithNotification(*notification);

  EXPECT_NE(nullptr, title_row());
  EXPECT_NE(nullptr, message_view());
  EXPECT_EQ(0, left_content()->GetIndexOf(title_row()));
  EXPECT_EQ(1, left_content()->GetIndexOf(message_view()));
}

TEST_F(AshNotificationViewTest, CreateOrUpdateTitle) {
  EXPECT_NE(nullptr, title_row());
  EXPECT_NE(nullptr, title_view());
  EXPECT_NE(nullptr, title_row_divider());
  EXPECT_NE(nullptr, timestamp_in_collapsed_view());

  std::unique_ptr<Notification> notification = CreateTestNotification();

  // Every view should be null when title is empty.
  notification->set_title(std::u16string());
  notification_view()->UpdateWithNotification(*notification);

  EXPECT_EQ(nullptr, title_row());

  const std::u16string& expected_text = u"title";
  notification->set_title(expected_text);

  notification_view()->UpdateWithNotification(*notification);

  EXPECT_NE(nullptr, title_row());
  EXPECT_EQ(expected_text, title_view()->GetText());
}

TEST_F(AshNotificationViewTest, UpdatesTimestampOverTime) {
  EXPECT_FALSE(timestamp_in_collapsed_view()->GetVisible());

  auto notification = CreateTestNotification(/*has_image=*/true);
  notification_view()->UpdateWithNotification(*notification);
  notification_view()->SetExpanded(false);

  EXPECT_TRUE(timestamp_in_collapsed_view()->GetVisible());

  UpdateTimestamp(base::Time::Now() + base::Hours(3) + base::Minutes(30));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST_FUTURE, 3),
            timestamp_in_collapsed_view()->GetText());

  AdvanceClock(base::Hours(3));

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST_FUTURE, 30),
            timestamp_in_collapsed_view()->GetText());

  AdvanceClock(base::Minutes(30));

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST),
      timestamp_in_collapsed_view()->GetText());

  AdvanceClock(base::Days(2));

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST, 2),
            timestamp_in_collapsed_view()->GetText());
}

TEST_F(AshNotificationViewTest, ExpandCollapseBehavior) {
  auto notification = CreateTestNotification(/*has_image=*/true);
  notification_view()->UpdateWithNotification(*notification);

  // Expected behavior in collapsed mode.
  notification_view()->SetExpanded(false);
  EXPECT_FALSE(header_row()->GetVisible());
  EXPECT_TRUE(timestamp_in_collapsed_view()->GetVisible());
  EXPECT_TRUE(title_row_divider()->GetVisible());

  // Expected behavior in expanded mode.
  notification_view()->SetExpanded(true);
  EXPECT_TRUE(header_row()->GetVisible());
  EXPECT_FALSE(timestamp_in_collapsed_view()->GetVisible());
  EXPECT_FALSE(title_row_divider()->GetVisible());
}

TEST_F(AshNotificationViewTest, GroupedNotificationStartsCollapsed) {
  auto notification = CreateTestNotification();
  notification->SetGroupParent();
  notification_view()->UpdateWithNotification(*notification.get());
  for (int i = 0;
       i < message_center_style::kMaxGroupedNotificationsInCollapsedState;
       i++) {
    auto group_child = CreateTestNotification();
    group_child->SetGroupChild();
    notification_view()->AddGroupNotification(*group_child.get(),
                                              /*newest_first=*/false);
  }
  // Grouped notification should start collapsed.
  EXPECT_FALSE(notification_view()->IsExpanded());
  EXPECT_TRUE(header_row()->GetVisible());
  EXPECT_TRUE(expand_button()->label_for_test()->GetVisible());
}

TEST_F(AshNotificationViewTest, GroupedNotificationCounterVisibility) {
  auto notification = CreateTestNotification();
  notification->SetGroupParent();
  notification_view()->UpdateWithNotification(*notification.get());
  for (int i = 0;
       i < message_center_style::kMaxGroupedNotificationsInCollapsedState + 1;
       i++) {
    auto group_child = CreateTestNotification();
    group_child->SetGroupChild();
    notification_view()->AddGroupNotification(*group_child.get(),
                                              /*newest_first=*/false);
  }

  EXPECT_TRUE(expand_button()->label_for_test()->GetVisible());

  auto* child_view = GetFirstGroupedChildNotificationView();
  EXPECT_TRUE(
      GetCollapsedSummaryViewForNotificationView(child_view)->GetVisible());
  EXPECT_FALSE(GetMainViewForNotificationView(child_view)->GetVisible());
}

TEST_F(AshNotificationViewTest, GroupedNotificationExpandState) {
  auto notification = CreateTestNotification();
  notification->SetGroupParent();
  notification_view()->UpdateWithNotification(*notification.get());
  for (int i = 0;
       i < message_center_style::kMaxGroupedNotificationsInCollapsedState + 1;
       i++) {
    auto group_child = CreateTestNotification();
    group_child->SetGroupChild();
    notification_view()->AddGroupNotification(*group_child.get(),
                                              /*newest_first=*/false);
  }

  auto* child_view = GetFirstGroupedChildNotificationView();
  EXPECT_TRUE(
      GetCollapsedSummaryViewForNotificationView(child_view)->GetVisible());
  EXPECT_FALSE(GetMainViewForNotificationView(child_view)->GetVisible());

  // Expanding the parent notification should make the counter invisible and
  // the child notifications should now have the main view visible instead of
  // the summary.
  notification_view()->SetExpanded(true);
  EXPECT_FALSE(expand_button()->label_for_test()->GetVisible());
  EXPECT_FALSE(
      GetCollapsedSummaryViewForNotificationView(child_view)->GetVisible());
  EXPECT_TRUE(GetMainViewForNotificationView(child_view)->GetVisible());
}

TEST_F(AshNotificationViewTest, ExpandButtonVisibility) {
  // Expand button should not be shown when it is not expandable.
  EXPECT_FALSE(expand_button()->GetVisible());

  // Expand button should be shown when it is expandable (i.e. there is an
  // image).
  auto notification = CreateTestNotification(/*has_image=*/true);
  notification_view()->UpdateWithNotification(*notification);
  EXPECT_TRUE(expand_button()->GetVisible());
}

TEST_F(AshNotificationViewTest, LeftContentNotVisibleInGroupedNotifications) {
  auto notification = CreateTestNotification();

  EXPECT_TRUE(left_content()->GetVisible());

  auto group_child = CreateTestNotification();
  notification_view()->AddGroupNotification(*group_child.get(), false);
  EXPECT_FALSE(left_content()->GetVisible());

  notification_view()->RemoveGroupNotification(group_child->id());
  EXPECT_TRUE(left_content()->GetVisible());
}
}  // namespace ash
