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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/message_center.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/notification.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
#if BUILDFLAG(IS_CHROMEOS_ASH)
    base::test::TestFuture<std::set<std::string>, bool> get_displayed_future;
    NotificationDisplayServiceFactory::GetForProfile(profile())->GetDisplayed(
        get_displayed_future.GetCallback());
#else
    base::test::TestFuture<const std::vector<std::string>&>
        get_displayed_future;
    auto& remote = chromeos::LacrosService::Get()
                       ->GetRemote<crosapi::mojom::MessageCenter>();
    EXPECT_TRUE(remote.get());
    remote->GetDisplayedNotifications(get_displayed_future.GetCallback());
#endif
    const auto& notification_ids = get_displayed_future.Get<0>();
    EXPECT_TRUE(get_displayed_future.Wait());
    return notification_ids;
  }

  void ClearAllNotifications() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    NotificationDisplayService* service =
        NotificationDisplayServiceFactory::GetForProfile(profile());
#else
    base::test::TestFuture<const std::vector<std::string>&>
        get_displayed_future;
    auto& service = chromeos::LacrosService::Get()
                        ->GetRemote<crosapi::mojom::MessageCenter>();
    EXPECT_TRUE(service.get());
#endif
    for (const std::string& notification_id : GetAllNotifications()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      service->Close(NotificationHandler::Type::TRANSIENT, notification_id);
#else
      service->CloseNotification(notification_id);
#endif
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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If ash does not contain the relevant test controller functionality,
  // then there's nothing to do for this test.
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service
           ->IsRegistered<crosapi::mojom::OneDriveNotificationService>() ||
      !lacros_service
           ->IsAvailable<crosapi::mojom::OneDriveNotificationService>()) {
    GTEST_SKIP()
        << "Unsupported ash version for the one drive notification service";
  }

  ClearAllNotifications();
  WaitUntilDisplayNotificationCount(/*display_count=*/0u);
  const absl::Cleanup policy_cleanup = [this]() { ClearAllNotifications(); };
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If ash does not contain the relevant test controller functionality,
  // then there's nothing to do for this test.
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service
           ->IsRegistered<crosapi::mojom::OneDriveNotificationService>() ||
      !lacros_service
           ->IsAvailable<crosapi::mojom::OneDriveNotificationService>()) {
    GTEST_SKIP()
        << "Unsupported ash version for the one drive notification service";
  }

  ClearAllNotifications();
  WaitUntilDisplayNotificationCount(/*display_count=*/0u);
  const absl::Cleanup policy_cleanup = [this]() { ClearAllNotifications(); };
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(OfdsConfigPrivateApiBrowserTest, UnsupportedAshVersion) {
  // If ash does not contain the relevant test controller functionality,
  // then there's nothing to do for this test.
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service
          ->IsRegistered<crosapi::mojom::OneDriveNotificationService>() &&
      lacros_service
          ->IsAvailable<crosapi::mojom::OneDriveNotificationService>()) {
    GTEST_SKIP()
        << "Supported ash version for the one drive notification service";
  }

  ClearAllNotifications();
  WaitUntilDisplayNotificationCount(/*display_count=*/0u);

  auto function = base::MakeRefCounted<
      extensions::OdfsConfigPrivateShowAutomatedMountErrorFunction>();
  EXPECT_EQ("Cannot show notification because ash version is not supported",
            api_test_utils::RunFunctionAndReturnError(
                function.get(), /*args=*/"[]", profile()));
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace extensions
