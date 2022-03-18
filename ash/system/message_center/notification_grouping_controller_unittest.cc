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

namespace {
const char kIdFormat[] = "id%ld";
}  // namespace

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
  std::string AddNotificationWithOriginUrl(const GURL& origin_url) {
    std::string id;
    MessageCenter::Get()->AddNotification(MakeNotification(id, origin_url));
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
                                                 const GURL& origin_url) {
    id_out = base::StringPrintf(kIdFormat, notifications_counter_);
    message_center::NotifierId notifier_id;
    notifier_id.profile_id = "abc@gmail.com";
    notifier_id.type = message_center::NotifierType::WEB_PAGE;
    auto notification = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, id_out,
        u"id" + base::NumberToString16(notifications_counter_),
        u"message" + base::NumberToString16(notifications_counter_),
        ui::ImageModel(), u"src", origin_url, notifier_id,
        message_center::RichNotificationData(), nullptr);
    notifications_counter_++;
    return notification;
  }

  base::test::ScopedFeatureList scoped_feature_list_;

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

  std::string id_parent = id0 + kIdSuffixForGroupContainerNotification;
  EXPECT_TRUE(message_center->FindNotificationById(id_parent)->group_parent());
}

TEST_F(NotificationGroupingControllerTest, BasicRemoval) {
  std::string id0, id1, id2;
  const GURL url(u"http://test-url.com");
  id0 = AddNotificationWithOriginUrl(url);
  id1 = AddNotificationWithOriginUrl(url);
  id2 = AddNotificationWithOriginUrl(url);

  std::string id_parent = id0 + kIdSuffixForGroupContainerNotification;
  // Group notification should stay intact if a single group notification is
  // removed.
  MessageCenter::Get()->RemoveNotification(id1, true);
  EXPECT_TRUE(
      MessageCenter::Get()->FindNotificationById(id_parent)->group_parent());

  // Adding and removing a non group notification should have no impact.
  std::string tmp = AddNotificationWithOriginUrl(GURL(u"tmp"));
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
  const GURL url(u"http://test-url.com");
  id0 = AddNotificationWithOriginUrl(url);

  std::string tmp;
  tmp = AddNotificationWithOriginUrl(url);

  std::string parent_id = id0 + kIdSuffixForGroupContainerNotification;
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

  MessageCenter::Get()->RemoveNotification(
      id0 + kIdSuffixForGroupContainerNotification, true);

  ASSERT_FALSE(MessageCenter::Get()->HasPopupNotifications());
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

  std::string parent_id = id0 + kIdSuffixForGroupContainerNotification;

  // Toggle the system tray to dismiss all popups.
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  GetPrimaryUnifiedSystemTray()->CloseBubble();

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

  std::string id_parent = id0 + kIdSuffixForGroupContainerNotification;
  EXPECT_TRUE(message_center->FindNotificationById(id_parent)->group_parent());

  // Make sure there is only a single popup (there would be more popups if
  // grouping didn't work)
  EXPECT_EQ(1u, message_center->GetPopupNotifications().size());
}

}  // namespace ash
