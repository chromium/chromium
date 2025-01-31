// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_grouping_controller.h"

#include <memory>

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/system/notification_center/ash_message_popup_collection.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/views/ash_notification_view.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ash/system/notification_center/views/notification_list_view.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "ui/color/color_id.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/notification_view_controller.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/animation/slide_out_controller.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {
const char kIdFormat[] = "id%ld";

class TestNotificationViewController
    : public message_center::NotificationViewController {
 public:
  TestNotificationViewController() = default;

  TestNotificationViewController(const TestNotificationViewController& other) =
      delete;
  TestNotificationViewController& operator=(
      const TestNotificationViewController& other) = delete;

  ~TestNotificationViewController() override = default;

  // message_center::NotificationViewController:
  message_center::MessageView* GetMessageViewForNotificationId(
      const std::string& notification_id) override {
    return nullptr;
  }
  void AnimateResize() override {}
  void ConvertNotificationViewToGroupedNotificationView(
      const std::string& ungrouped_notification_id,
      const std::string& new_grouped_notification_id) override {}
  void ConvertGroupedNotificationViewToNotificationView(
      const std::string& grouped_notification_id,
      const std::string& new_single_notification_id) override {}
  void OnChildNotificationViewUpdated(
      const std::string& parent_notification_id,
      const std::string& child_notification_id) override {
    on_child_updated_called_ = true;
    on_child_updated_parent_id_ = parent_notification_id;
    on_child_updated_child_id_ = child_notification_id;
  }

  bool on_child_updated_called() { return on_child_updated_called_; }
  std::string on_child_updated_parent_id() {
    return on_child_updated_parent_id_;
  }
  std::string on_child_updated_child_id() { return on_child_updated_child_id_; }

 private:
  bool on_child_updated_called_ = false;
  std::string on_child_updated_parent_id_;
  std::string on_child_updated_child_id_;
};

class TestNotificationGroupingController
    : public NotificationGroupingController {
 public:
  explicit TestNotificationGroupingController(
      NotificationCenterTray* notification_tray)
      : NotificationGroupingController(notification_tray) {
    test_view_controller_ = std::make_unique<TestNotificationViewController>();
  }

  TestNotificationGroupingController(
      const TestNotificationGroupingController& other) = delete;
  TestNotificationGroupingController& operator=(
      const TestNotificationGroupingController& other) = delete;

  ~TestNotificationGroupingController() override = default;

  // NotificationGroupingController:
  message_center::NotificationViewController*
  GetActiveNotificationViewController() override {
    return test_view_controller_.get();
  }

 private:
  std::unique_ptr<TestNotificationViewController> test_view_controller_;
};

}  // namespace

class NotificationGroupingControllerTest : public AshTestBase {
 public:
  NotificationGroupingControllerTest() = default;
  NotificationGroupingControllerTest(
      const NotificationGroupingControllerTest& other) = delete;
  NotificationGroupingControllerTest& operator=(
      const NotificationGroupingControllerTest& other) = delete;
  ~NotificationGroupingControllerTest() override = default;

 protected:
  std::string AddNotificationWithOriginUrl(const GURL& origin_url) {
    std::string id;
    MessageCenter::Get()->AddNotification(MakeNotification(id, origin_url));
    return id;
  }

  void AnimateUntilIdle() {
    AshMessagePopupCollection* popup_collection =
        GetPrimaryNotificationCenterTray()->popup_collection();

    while (popup_collection->animation()->is_animating()) {
      popup_collection->animation()->SetCurrentValue(1.0);
      popup_collection->animation()->End();
    }
  }

  message_center::MessagePopupView* GetPopupView(const std::string& id) {
    return GetPrimaryNotificationCenterTray()
        ->popup_collection()
        ->GetPopupViewForNotificationID(id);
  }

