// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/notification_menu_view.h"

#include "ash/app_menu/notification_item_view.h"
#include "ash/app_menu/notification_menu_view_test_api.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {

namespace {

// The app id used in tests.
constexpr char kTestAppId[] = "test-app-id";

class MockNotificationMenuController : public views::SlideOutControllerDelegate,
                                       public NotificationMenuView::Delegate {
 public:
  MockNotificationMenuController() = default;

  MockNotificationMenuController(const MockNotificationMenuController&) =
      delete;
  MockNotificationMenuController& operator=(
      const MockNotificationMenuController&) = delete;

  ~MockNotificationMenuController() override = default;

  void ActivateNotificationAndClose(
      const std::string& notification_id) override {
    activation_count_++;
  }

  void OnOverflowAddedOrRemoved() override {
    overflow_added_or_removed_count_++;
  }

  ui::Layer* GetSlideOutLayer() override {
    return notification_menu_view_->GetSlideOutLayer();
  }

  void OnSlideChanged(bool in_progress) override {}

  void OnSlideOut() override { slide_out_count_++; }

  void set_notification_menu_view(
      NotificationMenuView* notification_menu_view) {
    notification_menu_view_ = notification_menu_view;
  }

  int slide_out_count_ = 0;
  int activation_count_ = 0;
  int overflow_added_or_removed_count_ = 0;

  // Owned by NotificationMenuViewTest.
  raw_ptr<NotificationMenuView, DanglingUntriaged> notification_menu_view_ =
      nullptr;
};

}  // namespace

class NotificationMenuViewTest : public views::ViewsTestBase {
 public:
  NotificationMenuViewTest() {}

  NotificationMenuViewTest(const NotificationMenuViewTest&) = delete;
  NotificationMenuViewTest& operator=(const NotificationMenuViewTest&) = delete;

  ~NotificationMenuViewTest() override = default;

  // views::ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    zero_duration_scope_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

    mock_notification_menu_controller_ =
        std::make_unique<MockNotificationMenuController>();

    auto notification_menu_view = std::make_unique<NotificationMenuView>(
        mock_notification_menu_controller_.get(),
        mock_notification_menu_controller_.get(), kTestAppId);

    // Set the NotificationMenuView so |mock_notification_menu_controller_|
    // can get the slide out layer. In production NotificationMenuController is
    // the NotificationItemViewDelegate, and it gets a reference to
    // NotificationMenuView when it is created.
    mock_notification_menu_controller_->set_notification_menu_view(
        notification_menu_view.get());

    test_api_ = std::make_unique<NotificationMenuViewTestAPI>(
        notification_menu_view.get());

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams init_params(
        CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                     views::Widget::InitParams::TYPE_POPUP));
    init_params.activatable = views::Widget::InitParams::Activatable::kYes;
    widget_->Init(std::move(init_params));
    notification_menu_view_ =
        widget_->SetContentsView(std::move(notification_menu_view));
    widget_->SetSize(notification_menu_view_->GetPreferredSize());
    widget_->Show();
    widget_->Activate();
  }

  void TearDown() override {
    widget_->Close();
    views::ViewsTestBase::TearDown();
  }

  message_center::Notification AddNotification(
      const std::string& notification_id,
      const std::u16string& title,
      const std::u16string& message) {
    const message_center::NotifierId notifier_id(
        message_center::NotifierType::APPLICATION, kTestAppId);
    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title,
        message, ui::ImageModel(), u"www.test.org", GURL(), notifier_id,
        message_center::RichNotificationData(), nullptr /* delegate */);
    notification_menu_view_->AddNotificationItemView(notification);
    views::test::RunScheduledLayout(notification_menu_view_);
    return notification;
  }

  message_center::Notification UpdateNotification(
      const std::string& notification_id,
      const std::u16string& title,
      const std::u16string& message) {
    const message_center::NotifierId notifier_id(
        message_center::NotifierType::APPLICATION, kTestAppId);
    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title,
        message, ui::ImageModel(), u"www.test.org", GURL(), notifier_id,
        message_center::RichNotificationData(), nullptr /* delegate */);
    notification_menu_view_->UpdateNotificationItemView(notification);
    return notification;
  }

  void CheckDisplayedNotification(
      const message_center::Notification& notification) {
    // Check whether the notification and view contents match.
    const auto* item =
        notification_menu_view_->GetDisplayedNotificationItemView();
    ASSERT_TRUE(item);
    EXPECT_EQ(item->notification_id(), notification.id());
    EXPECT_EQ(item->title(), notification.title());
    EXPECT_EQ(item->message(), notification.message());
  }

  void BeginScroll() {
    DispatchGesture(
        ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));
  }

  void EndScroll() {
    DispatchGesture(ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));
  }

  void ScrollBy(int dx) {
    DispatchGesture(
        ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, dx, 0));
  }

  void DispatchGesture(const ui::GestureEventDetails& details) {
    ui::test::EventGenerator generator(
        GetRootWindow(notification_menu_view_->GetWidget()));

    const auto* item =
        notification_menu_view_->GetDisplayedNotificationItemView();
    ui::GestureEvent event(0, item->GetBoundsInScreen().y(), 0,
                           ui::EventTimeForNow(), details);
    generator.Dispatch(&event);
  }

  float GetSlideAmount() const {
    return notification_menu_view_->GetSlideOutLayer()
        ->transform()
        .To2dTranslation()
        .x();
  }

  NotificationMenuView* notification_menu_view() {
    return notification_menu_view_;
  }

  NotificationMenuViewTestAPI* test_api() { return test_api_.get(); }

  MockNotificationMenuController* mock_notification_menu_controller() {
    return mock_notification_menu_controller_.get();
  }

 private:
  std::unique_ptr<MockNotificationMenuController>
      mock_notification_menu_controller_;
  raw_ptr<NotificationMenuView, DanglingUntriaged> notification_menu_view_;
  std::unique_ptr<NotificationMenuViewTestAPI> test_api_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_scope_;
};

