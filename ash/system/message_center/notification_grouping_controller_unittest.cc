// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/notification_grouping_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/system/message_center/ash_message_popup_collection.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/message_center/views/message_popup_view.h"

using message_center::kIdSuffixForGroupContainerNotification;
using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

class NotificationGroupingControllerTest : public AshTestBase {
 public:
  NotificationGroupingControllerTest() = default;
  NotificationGroupingControllerTest(
      const NotificationGroupingControllerTest& other) = delete;
  NotificationGroupingControllerTest& operator=(
      const NotificationGroupingControllerTest& other) = delete;
  ~NotificationGroupingControllerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kNotificationsRefresh);
    AshTestBase::SetUp();
  }

 protected:
  std::string AddNotificationWithNotifierId(std::string notifier_id) {
    std::string id;
    MessageCenter::Get()->AddNotification(MakeNotification(id, notifier_id));
    return id;
  }

  void AnimateUntilIdle() {
    AshMessagePopupCollection* popup_collection =
        GetPrimaryUnifiedSystemTray()->GetMessagePopupCollection();

    while (popup_collection->animation()->is_animating()) {
      popup_collection->animation()->SetCurrentValue(1.0);
      popup_collection->animation()->End();
    }
  }

  message_center::MessagePopupView* GetPopupView(const std::string& id) {
    return GetPrimaryUnifiedSystemTray()->GetPopupViewForNotificationID(id);
  }

  // Construct a new notification for testing.
  std::unique_ptr<Notification> MakeNotification(std::string& id_out,
                                                 std::string notifier_id) {
    id_out = base::StringPrintf(kIdFormat, notifications_counter_);
    auto notification = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, id_out,
        u"id" + base::NumberToString16(notifications_counter_),
        u"message" + base::NumberToString16(notifications_counter_),
        gfx::Image(), u"src", GURL(),
        message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                   notifier_id),
        message_center::RichNotificationData(), nullptr);
    notifications_counter_++;
    return notification;
  }

  static const char kIdFormat[];

  base::test::ScopedFeatureList scoped_feature_list_;

  size_t notifications_counter_ = 0;
};

const char NotificationGroupingControllerTest::kIdFormat[] = "id%ld";

TEST_F(NotificationGroupingControllerTest, BasicGrouping) {
  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2;
  const char group_id[] = "group";
  id0 = AddNotificationWithNotifierId(group_id);
  id1 = AddNotificationWithNotifierId(group_id);
  id2 = AddNotificationWithNotifierId(group_id);

  EXPECT_TRUE(message_center->FindNotificationById(id0)->group_child());
  EXPECT_TRUE(message_center->FindNotificationById(id1)->group_child());
  EXPECT_TRUE(message_center->FindNotificationById(id2)->group_child());

  std::string id_parent = id0 + kIdSuffixForGroupContainerNotification;
  EXPECT_TRUE(message_center->FindNotificationById(id_parent)->group_parent());
}

TEST_F(NotificationGroupingControllerTest, BasicRemoval) {
  std::string id0, id1, id2;
  const char group_id[] = "group";
  id0 = AddNotificationWithNotifierId(group_id);
  id1 = AddNotificationWithNotifierId(group_id);
  id2 = AddNotificationWithNotifierId(group_id);

  std::string id_parent = id0 + kIdSuffixForGroupContainerNotification;
  // Group notification should stay intact if a single group notification is
  // removed.
  MessageCenter::Get()->RemoveNotification(id1, true);
  EXPECT_TRUE(
      MessageCenter::Get()->FindNotificationById(id_parent)->group_parent());

  // Adding and removing a non group notification should have no impact.
  std::string tmp = AddNotificationWithNotifierId("tmp");
  MessageCenter::Get()->RemoveNotification(tmp, true);

  EXPECT_TRUE(MessageCenter::Get()->FindNotificationById(id0)->group_child());
  EXPECT_TRUE(MessageCenter::Get()->FindNotificationById(id2)->group_child());
  EXPECT_TRUE(
      MessageCenter::Get()->FindNotificationById(id_parent)->group_parent());
}