  // Construct a new notification for testing.
  std::unique_ptr<Notification> MakeNotification(std::string& id_out,
                                                 const GURL& origin_url) {
    id_out = base::StringPrintf(kIdFormat, notifications_counter_);
    message_center::NotifierId notifier_id;
    notifier_id.profile_id = "abc@gmail.com";
    notifier_id.type = message_center::NotifierType::WEB_PAGE;
    notifier_id.url = origin_url;
    auto notification = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, id_out,
        u"id" + base::NumberToString16(notifications_counter_),
        u"message" + base::NumberToString16(notifications_counter_),
        ui::ImageModel(), u"src", origin_url, notifier_id,
        message_center::RichNotificationData(), nullptr);
    notifications_counter_++;
    return notification;
  }

  std::unique_ptr<Notification> MakeNotificationWithNotifierId(
      std::string& id_out,
      const GURL& origin_url,
      message_center::NotifierId notifier_id) {
    id_out = base::StringPrintf(kIdFormat, notifications_counter_);
    auto notification = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, id_out,
        u"id" + base::NumberToString16(notifications_counter_),
        u"message" + base::NumberToString16(notifications_counter_),
        ui::ImageModel(), u"src", origin_url, notifier_id,
        message_center::RichNotificationData(), nullptr);
    notifications_counter_++;
    return notification;
  }

  void GenerateGestureEvent(const ui::GestureEventDetails& details,
                            views::SlideOutController* slide_out_controller) {
    ui::GestureEvent gesture_event(0, 0, ui::EF_NONE, base::TimeTicks(),
                                   details);
    slide_out_controller->OnGestureEvent(&gesture_event);

    base::RunLoop().RunUntilIdle();
  }

  void GenerateSwipe(int swipe_amount,
                     views::SlideOutController* slide_out_controller) {
    GenerateGestureEvent(
        ui::GestureEventDetails(ui::EventType::kGestureScrollBegin),
        slide_out_controller);
    GenerateGestureEvent(
        ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate,
                                swipe_amount, 0),
        slide_out_controller);
    GenerateGestureEvent(
        ui::GestureEventDetails(ui::EventType::kGestureScrollEnd),
        slide_out_controller);
  }

  views::SlideOutController* GetSlideOutController(AshNotificationView* view) {
    return view->slide_out_controller_for_test();
  }

  bool IsNotificationListViewAnimating() const {
    return GetPrimaryNotificationCenterTray()
        ->GetNotificationListView()
        ->IsAnimating();
  }

  size_t notifications_counter_ = 0;
};

TEST_F(NotificationGroupingControllerTest, BasicGrouping) {
  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2;
  const GURL url(u"http://test-url.com/");
  id0 = AddNotificationWithOriginUrl(url);
  id1 = AddNotificationWithOriginUrl(url);
  id2 = AddNotificationWithOriginUrl(url);

  EXPECT_TRUE(message_center->FindNotificationById(id0)->group_child());
  EXPECT_TRUE(message_center->FindNotificationById(id1)->group_child());
  EXPECT_TRUE(message_center->FindNotificationById(id2)->group_child());

  std::string id_parent =
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center->FindNotificationById(id0)->notifier_id());
  EXPECT_TRUE(message_center->FindNotificationById(id_parent)->group_parent());
}

// Test a situation where a notification being added is already marked
// as a parent.
// In this case, make sure it's added as such and no parent_copy
// notification is added.
TEST_F(NotificationGroupingControllerTest, AddPreparentNotificationGroupsCorrectly) {
  auto* message_center = MessageCenter::Get();
  std::string id0, id1;
  const GURL url(u"http://test-url.com/");
  auto parent_notification = MakeNotification(id0, url);
  auto child_notification = MakeNotification(id1, url);
  parent_notification->SetGroupParent();
  message_center->AddNotification(std::move(parent_notification));
  message_center->AddNotification(std::move(child_notification));

  EXPECT_TRUE(message_center->FindNotificationById(id0)->group_parent());
  EXPECT_TRUE(message_center->FindNotificationById(id1)->group_child());

  std::string id_parent =
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center->FindNotificationById(id0)->notifier_id());
  EXPECT_FALSE(message_center->FindNotificationById(id_parent));
}

TEST_F(NotificationGroupingControllerTest, BasicRemoval) {
  std::string id0, id1, id2;
  const GURL url(u"http://test-url.com");
  id0 = AddNotificationWithOriginUrl(url);
  id1 = AddNotificationWithOriginUrl(url);
  id2 = AddNotificationWithOriginUrl(url);

  auto* message_center = MessageCenter::Get();
  std::string id_parent =
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center->FindNotificationById(id0)->notifier_id());
  // Group notification should stay intact if a single group notification is
  // removed.
  message_center->RemoveNotification(id1, true);
  EXPECT_TRUE(message_center->FindNotificationById(id_parent)->group_parent());

  // Adding and removing a non group notification should have no impact.
  std::string tmp = AddNotificationWithOriginUrl(GURL(u"tmp"));
  message_center->RemoveNotification(tmp, true);

  EXPECT_TRUE(message_center->FindNotificationById(id0)->group_child());
  EXPECT_TRUE(message_center->FindNotificationById(id2)->group_child());
  EXPECT_TRUE(message_center->FindNotificationById(id_parent)->group_parent());
}