// Tests that the correct NotificationItemView is shown when notifications come
// and go.
TEST_F(NotificationMenuViewTest, Basic) {
  // Add a notification to the view.
  const message_center::Notification notification_0 =
      AddNotification("notification_id_0", u"title_0", u"message_0");

  // The counter should update to 1, and the displayed NotificationItemView
  // should match the notification.
  EXPECT_EQ(base::NumberToString16(1), test_api()->GetCounterViewContents());
  EXPECT_EQ(1, test_api()->GetItemViewCount());
  CheckDisplayedNotification(notification_0);

  // Add a second notification to the view, the counter view and displayed
  // NotificationItemView should change.
  const message_center::Notification notification_1 =
      AddNotification("notification_id_1", u"title_1", u"message_1");
  EXPECT_EQ(base::NumberToString16(2), test_api()->GetCounterViewContents());
  EXPECT_EQ(2, test_api()->GetItemViewCount());
  CheckDisplayedNotification(notification_1);

  // Remove |notification_1|, |notification_0| should be shown.
  notification_menu_view()->OnNotificationRemoved(notification_1.id());
  EXPECT_EQ(base::NumberToString16(1), test_api()->GetCounterViewContents());
  EXPECT_EQ(1, test_api()->GetItemViewCount());
  CheckDisplayedNotification(notification_0);
}

TEST_F(NotificationMenuViewTest, MultipleNotificationsBasic) {
  // Add multiple notifications to the view.
  const message_center::Notification notification_0 =
      AddNotification("notification_id_0", u"title_0", u"message_0");

  // Overflow should not be created until there are two notifications.
  EXPECT_FALSE(test_api()->GetOverflowView());
  EXPECT_EQ(
      0, mock_notification_menu_controller()->overflow_added_or_removed_count_);

  // Add a second notification, this will push |notification_0| into overflow.
  const message_center::Notification notification_1 =
      AddNotification("notification_id_1", u"title_1", u"message_1");

  CheckDisplayedNotification(notification_1);
  EXPECT_TRUE(test_api()->GetOverflowView());
  EXPECT_EQ(
      1, mock_notification_menu_controller()->overflow_added_or_removed_count_);

  // Remove the notification that is in overflow.
  notification_menu_view()->OnNotificationRemoved(notification_0.id());

  // The displayed notification should not have changed, and the overflow view
  // should be deleted.
  CheckDisplayedNotification(notification_1);
  EXPECT_FALSE(test_api()->GetOverflowView());
}

// Tests that when the displayed NotificationItemView is removed, the
// notification from the overflow view becomes the displayed view.
TEST_F(NotificationMenuViewTest, ShowNotificationFromOverflow) {
  // Add multiple notifications to the view.
  const message_center::Notification notification_0 =
      AddNotification("notification_id_0", u"title_0", u"message_0");

  EXPECT_FALSE(test_api()->GetOverflowView());
  const message_center::Notification notification_1 =
      AddNotification("notification_id_1", u"title_1", u"message_1");

  // |notification_1| should be the displayed NotificationItemView.
  CheckDisplayedNotification(notification_1);
  EXPECT_TRUE(test_api()->GetOverflowView());

  // Remove the displayed NotificationItemView, the overflow notification should
  // take its place and overflow should be removed.
  notification_menu_view()->OnNotificationRemoved(notification_1.id());

  CheckDisplayedNotification(notification_0);
  EXPECT_FALSE(test_api()->GetOverflowView());
}

// Tests that removing a notification that is not being shown only updates the
// counter.
TEST_F(NotificationMenuViewTest, RemoveOlderNotification) {
  // Add two notifications.
  const message_center::Notification notification_0 =
      AddNotification("notification_id_0", u"title_0", u"message_0");
  const message_center::Notification notification_1 =
      AddNotification("notification_id_1", u"title_1", u"message_1");

  // The latest notification should be shown.
  EXPECT_EQ(base::NumberToString16(2), test_api()->GetCounterViewContents());
  EXPECT_EQ(2, test_api()->GetItemViewCount());
  CheckDisplayedNotification(notification_1);

  // Remove the older notification, |notification_0|.
  notification_menu_view()->OnNotificationRemoved(notification_0.id());

  // The latest notification, |notification_1|, should be shown.
  EXPECT_EQ(base::NumberToString16(1), test_api()->GetCounterViewContents());
  EXPECT_EQ(1, test_api()->GetItemViewCount());
  CheckDisplayedNotification(notification_1);
}

