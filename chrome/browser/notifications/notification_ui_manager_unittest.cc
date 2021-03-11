// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager_impl.h"
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_delegate.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace message_center {

class NotificationUIManagerTest : public BrowserWithTestWindowTest {
 public:
  NotificationUIManagerTest() {}

 protected:
  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    auto init_params = crosapi::mojom::BrowserInitParams::New();
    lacros_chrome_service_ =
        std::make_unique<chromeos::LacrosChromeServiceImpl>(
            /*delegate=*/nullptr);
    lacros_chrome_service_->SetInitParamsForTests(std::move(init_params));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    MessageCenter::Initialize();

    BrowserWithTestWindowTest::SetUp();
    message_center_ = MessageCenter::Get();
    notification_manager()->ResetUiControllerForTest();
  }

  void TearDown() override {
    BrowserWithTestWindowTest::TearDown();
    MessageCenter::Shutdown();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    lacros_chrome_service_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  NotificationUIManagerImpl* notification_manager() {
    return (NotificationUIManagerImpl*)
        g_browser_process->notification_ui_manager();
  }

  MessageCenter* message_center() { return message_center_; }

  const Notification GetANotification(const std::string& id) {
    return Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, id, std::u16string(),
        std::u16string(), gfx::Image(), std::u16string(),
        GURL("chrome-extension://adflkjsdflkdsfdsflkjdsflkdjfs"),
        NotifierId(NotifierType::APPLICATION, "adflkjsdflkdsfdsflkjdsflkdjfs"),
        message_center::RichNotificationData(),
        new message_center::NotificationDelegate());
  }

 private:
  MessageCenter* message_center_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // chromeos::LacrosChromeServiceImpl is used by Lacros implementation of
  // CheckIdleStateIsLocked(). Once Lacros transitions away from using
  // NotificationUIManagerImpl (like ChromeOS) then this file can be excluded
  // and Lacros-specific code can be removed.
  std::unique_ptr<chromeos::LacrosChromeServiceImpl> lacros_chrome_service_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

TEST_F(NotificationUIManagerTest, SetupNotificationManager) {
  TestingProfile profile;
  notification_manager()->Add(GetANotification("test"), &profile);
}

TEST_F(NotificationUIManagerTest, AddNotificationOnShutdown) {
  TestingProfile profile;
  EXPECT_TRUE(message_center()->NotificationCount() == 0);
  notification_manager()->Add(GetANotification("test"), &profile);
  EXPECT_TRUE(message_center()->NotificationCount() == 1);

  // Verify the number of notifications does not increase when trying to add a
  // notifcation on shutdown.
  notification_manager()->StartShutdown();
  EXPECT_TRUE(message_center()->NotificationCount() == 0);
  notification_manager()->Add(GetANotification("test2"), &profile);
  EXPECT_TRUE(message_center()->NotificationCount() == 0);
}

TEST_F(NotificationUIManagerTest, UpdateNotification) {
  TestingProfile profile;
  EXPECT_TRUE(message_center()->NotificationCount() == 0);
  notification_manager()->Add(GetANotification("test"), &profile);
  EXPECT_TRUE(message_center()->NotificationCount() == 1);
  ASSERT_TRUE(
      notification_manager()->Update(GetANotification("test"), &profile));
  EXPECT_TRUE(message_center()->NotificationCount() == 1);
}

// Regression test for crbug.com/767868
TEST_F(NotificationUIManagerTest, GetAllIdsReturnsOriginalId) {
  TestingProfile profile;
  EXPECT_TRUE(message_center()->NotificationCount() == 0);
  notification_manager()->Add(GetANotification("test"), &profile);
  std::set<std::string> ids = notification_manager()->GetAllIdsByProfile(
      NotificationUIManager::GetProfileID(&profile));
  ASSERT_EQ(1u, ids.size());
  EXPECT_EQ(*ids.begin(), "test");
}

}  // namespace message_center