// Tests that having a grouped notification as the latest notification does not
// animate the notification list. This happened because latest notifications get
// expanded and it was rapidly being collapsed due to it being grouped.
TEST_F(NotificationGroupingControllerTest, LatestNotificationDoesNotAnimate) {
  // Add two grouped notifications.
  const GURL url(u"http://test-url.com");
  AddNotificationWithOriginUrl(url);
  AddNotificationWithOriginUrl(url);

  // Show the notification center.
  GetPrimaryNotificationCenterTray()->ShowBubble();

  // List should not be animating when the latest notification is grouped.
  EXPECT_FALSE(IsNotificationListViewAnimating());
}

TEST_F(NotificationGroupingControllerTest,
       ParentNotificationReshownWithNewChild) {
  TrayBackgroundView* tray;

  tray = GetPrimaryNotificationCenterTray();

  std::string id0;
  const GURL url(u"http://test-url.com");
  id0 = AddNotificationWithOriginUrl(url);

  std::string tmp;
  tmp = AddNotificationWithOriginUrl(url);

  std::string parent_id =
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                MessageCenter::Get()->FindNotificationById(id0)->notifier_id());
  EXPECT_TRUE(GetPopupView(parent_id));

  // Toggle the system tray to dismiss all popups.
  tray->ShowBubble();
  tray->CloseBubble();

  EXPECT_FALSE(GetPopupView(parent_id));

  // Adding notification with a different notifier id should have no effect.
  AddNotificationWithOriginUrl(GURL("tmp"));
  EXPECT_FALSE(GetPopupView(parent_id));

  AddNotificationWithOriginUrl(url);

  // Move down or fade in animation might happen before showing the popup.
  AnimateUntilIdle();

  EXPECT_TRUE(GetPopupView(parent_id));
}

TEST_F(NotificationGroupingControllerTest,
       RemovingParentRemovesChildGroupNotifications) {
  std::string id0;
  const GURL url(u"http://test-url.com");
  id0 = AddNotificationWithOriginUrl(url);

  std::string tmp;
  AddNotificationWithOriginUrl(url);
  AddNotificationWithOriginUrl(url);

  auto* message_center = MessageCenter::Get();
  MessageCenter::Get()->RemoveNotification(
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center->FindNotificationById(id0)->notifier_id()),
      true);

  ASSERT_FALSE(message_center->HasPopupNotifications());
}

TEST_F(NotificationGroupingControllerTest,
       RepopulatedParentNotificationRemoval) {
  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2, id3, id4;
  const GURL url(u"http://test-url.com");
  id0 = AddNotificationWithOriginUrl(url);
  id1 = AddNotificationWithOriginUrl(url);
  id2 = AddNotificationWithOriginUrl(url);
  id3 = AddNotificationWithOriginUrl(url);

  std::string parent_id =
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center->FindNotificationById(id0)->notifier_id());

  // Toggle the notification tray to dismiss all popups.
  GetPrimaryNotificationCenterTray()->ShowBubble();
  GetPrimaryNotificationCenterTray()->CloseBubble();

  ASSERT_FALSE(MessageCenter::Get()->HasPopupNotifications());

  id4 = AddNotificationWithOriginUrl(url);

  AnimateUntilIdle();

  message_center->RemoveNotification(id0, true);
  message_center->RemoveNotification(id1, true);
  message_center->RemoveNotification(id2, true);
  message_center->RemoveNotification(id3, true);

  auto* last_child = MessageCenter::Get()->FindNotificationById(id4);
  auto* parent = MessageCenter::Get()->FindNotificationById(parent_id);

  EXPECT_TRUE(last_child->group_child());
  EXPECT_TRUE(parent->group_parent());
}

TEST_F(NotificationGroupingControllerTest, ParentNotificationMetadata) {
  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2;
  const GURL url(u"http://test-url.com/");
  const auto icon = gfx::VectorIcon();
  const auto small_image = gfx::Image();
  const std::u16string display_source0 = u"test_display_source0";

  auto notification = MakeNotification(id0, url);
  notification->set_accent_color_id(ui::kColorAshSystemUIMenuIcon);
  notification->set_accent_color(SK_ColorRED);
  notification->set_parent_vector_small_image(icon);
  notification->SetSmallImage(small_image);
  notification->set_display_source(display_source0);
  message_center->AddNotification(std::move(notification));

  id1 = AddNotificationWithOriginUrl(url);
  id2 = AddNotificationWithOriginUrl(url);

  auto* parent_notification = message_center->FindNotificationById(
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center->FindNotificationById(id0)->notifier_id()));
  EXPECT_TRUE(parent_notification->group_parent());

  // Parent notification should inherit attributes from the child.
  EXPECT_EQ(ui::kColorAshSystemUIMenuIcon,
            parent_notification->accent_color_id());
  EXPECT_EQ(SK_ColorRED, parent_notification->accent_color());
  EXPECT_EQ(&icon, &parent_notification->vector_small_image());
  EXPECT_EQ(small_image, parent_notification->small_image());
  EXPECT_EQ(display_source0, parent_notification->display_source());
}