// Tests that the displayed NotificationItemView is only dismissed when dragged
// beyond the threshold.
TEST_F(NotificationMenuViewTest, SlideOut) {
  AddNotification("notification_id", u"title", u"message");

  EXPECT_EQ(0, mock_notification_menu_controller()->slide_out_count_);

  BeginScroll();
  // Scroll by a small amount, the notification should move but not slide out.
  ScrollBy(-10);
  EXPECT_EQ(0, mock_notification_menu_controller()->slide_out_count_);
  EXPECT_EQ(-10.f, GetSlideAmount());

  // End the scroll gesture, the notifications should return to its resting
  // place.
  EndScroll();
  EXPECT_EQ(0, mock_notification_menu_controller()->slide_out_count_);
  EXPECT_EQ(0.f, GetSlideAmount());

  BeginScroll();
  // Scroll beyond the threshold but do not release the gesture scroll.
  ScrollBy(-200);
  EXPECT_EQ(-200.f, GetSlideAmount());
  // Release the gesture, the notification should slide out.
  EndScroll();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_notification_menu_controller()->slide_out_count_);
  EXPECT_EQ(0, mock_notification_menu_controller()->activation_count_);
}

// Tests that tapping a notification activates it.
TEST_F(NotificationMenuViewTest, TapNotification) {
  AddNotification("notification_id", u"title", u"message");
  EXPECT_EQ(0, mock_notification_menu_controller()->activation_count_);
  DispatchGesture(ui::GestureEventDetails(ui::EventType::kGestureTap));

  EXPECT_EQ(1, mock_notification_menu_controller()->activation_count_);
}

// Tests that an in bounds mouse release activates a notification.
TEST_F(NotificationMenuViewTest, ClickNotification) {
  AddNotification("notification_id", u"title", u"message");
  EXPECT_EQ(0, mock_notification_menu_controller()->activation_count_);

  const auto* item =
      notification_menu_view()->GetDisplayedNotificationItemView();
  const gfx::Point cursor_location = item->GetBoundsInScreen().origin();
  ui::MouseEvent press(ui::EventType::kMousePressed, cursor_location,
                       cursor_location, ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_NONE);
  notification_menu_view()->GetWidget()->OnMouseEvent(&press);
  EXPECT_EQ(0, mock_notification_menu_controller()->activation_count_);

  ui::MouseEvent release(ui::EventType::kMouseReleased, cursor_location,
                         cursor_location, ui::EventTimeForNow(),
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_NONE);
  notification_menu_view()->GetWidget()->OnMouseEvent(&release);
  EXPECT_EQ(1, mock_notification_menu_controller()->activation_count_);
}

// Tests that an out of bounds mouse release does not activate a notification.
TEST_F(NotificationMenuViewTest, OutOfBoundsClick) {
  AddNotification("notification_id", u"title", u"message");
  EXPECT_EQ(0, mock_notification_menu_controller()->activation_count_);

  const auto* item =
      notification_menu_view()->GetDisplayedNotificationItemView();
  const gfx::Point cursor_location = item->GetBoundsInScreen().origin();
  ui::MouseEvent press(ui::EventType::kMousePressed, cursor_location,
                       cursor_location, ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_NONE);
  notification_menu_view()->GetWidget()->OnMouseEvent(&press);
  EXPECT_EQ(0, mock_notification_menu_controller()->activation_count_);

  const gfx::Point out_of_bounds;
  ui::MouseEvent out_of_bounds_release(
      ui::EventType::kMouseReleased, out_of_bounds, out_of_bounds,
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_NONE);
  notification_menu_view()->GetWidget()->OnMouseEvent(&out_of_bounds_release);

  EXPECT_EQ(0, mock_notification_menu_controller()->activation_count_);
}

// Tests updating notifications that do and do not exist.
TEST_F(NotificationMenuViewTest, UpdateNotification) {
  // Add a notification.
  const std::string notification_id = "notification_id";
  AddNotification(notification_id, u"title", u"message");
  // Send an updated notification with a matching |notification_id|.
  const message_center::Notification updated_notification =
      UpdateNotification(notification_id, u"new_title", u"new_message");

  // The displayed notification's contents should have changed to match the
  // updated notification.
  EXPECT_EQ(base::NumberToString16(1), test_api()->GetCounterViewContents());
  EXPECT_EQ(1, test_api()->GetItemViewCount());
  CheckDisplayedNotification(updated_notification);

  // Send an updated notification for a notification which doesn't yet exist.
  UpdateNotification("Bad notification", u"Bad Title", u"Bad Message");

  // Test that the displayed notification has not been changed.
  EXPECT_EQ(base::NumberToString16(1), test_api()->GetCounterViewContents());
  EXPECT_EQ(1, test_api()->GetItemViewCount());
  CheckDisplayedNotification(updated_notification);
}

}  // namespace ash
