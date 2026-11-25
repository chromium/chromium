// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_delegator.h"

#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/notifications/notification_ui_manager_impl.h"
#include "chrome/browser/notifications/stub_notification_platform_bridge.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace {

// A system bridge that fails to initialize and becomes available later.
class LateNotificationPlatformBridge : public StubNotificationPlatformBridge {
 public:
  void GetDisplayed(Profile* profile,
                    GetDisplayedNotificationsCallback callback) const override {
    ++get_displayed_calls_;
  }

  void SetReadyCallback(NotificationBridgeReadyCallback callback) override {
    std::move(callback).Run(false);
  }

  base::CallbackListSubscription AddAvailableCallback(
      base::RepeatingClosure callback) override {
    return available_callbacks_.Add(std::move(callback));
  }

  void MakeAvailable() { available_callbacks_.Notify(); }

  int get_displayed_calls() const { return get_displayed_calls_; }

 private:
  mutable int get_displayed_calls_ = 0;
  base::RepeatingClosureList available_callbacks_;
};

}  // namespace

TEST(NotificationPlatformBridgeDelegatorTest, UsesSystemBridgeOnceAvailable) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  auto owned_bridge = std::make_unique<LateNotificationPlatformBridge>();
  LateNotificationPlatformBridge* bridge = owned_bridge.get();
  TestingBrowserProcess::GetGlobal()->SetNotificationPlatformBridge(
      std::move(owned_bridge));

  {
    NotificationPlatformBridgeDelegator delegator(&profile, base::DoNothing());
    bridge->MakeAvailable();
    delegator.GetDisplayed(base::DoNothing());
    EXPECT_EQ(1, bridge->get_displayed_calls());
  }

  TestingBrowserProcess::GetGlobal()->SetNotificationPlatformBridge(nullptr);
}

// Notifications that the message center shows while the system bridge is
// unavailable are closed when it becomes available, except TRANSIENT ones,
// which stay in the message center.
TEST(NotificationPlatformBridgeDelegatorTest,
     ClosesMessageCenterNotificationsOnSwitch) {
  content::BrowserTaskEnvironment task_environment;
  message_center::MessageCenter::Initialize();
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());
  Profile* profile = profile_manager.CreateTestingProfile("test-profile");
  auto ui_manager = std::make_unique<NotificationUIManagerImpl>();
  ui_manager->ResetUiControllerForTest();
  TestingBrowserProcess::GetGlobal()->SetNotificationUIManager(
      std::move(ui_manager));
  auto owned_bridge = std::make_unique<LateNotificationPlatformBridge>();
  LateNotificationPlatformBridge* bridge = owned_bridge.get();
  TestingBrowserProcess::GetGlobal()->SetNotificationPlatformBridge(
      std::move(owned_bridge));

  auto make_notification = [](const std::string& id) {
    return message_center::Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, id, u"title", u"message",
        ui::ImageModel(), u"source", GURL("https://example.com"),
        message_center::NotifierId(GURL("https://example.com")),
        message_center::RichNotificationData(),
        base::MakeRefCounted<message_center::NotificationDelegate>());
  };
  {
    NotificationPlatformBridgeDelegator delegator(profile, base::DoNothing());
    delegator.Display(NotificationHandler::Type::WEB_PERSISTENT,
                      make_notification("web"), nullptr);
    delegator.Display(NotificationHandler::Type::TRANSIENT,
                      make_notification("transient"), nullptr);
    auto* message_center = message_center::MessageCenter::Get();
    EXPECT_EQ(2u, message_center->NotificationCount());

    bridge->MakeAvailable();
    EXPECT_EQ(1u, message_center->NotificationCount());
  }

  TestingBrowserProcess::GetGlobal()->SetNotificationPlatformBridge(nullptr);
  TestingBrowserProcess::GetGlobal()->SetNotificationUIManager(nullptr);
  message_center::MessageCenter::Shutdown();
  task_environment.RunUntilIdle();
}