// Parent notification's priority should always match the priority of the last
// added notification to the group.
TEST_F(NotificationGroupingControllerTest, ParentNotificationPriority) {
  auto* message_center = MessageCenter::Get();
  std::string id1, id2, id3;
  const GURL url(u"http://test-url.com/");

  auto notification = MakeNotification(id1, url);
  notification->set_priority(message_center::LOW_PRIORITY);
  message_center->AddNotification(std::move(notification));

  auto notification2 = MakeNotification(id2, url);
  notification2->set_priority(message_center::LOW_PRIORITY);
  message_center->AddNotification(std::move(notification2));

  auto* parent_notification = message_center->FindNotificationById(
      id1 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center->FindNotificationById(id1)->notifier_id()));
  EXPECT_TRUE(parent_notification->group_parent());

  EXPECT_EQ(message_center::LOW_PRIORITY, parent_notification->priority());

  auto notification3 = MakeNotification(id3, url);
  notification3->set_priority(message_center::HIGH_PRIORITY);
  message_center->AddNotification(std::move(notification3));

  EXPECT_EQ(message_center::HIGH_PRIORITY, parent_notification->priority());
}

TEST_F(NotificationGroupingControllerTest,
       NotificationsGroupingOnMultipleScreens) {
  UpdateDisplay("800x600,800x600");
  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2;
  const GURL url(u"http://test-url.com/");
  id0 = AddNotificationWithOriginUrl(url);
  id1 = AddNotificationWithOriginUrl(url);
  id2 = AddNotificationWithOriginUrl(url);

  EXPECT_TRUE(message_center->FindNotificationById(id0)->group_child());
  EXPECT_TRUE(message_center->FindNotificationById(id1)->group_child());
  EXPECT_TRUE(message_center->FindNotificationById(id2)->group_child());

  std::string id_parent =
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center->FindNotificationById(id0)->notifier_id());
  EXPECT_TRUE(message_center->FindNotificationById(id_parent)->group_parent());

  // Make sure there is only a single popup (there would be more popups if
  // grouping didn't work)
  EXPECT_EQ(1u, message_center->GetPopupNotifications().size());
}

// Even though it is not a web notification, privacy indicators notification
// should group together.
TEST_F(NotificationGroupingControllerTest, GroupPrivacyIndicatorsNotification) {
  auto* message_center = MessageCenter::Get();
  std::string id0, id1;
  const GURL url0(u"http://test-url1.com/");
  const GURL url1(u"http://test-url2.com/");
  auto notifier_id =
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kPrivacyIndicatorsNotifierId,
                                 NotificationCatalogName::kPrivacyIndicators);
  auto notification0 = MakeNotificationWithNotifierId(id0, url0, notifier_id);
  auto notification1 = MakeNotificationWithNotifierId(id1, url1, notifier_id);
  message_center->AddNotification(std::move(notification0));
  message_center->AddNotification(std::move(notification1));

  std::string id_parent =
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center->FindNotificationById(id0)->notifier_id());
  EXPECT_TRUE(message_center->FindNotificationById(id_parent));
}

// Create a group notification while the message center bubble is
// is shown.
TEST_F(NotificationGroupingControllerTest,
       NotificationsGroupingMessageCenterBubbleShown) {
  GetPrimaryNotificationCenterTray()->ShowBubble();

  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2;
  const GURL url(u"http://test-url.com/");

  id0 = AddNotificationWithOriginUrl(url);
  id1 = AddNotificationWithOriginUrl(url);
  id2 = AddNotificationWithOriginUrl(url);

  EXPECT_TRUE(message_center->FindNotificationById(id0)->group_child());
  EXPECT_TRUE(message_center->FindNotificationById(id1)->group_child());

  std::string id_parent =
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center->FindNotificationById(id0)->notifier_id());
  EXPECT_TRUE(message_center->FindNotificationById(id_parent)->group_parent());
}