TEST_F(NotificationGroupingControllerTest,
       ParentNotificationReshownWithNewChild) {
  auto* tray = GetPrimaryUnifiedSystemTray();

  std::string id0;
  const char group_id[] = "group";
  id0 = AddNotificationWithNotifierId(group_id);

  std::string tmp;
  tmp = AddNotificationWithNotifierId(group_id);

  std::string parent_id = id0 + kIdSuffixForGroupContainerNotification;
  EXPECT_TRUE(GetPopupView(parent_id));

  // Toggle the system tray to dismiss all popups.
  tray->ShowBubble();
  tray->CloseBubble();

  EXPECT_FALSE(GetPopupView(parent_id));

  // Adding notification with a different notifier id should have no effect.
  AddNotificationWithNotifierId("tmp");
  EXPECT_FALSE(GetPopupView(parent_id));

  AddNotificationWithNotifierId(group_id);

  // Move down or fade in animation might happen before showing the popup.
  AnimateUntilIdle();

  EXPECT_TRUE(GetPopupView(parent_id));
}

TEST_F(NotificationGroupingControllerTest,
       RemovingParentRemovesChildGroupNotifications) {
  std::string id0;
  const char group_id[] = "group";
  id0 = AddNotificationWithNotifierId(group_id);

  std::string tmp;
  AddNotificationWithNotifierId(group_id);
  AddNotificationWithNotifierId(group_id);

  MessageCenter::Get()->RemoveNotification(
      id0 + kIdSuffixForGroupContainerNotification, true);

  ASSERT_FALSE(MessageCenter::Get()->HasPopupNotifications());
}

TEST_F(NotificationGroupingControllerTest,
       ConvertingGroupedNotificationToSingleNotificationAndBack) {
  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2;
  const char group_id[] = "group";
  id0 = AddNotificationWithNotifierId(group_id);
  id1 = AddNotificationWithNotifierId(group_id);
  id2 = AddNotificationWithNotifierId(group_id);

  std::string parent_id = id0 + kIdSuffixForGroupContainerNotification;
  EXPECT_TRUE(
      MessageCenter::Get()->FindNotificationById(parent_id)->group_parent());

  // Removing all but 1 notification should convert it back to a single
  // notification and result in the removal of the parent notification.
  message_center->RemoveNotification(id0, true);
  message_center->RemoveNotification(id1, true);

  auto* single_notification = message_center->FindNotificationById(id2);
  EXPECT_FALSE(single_notification->group_child() ||
               single_notification->group_parent());
  EXPECT_FALSE(message_center->FindNotificationById(parent_id));

  // Adding further notifications should create a new group with the parent id
  // being derived from `id2`.
  id0 = AddNotificationWithNotifierId(group_id);
  id1 = AddNotificationWithNotifierId(group_id);

  parent_id = id2 + kIdSuffixForGroupContainerNotification;
  EXPECT_TRUE(message_center->FindNotificationById(parent_id));
}

TEST_F(NotificationGroupingControllerTest,
       ConvertingRepopulatedParentToSingleNotification) {
  auto* message_center = MessageCenter::Get();
  std::string id0, id1, id2, id3, id4;
  const char group_id[] = "group";
  id0 = AddNotificationWithNotifierId(group_id);
  id1 = AddNotificationWithNotifierId(group_id);
  id2 = AddNotificationWithNotifierId(group_id);
  id3 = AddNotificationWithNotifierId(group_id);

  std::string parent_id = id0 + kIdSuffixForGroupContainerNotification;

  // Toggle the system tray to dismiss all popups.
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  GetPrimaryUnifiedSystemTray()->CloseBubble();

  id4 = AddNotificationWithNotifierId(group_id);

  AnimateUntilIdle();

  message_center->RemoveNotification(id0, true);
  message_center->RemoveNotification(id1, true);
  message_center->RemoveNotification(id2, true);
  message_center->RemoveNotification(id3, true);

  auto* single_notification = MessageCenter::Get()->FindNotificationById(id4);
  EXPECT_FALSE(single_notification->group_child() ||
               single_notification->group_parent());
  EXPECT_FALSE(MessageCenter::Get()->FindNotificationById(parent_id));
}

}  // namespace ash
