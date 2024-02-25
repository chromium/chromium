// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager_impl.h"
#include "chrome/browser/notifications/profile_notification.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace message_center {

class NotificationUIManagerTest : public BrowserWithTestWindowTest {
 public:
  NotificationUIManagerTest() {}

 protected:
  void SetUp() override {
    MessageCenter::Initialize();

    BrowserWithTestWindowTest::SetUp();
    message_center_ = MessageCenter::Get();
    notification_manager()->ResetUiControllerForTest();
  }

  void TearDown() override {
    BrowserWithTestWindowTest::TearDown();
    MessageCenter::Shutdown();
  }

  NotificationUIManagerImpl* notification_manager() {
    return (NotificationUIManagerImpl*)
        g_browser_process->notification_ui_manager();
  }

  MessageCenter* message_center() { return message_center_; }

  const Notification GetANotification(const std::string& id) {
    return Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, id, std::u16string(),
        std::u16string(), ui::ImageModel(), std::u16string(),
        GURL("chrome-extension://adflkjsdflkdsfdsflkjdsflkdjfs"),
        NotifierId(NotifierType::APPLICATION, "adflkjsdflkdsfdsflkjdsflkdjfs"),
        message_center::RichNotificationData(),
        new message_center::NotificationDelegate());
  }

 private:
  raw_ptr<MessageCenter, DanglingUntriaged> message_center_;
};

TEST_F(NotificationUIManagerTest, SetupNotificationManager) {
  notification_manager()->Add(GetANotification("test"), profile());
}

TEST_F(NotificationUIManagerTest, AddNotificationOnShutdown) {
  EXPECT_TRUE(message_center()->NotificationCount() == 0);
  notification_manager()->Add(GetANotification("test"), profile());
  EXPECT_TRUE(message_center()->NotificationCount() == 1);

  // Verify the number of notifications does not increase when trying to add a
  // notifcation on shutdown.
  notification_manager()->StartShutdown();
  EXPECT_TRUE(message_center()->NotificationCount() == 0);
  notification_manager()->Add(GetANotification("test2"), profile());
  EXPECT_TRUE(message_center()->NotificationCount() == 0);

  base::RunLoop().RunUntilIdle();
}

TEST_F(NotificationUIManagerTest, UpdateNotification) {
  EXPECT_TRUE(message_center()->NotificationCount() == 0);
  notification_manager()->Add(GetANotification("test"), profile());
  EXPECT_TRUE(message_center()->NotificationCount() == 1);
  ASSERT_TRUE(
      notification_manager()->Update(GetANotification("test"), profile()));
  EXPECT_TRUE(message_center()->NotificationCount() == 1);

  base::RunLoop().RunUntilIdle();
}

// Regression test for crbug.com/767868
TEST_F(NotificationUIManagerTest, GetAllIdsReturnsOriginalId) {
  EXPECT_TRUE(message_center()->NotificationCount() == 0);
  notification_manager()->Add(GetANotification("test"), profile());
  std::set<std::string> ids = notification_manager()->GetAllIdsByProfile(
      ProfileNotification::GetProfileID(profile()));
  ASSERT_EQ(1u, ids.size());
  EXPECT_EQ(*ids.begin(), "test");
}

TEST_F(NotificationUIManagerTest, GetAllIdsByOriginReturnsOriginalId) {
  EXPECT_TRUE(message_center()->NotificationCount() == 0);
  notification_manager()->Add(GetANotification("test"), profile());
  std::set<std::string> ids =
      notification_manager()->GetAllIdsByProfileAndOrigin(
          ProfileNotification::GetProfileID(profile()),
          GURL("chrome-extension://adflkjsdflkdsfdsflkjdsflkdjfs"));
  ASSERT_EQ(1u, ids.size());
  EXPECT_EQ(*ids.begin(), "test");
}

}  // namespace message_center