TEST_F(NotificationGroupingControllerTest,
       GroupedNotificationRemovedDuringAnimation) {
  auto* message_center = MessageCenter::Get();
  std::string id0, id1;
  const GURL url(u"http://test-url.com/");

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  id0 = AddNotificationWithOriginUrl(url);
  id1 = AddNotificationWithOriginUrl(url);

  // Remove notification with `id` before the animation completes.
  message_center->RemoveNotification(id1, true);

  // Wait for the animation to end to ensure there is no crash
  ui::LayerAnimationStoppedWaiter waiter;
  waiter.Wait(GetPopupView(id0)->message_view()->layer());
}

TEST_F(NotificationGroupingControllerTest,
       ParentNotificationRemovedDuringAnimation) {
  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto* message_center = MessageCenter::Get();
  std::string id0, id1;
  const GURL url(u"http://test-url.com/");

  id0 = AddNotificationWithOriginUrl(url);
  id1 = AddNotificationWithOriginUrl(url);

  // Remove the first notification before the animation completes.
  message_center->RemoveNotification(id0, true);

  // Wait for the animation to end to ensure there is no crash
  ui::LayerAnimationStoppedWaiter waiter;
  waiter.Wait(GetPopupView(id0)->message_view()->layer());

  // Make sure the second notification is still there.
  EXPECT_FALSE(message_center->FindNotificationById(id0));
  EXPECT_TRUE(message_center->FindNotificationById(id1));
}

// Regression test for b/251686768. Tests that a grouped notification is
// correctly dismissed when swiped in the collapse state rather than moved into
// the center of the screen. Also, tests that the correct notifications are
// dismissed by swiping in the expanded state.
TEST_F(NotificationGroupingControllerTest, NotificationSwipeGestureBehavior) {
  auto* message_center = MessageCenter::Get();
  std::string parent_id, id0, id1, id2, id3;
  const GURL url(u"http://test-url.com/");

  id0 = AddNotificationWithOriginUrl(url);
  id1 = AddNotificationWithOriginUrl(url);
  id2 = AddNotificationWithOriginUrl(url);
  id3 = AddNotificationWithOriginUrl(url);

  parent_id =
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center->FindNotificationById(id0)->notifier_id());

  AshNotificationView* parent_message_view = static_cast<AshNotificationView*>(
      GetPopupView(parent_id)->message_view());

  auto* message_view_2 =
      GetPopupView(parent_id)->message_view()->FindGroupNotificationView(id2);
  auto* message_view_3 =
      GetPopupView(parent_id)->message_view()->FindGroupNotificationView(id3);

  parent_message_view->ToggleExpand();
  EXPECT_TRUE(parent_message_view->IsExpanded());

  // Swiping out a group child notification while the parent notification is
  // expanded should only slide and remove the group child notification.
  GenerateSwipe(300, GetSlideOutController(
                         static_cast<AshNotificationView*>(message_view_3)));
  EXPECT_FALSE(message_center->FindNotificationById(id3));
  EXPECT_TRUE(message_center->FindNotificationById(parent_id));

  parent_message_view->ToggleExpand();
  EXPECT_FALSE(parent_message_view->IsExpanded());

  // Swiping out a group child notification while the parent notification is
  // collapsed should dismiss the popup but keep all notifications in the
  // notification center.
  GenerateSwipe(300, GetSlideOutController(
                         static_cast<AshNotificationView*>(message_view_2)));

  EXPECT_FALSE(message_center->FindPopupNotificationById(parent_id));
  EXPECT_TRUE(message_center->FindNotificationById(id1));
  EXPECT_TRUE(message_center->FindNotificationById(id2));
}

// Regression test for b/251684908. Tests that a duplicate `AddNotification`
// event does not cause the associated notification popup to be dismissed or the
// original notification to be grouped incorrectly.
TEST_F(NotificationGroupingControllerTest, DuplicateAddNotificationNotGrouped) {
  std::string id = AddNotificationWithOriginUrl(GURL(u"http://test-url.com/"));

  auto* popup = GetPopupView(id);
  EXPECT_TRUE(popup->GetVisible());

  auto* message_center = message_center::MessageCenter::Get();

  // Add a copy of the original notification.
  auto* original_notification = message_center->FindNotificationById(id);
  message_center->AddNotification(
      std::make_unique<Notification>(*original_notification));

  // Add a new notification to force an update to all notification popups.
  AddNotificationWithOriginUrl(GURL(u"http://other-url.com/"));

  // Make sure the popup for the `original_notification` still exists and is
  // visible. Also, make sure the `original_notification` was not grouped.
  EXPECT_TRUE(GetPopupView(id));
  EXPECT_TRUE(popup->GetVisible());
  EXPECT_FALSE(message_center->FindNotificationById(id)->group_child());
}

