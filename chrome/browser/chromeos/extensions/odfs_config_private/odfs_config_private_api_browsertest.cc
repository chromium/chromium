// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/odfs_config_private/odfs_config_private_api.h"

#include <memory>
#include <set>

#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/message_center/public/cpp/notification.h"

namespace extensions {

class OfdsConfigPrivateApiBrowserTest : public ExtensionApiTest {
 public:
  OfdsConfigPrivateApiBrowserTest() = default;
  OfdsConfigPrivateApiBrowserTest(const OfdsConfigPrivateApiBrowserTest&) =
      delete;
  OfdsConfigPrivateApiBrowserTest& operator=(
      const OfdsConfigPrivateApiBrowserTest&) = delete;
  ~OfdsConfigPrivateApiBrowserTest() override = default;

 protected:
  auto GetAllNotifications() {
    base::test::TestFuture<std::set<std::string>, bool> get_displayed_future;
    NotificationDisplayServiceFactory::GetForProfile(profile())->GetDisplayed(
        get_displayed_future.GetCallback());
    const auto& notification_ids = get_displayed_future.Get<0>();
    EXPECT_TRUE(get_displayed_future.Wait());
    return notification_ids;
  }

  void ClearAllNotifications() {
    NotificationDisplayService* service =
        NotificationDisplayServiceFactory::GetForProfile(profile());
    for (const std::string& notification_id : GetAllNotifications()) {
      service->Close(NotificationHandler::Type::TRANSIENT, notification_id);
    }
  }

  size_t GetDisplayedNotificationsCount() {
    return GetAllNotifications().size();
  }

  void WaitUntilDisplayNotificationCount(size_t display_count) {
    ASSERT_TRUE(base::test::RunUntil([&]() -> bool {
      return GetDisplayedNotificationsCount() == display_count;
    }));
  }
};

IN_PROC_BROWSER_TEST_F(OfdsConfigPrivateApiBrowserTest,
                       ShowAutomatedMountErrorNotificationIsShown) {
  auto function = base::MakeRefCounted<
      extensions::OdfsConfigPrivateShowAutomatedMountErrorFunction>();
  api_test_utils::RunFunction(function.get(), /*args=*/"[]", profile());

  WaitUntilDisplayNotificationCount(/*display_count=*/1u);
  auto notifications = GetAllNotifications();

  ASSERT_EQ(1u, notifications.size());
  EXPECT_THAT(*notifications.begin(),
              testing::HasSubstr("automated_mount_error_notification_id"));
}

IN_PROC_BROWSER_TEST_F(OfdsConfigPrivateApiBrowserTest,
                       ShowAutomatedMountErrorNotificationCalledTwice) {
  auto function_first_call = base::MakeRefCounted<
      extensions::OdfsConfigPrivateShowAutomatedMountErrorFunction>();
  api_test_utils::RunFunction(function_first_call.get(), /*args=*/"[]",
                              profile());

  WaitUntilDisplayNotificationCount(/*display_count=*/1u);
  auto notifications = GetAllNotifications();

  ASSERT_EQ(1u, notifications.size());
  EXPECT_THAT(*notifications.begin(),
              testing::HasSubstr("automated_mount_error_notification_id"));

  auto function_second_call = base::MakeRefCounted<
      extensions::OdfsConfigPrivateShowAutomatedMountErrorFunction>();
  api_test_utils::RunFunction(function_second_call.get(), /*args=*/"[]",
                              profile());

  WaitUntilDisplayNotificationCount(/*display_count=*/1u);
  auto second_notifications = GetAllNotifications();

  ASSERT_EQ(1u, second_notifications.size());
  EXPECT_THAT(*second_notifications.begin(),
              testing::HasSubstr("automated_mount_error_notification_id"));
}

}  // namespace extensions
