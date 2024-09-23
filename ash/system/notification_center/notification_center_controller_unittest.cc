// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_controller.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/notification_center/notification_center_bubble.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/views/ash_notification_view.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ash/system/notification_center/views/notification_list_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

namespace {

NotificationCenterTray* GetNotificationCenterTray() {
  return Shell::GetPrimaryRootWindowController()
      ->GetStatusAreaWidget()
      ->notification_center_tray();
}

views::View* GetOngoingProcessListView() {
  auto* notification_center_bubble = GetNotificationCenterTray()->bubble();
  CHECK(notification_center_bubble);
  return notification_center_bubble->GetNotificationCenterView()->GetViewByID(
      VIEW_ID_NOTIFICATION_BUBBLE_ONGOING_PROCESS_LIST);
}

views::View* GetNotificationListView() {
  auto* notification_center_bubble = GetNotificationCenterTray()->bubble();
  CHECK(notification_center_bubble);
  return notification_center_bubble->GetNotificationCenterView()->GetViewByID(
      VIEW_ID_NOTIFICATION_BUBBLE_NOTIFICATION_LIST);
}

}  // namespace

class NotificationCenterControllerTest : public AshTestBase {
 public:
  NotificationCenterControllerTest() : AshTestBase() {
    // `NotificationCenterController` is only used whenever Ongoing Processes
    // are enabled.
    scoped_feature_list_.InitWithFeatureState(features::kOngoingProcesses,
                                              true);
  }

  void SetUp() override {
    AshTestBase::SetUp();
    test_api_ = std::make_unique<NotificationCenterTestApi>();
  }

  NotificationCenterTestApi* test_api() { return test_api_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<NotificationCenterTestApi> test_api_;
};

// Tests that pinned system notifications are added to the ongoing process list.
// Any other type of notifications, including non-pinned or non-system (sourced
// from Web, ARC, etc.) will exist in the regular notification list.
TEST_F(NotificationCenterControllerTest, NotificationView) {
  NotificationCenterTray* notification_center_tray =
      GetNotificationCenterTray();
  notification_center_tray->ShowBubble();

  auto* ongoing_process_list = GetOngoingProcessListView();
  auto* notification_list = GetNotificationListView();
  EXPECT_EQ(ongoing_process_list->children().size(), 0u);
  EXPECT_EQ(notification_list->children().size(), 0u);

  // Add a system notification, verify it's added to the regular notification
  // list.
  test_api()->AddNotification();
  EXPECT_EQ(ongoing_process_list->children().size(), 0u);
  EXPECT_EQ(notification_list->children().size(), 1u);

  // Add a system pinned notification, verify it's added to the ongoing
  // process list.
  test_api()->AddPinnedNotification();
  EXPECT_EQ(ongoing_process_list->children().size(), 1u);
  EXPECT_EQ(notification_list->children().size(), 1u);

  // Add a web notification, verify it's added to the regular notification list.
  test_api()->AddNotificationWithSourceUrl("http://test-url-1.com");
  EXPECT_EQ(ongoing_process_list->children().size(), 1u);
  EXPECT_EQ(notification_list->children().size(), 2u);

  // Add a web pinned notification with a different URL, verify it's added to
  // the regular notification list, since it's not of type `SYSTEM_COMPONENT`.
  test_api()->AddPinnedNotificationWithSourceUrl("http://test-url-2.com");
  EXPECT_EQ(ongoing_process_list->children().size(), 1u);
  EXPECT_EQ(notification_list->children().size(), 3u);
}

}  // namespace ash