TEST_F(NotificationGroupingControllerTest, ChildNotificationUpdate) {
  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2;
  const GURL url(u"http://test-url.com/");
  id0 = AddNotificationWithOriginUrl(url);
  id1 = AddNotificationWithOriginUrl(url);

  EXPECT_TRUE(message_center->FindNotificationById(id0)->group_child());

  // Update the notification.
  auto notification = MakeNotification(id2, url);
  auto updated_notification =
      std::make_unique<Notification>(id0, *notification.get());
  message_center->UpdateNotification(id0, std::move(updated_notification));

  // Make sure the updated notification is still a group child.
  EXPECT_TRUE(message_center->FindNotificationById(id0)->group_child());
}

TEST_F(NotificationGroupingControllerTest, ChildNotificationViewUpdate) {
  TestNotificationGroupingController test_controller(
      GetPrimaryNotificationCenterTray());

  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2;
  const GURL url(u"http://test-url.com/");
  id0 = AddNotificationWithOriginUrl(url);
  id1 = AddNotificationWithOriginUrl(url);
  std::string parent_id =
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center->FindNotificationById(id0)->notifier_id());

  EXPECT_TRUE(message_center->FindNotificationById(id0)->group_child());

  // Update the notification.
  auto notification = MakeNotification(id2, url);
  auto updated_notification =
      std::make_unique<Notification>(id0, *notification.get());
  message_center->UpdateNotification(id0, std::move(updated_notification));

  auto* notification_view_controller =
      static_cast<TestNotificationViewController*>(
          test_controller.GetActiveNotificationViewController());

  // When a child notification is updated, `OnChildNotificationViewUpdated()`
  // should be called with the correct parent and child ids.
  EXPECT_TRUE(notification_view_controller->on_child_updated_called());
  EXPECT_EQ(parent_id,
            notification_view_controller->on_child_updated_parent_id());
  EXPECT_EQ(id0, notification_view_controller->on_child_updated_child_id());
}

// When the last child of the group notification is removed, its parent
// notification should be removed as well. We are testing in the case where
// there is no popup or notification center is not showing.
// TODO(crbug.com/1417929): Re-enable this test
TEST_F(NotificationGroupingControllerTest, DISABLED_ChildNotificationRemove) {
  auto* message_center = MessageCenter::Get();
  std::string id0, id1;
  const GURL url(u"http://test-url.com/");
  id0 = AddNotificationWithOriginUrl(url);
  id1 = AddNotificationWithOriginUrl(url);
  std::string id_parent =
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center->FindNotificationById(id0)->notifier_id());

  // Toggle the system tray to dismiss all popups.
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  GetPrimaryUnifiedSystemTray()->CloseBubble();

  EXPECT_EQ(3u, message_center->GetVisibleNotifications().size());

  // Remove one child. Parent notification is still retained.
  message_center->RemoveNotification(id1, /*by_user=*/false);
  EXPECT_EQ(2u, message_center->GetVisibleNotifications().size());
  EXPECT_TRUE(message_center->FindNotificationById(id_parent));
  EXPECT_FALSE(message_center->FindNotificationById(id1));
  EXPECT_TRUE(message_center->FindNotificationById(id0));

  // Remove the last child notification. Parent notification should be removed.
  message_center->RemoveNotification(id0, /*by_user=*/false);
  EXPECT_EQ(0u, message_center->GetVisibleNotifications().size());
  EXPECT_FALSE(message_center->FindNotificationById(id_parent));
  EXPECT_FALSE(message_center->FindNotificationById(id0));
}

TEST_F(NotificationGroupingControllerTest,
       ChildNotificationsWithDifferentPriorities) {
  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2;
  const GURL url(u"http://test-url.com/");

  // Create 2 notifications with low priority, the parent notification should be
  // low priority as well.
  auto notification = MakeNotification(id0, url);
  notification->set_priority(message_center::LOW_PRIORITY);
  message_center->AddNotification(std::move(notification));

  notification = MakeNotification(id1, url);
  notification->set_priority(message_center::LOW_PRIORITY);
  message_center->AddNotification(std::move(notification));

  std::string id_parent =
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center->FindNotificationById(id0)->notifier_id());
  auto* parent_notification = message_center->FindNotificationById(id_parent);
  ASSERT_TRUE(parent_notification->group_parent());
  ASSERT_EQ(message_center::LOW_PRIORITY, parent_notification->priority());

  // Now create another notification in the group with normal priority. Parent
  // should still have the same id and have normal priority now.
  id2 = AddNotificationWithOriginUrl(url);
  parent_notification = message_center->FindNotificationById(id_parent);
  ASSERT_TRUE(parent_notification);
  ASSERT_EQ(message_center::DEFAULT_PRIORITY, parent_notification->priority());

  // There should be 4 notifications now (no duplicates created).
  EXPECT_EQ(4u, message_center->GetVisibleNotifications().size());

  // Remove one child then add back. Parent notification is still retained with
  // same id.
  message_center->RemoveNotification(id0,
                                     /*by_user=*/false);
  notification = MakeNotification(id0, url);
  notification->set_priority(message_center::LOW_PRIORITY);
  message_center->AddNotification(std::move(notification));

  parent_notification = message_center->FindNotificationById(id_parent);
  ASSERT_TRUE(parent_notification->group_parent());

  // Should have no duplicates.
  EXPECT_EQ(4u, message_center->GetVisibleNotifications().size());
}

TEST_F(NotificationGroupingControllerTest, ChildNotificationsPinned) {
  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2;
  const GURL url(u"http://test-url.com/");

  // Creates 2 notifications with one is pinned, the parent notification should
  // be pinned as well.
  auto notification = MakeNotification(id0, url);
  notification->set_pinned(false);
  message_center->AddNotification(std::move(notification));

  notification = MakeNotification(id1, url);
  notification->set_pinned(true);
  message_center->AddNotification(std::move(notification));

  std::string id_parent =
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center->FindNotificationById(id0)->notifier_id());
  auto* parent_notification = message_center->FindNotificationById(id_parent);
  ASSERT_TRUE(parent_notification->group_parent());
  EXPECT_TRUE(parent_notification->pinned());

  // Adds another pinned notification. Parent should still be pinned.
  notification = MakeNotification(id2, url);
  notification->set_pinned(true);
  message_center->AddNotification(std::move(notification));

  EXPECT_TRUE(parent_notification->pinned());

  // Removes one pinned notification. Parent should still be pinned.
  message_center->RemoveNotification(id1,
                                     /*by_user=*/false);

  EXPECT_TRUE(parent_notification->pinned());

  // Removes the other pinned notification. Parent then should not be pinned.
  message_center->RemoveNotification(id2,
                                     /*by_user=*/false);

  EXPECT_FALSE(parent_notification->pinned());

  // Adds back the pinned notification. Parent should now be pinned.
  notification = MakeNotification(id2, url);
  notification->set_pinned(true);
  message_center->AddNotification(std::move(notification));

  EXPECT_TRUE(parent_notification->pinned());
}

TEST_F(NotificationGroupingControllerTest, ChildNotificationsUpdatePinned) {
  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2, id3;
  const GURL url(u"http://test-url.com/");

  // Creates 2 un-pinned notifications, the parent notification should be
  // un-pinned as well.
  auto notification = MakeNotification(id0, url);
  notification->set_pinned(false);
  message_center->AddNotification(std::move(notification));

  notification = MakeNotification(id1, url);
  notification->set_pinned(false);
  message_center->AddNotification(std::move(notification));

  std::string id_parent =
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center->FindNotificationById(id0)->notifier_id());
  auto* parent_notification = message_center->FindNotificationById(id_parent);
  ASSERT_TRUE(parent_notification->group_parent());
  EXPECT_FALSE(parent_notification->pinned());

  // Updates one to pinned, the parent should be pinned now.
  notification = MakeNotification(id2, url);
  notification->set_pinned(true);
  message_center->UpdateNotification(id1, std::move(notification));

  EXPECT_TRUE(parent_notification->pinned());

  // Updates again to un-pinned, the parent should be un-pinned.
  notification = MakeNotification(id3, url);
  notification->set_pinned(false);
  message_center->UpdateNotification(id2, std::move(notification));

  EXPECT_FALSE(parent_notification->pinned());
}

TEST_F(NotificationGroupingControllerTest,
       ArcNotificationGroupingWithoutGroupKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kRenderArcNotificationsByChrome);
  auto* message_center = MessageCenter::Get();

  const GURL url(u"http://test-url.com/");
  std::string id0;
  auto arc_notifier_id = message_center::NotifierId(
      message_center::NotifierType::ARC_APPLICATION, "test-id");

  // Add 4 ARC notifications.
  message_center->AddNotification(
      MakeNotificationWithNotifierId(id0, url, arc_notifier_id));
  for (int i = 0; i < 3; i++) {
    std::string tmp;
    message_center->AddNotification(
        MakeNotificationWithNotifierId(tmp, url, arc_notifier_id));
  }

  // Make sure there is no grouping with 4 ARC notifications.
  auto notifications = message_center->GetVisibleNotifications();
  EXPECT_EQ(notifications.size(), 4u);
  for (Notification* n : notifications) {
    EXPECT_FALSE(n->group_child() || n->group_parent());
  }

  for (int i = 0; i < 3; i++) {
    std::string tmp;
    message_center->AddNotification(
        MakeNotificationWithNotifierId(tmp, url, arc_notifier_id));
  }

  // Make sure there is one notification set as the parent and all others are
  // set to group children.
  std::string parent_id =
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center->FindNotificationById(id0)->notifier_id());
  notifications = message_center->GetVisibleNotifications();
  for (Notification* n : notifications) {
    if (n->id() == parent_id) {
      EXPECT_TRUE(n->group_parent());
      continue;
    }
    EXPECT_TRUE(n->group_child());
  }
}

// Test to make sure `web_app_id` in the `NotifierId` is used to determine
// grouping.
TEST_F(NotificationGroupingControllerTest, WebAppIdImpactsGrouping) {
  GURL origin_url = GURL("http://test-url.com");
  auto notifier_id = message_center::NotifierId(origin_url);

  auto web_app_notifier_id = message_center::NotifierId(notifier_id);
  web_app_notifier_id.web_app_id = "test-web-app";

  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2;
  message_center->AddNotification(
      MakeNotificationWithNotifierId(id0, origin_url, notifier_id));
  message_center->AddNotification(
      MakeNotificationWithNotifierId(id1, origin_url, web_app_notifier_id));

  // Make sure notifications with the same origin don't get grouped if one of
  // them has a populated `web_app_id`.
  EXPECT_FALSE(message_center->FindNotificationById(id0)->group_child());
  EXPECT_FALSE(message_center->FindNotificationById(id1)->group_child());

  // Add another notification with `web_notifier_id` and expect the
  // notifications with it to be grouped while the notification without the
  // `web_app_id` should remain single.
  message_center->AddNotification(
      MakeNotificationWithNotifierId(id2, origin_url, web_app_notifier_id));

  EXPECT_FALSE(message_center->FindNotificationById(id0)->group_child());
  EXPECT_TRUE(message_center->FindNotificationById(id1)->group_child());
  EXPECT_TRUE(message_center->FindNotificationById(id1)->group_child());
}

// Test to make sure a notification update coming from a PWA for an existing web
// notification is handled appropriately.
TEST_F(NotificationGroupingControllerTest, PWANotificationUpdate) {
  GURL origin_url = GURL("http://test-url.com");
  auto notifier_id = message_center::NotifierId(origin_url);

  auto web_app_notifier_id = message_center::NotifierId(notifier_id);
  web_app_notifier_id.web_app_id = "test-web-app";

  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2, id3;
  message_center->AddNotification(
      MakeNotificationWithNotifierId(id0, origin_url, notifier_id));
  message_center->AddNotification(
      MakeNotificationWithNotifierId(id1, origin_url, notifier_id));
  message_center->AddNotification(
      MakeNotificationWithNotifierId(id2, origin_url, notifier_id));

  // Add a notification with `id0` to trigger an update with a different
  // notifier_id.
  std::string temp_id;
  auto updated_notification =
      MakeNotificationWithNotifierId(temp_id, origin_url, web_app_notifier_id);
  message_center->AddNotification(
      std::make_unique<message_center::Notification>(id0,
                                                     *updated_notification));

  // Make sure the notification `id0` is no longer grouped and the rest of the
  // group is unchanged.
  auto* notification0 = message_center->FindNotificationById(id0);
  auto* notification1 = message_center->FindNotificationById(id1);
  EXPECT_FALSE(notification0->group_child());
  EXPECT_TRUE(notification1->group_child());
  EXPECT_TRUE(message_center->FindNotificationById(id2)->group_child());

  // Adding another notification with the `web_app_notifier_id` should result in
  // 2 separate grouped notifications.
  message_center->AddNotification(
      MakeNotificationWithNotifierId(id3, origin_url, web_app_notifier_id));

  auto* parent_notification0 =
      message_center->FindParentNotification(notification0);
  auto* parent_notification1 =
      message_center->FindParentNotification(notification1);
  EXPECT_TRUE(parent_notification0);
  EXPECT_TRUE(parent_notification1);
  EXPECT_NE(parent_notification0, parent_notification1);
}

}  // namespace ash
