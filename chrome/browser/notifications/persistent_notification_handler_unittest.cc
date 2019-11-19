// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/persistent_notification_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/notifications/metrics/mock_notification_metrics_logger.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/persistent_notification_status.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_permission_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

using ::testing::_;
using ::testing::Return;

namespace {

const char kExampleNotificationId[] = "example_notification_id";
const char kExampleOrigin[] = "https://example.com";

class TestingProfileWithPermissionManager : public TestingProfile {
 public:
  TestingProfileWithPermissionManager()
      : permission_manager_(
            std::make_unique<
                testing::NiceMock<content::MockPermissionManager>>()) {}

  ~TestingProfileWithPermissionManager() override = default;

  // Sets the notification permission status to |permission_status|.
  void SetNotificationPermissionStatus(
      blink::mojom::PermissionStatus permission_status) {
    ON_CALL(*permission_manager_,
            GetPermissionStatus(content::PermissionType::NOTIFICATIONS, _, _))
        .WillByDefault(Return(permission_status));
  }

  // TestingProfile overrides:
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override {
    return permission_manager_.get();
  }

 private:
  std::unique_ptr<content::MockPermissionManager> permission_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestingProfileWithPermissionManager);
};

}  // namespace

class PersistentNotificationHandlerTest : public ::testing::Test {
 public:
  PersistentNotificationHandlerTest()
      : display_service_tester_(&profile_), origin_(kExampleOrigin) {}

  ~PersistentNotificationHandlerTest() override = default;

  // ::testing::Test overrides:
  void SetUp() override {
    mock_logger_ = static_cast<MockNotificationMetricsLogger*>(
        NotificationMetricsLoggerFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                &profile_,
                base::BindRepeating(
                    &MockNotificationMetricsLogger::FactoryForTests)));

    PlatformNotificationServiceFactory::GetForProfile(&profile_)
        ->ClearClosedNotificationsForTesting();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileWithPermissionManager profile_;
  NotificationDisplayServiceTester display_service_tester_;

  // The origin for which these tests are being run.
  GURL origin_;

  // Owned by the |profile_| as a keyed service.
  MockNotificationMetricsLogger* mock_logger_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(PersistentNotificationHandlerTest);
};

TEST_F(PersistentNotificationHandlerTest, OnClick_WithoutPermission) {
  EXPECT_CALL(*mock_logger_, LogPersistentNotificationClickWithoutPermission());
  profile_.SetNotificationPermissionStatus(
      blink::mojom::PermissionStatus::DENIED);

  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();

  handler->OnClick(&profile_, origin_, kExampleNotificationId,
                   base::nullopt /* action_index */, base::nullopt /* reply */,
                   base::DoNothing());
}

TEST_F(PersistentNotificationHandlerTest,
       OnClick_CloseUnactionableNotifications) {
  // Show a notification for a particular origin.
  {
    base::RunLoop run_loop;
    display_service_tester_.SetNotificationAddedClosure(run_loop.QuitClosure());

    EXPECT_CALL(*mock_logger_, LogPersistentNotificationShown());

    PlatformNotificationServiceFactory::GetForProfile(&profile_)
        ->DisplayPersistentNotification(
            kExampleNotificationId, origin_ /* service_worker_scope */, origin_,
            blink::PlatformNotificationData(), blink::NotificationResources());

    run_loop.Run();
  }

  ASSERT_TRUE(display_service_tester_.GetNotification(kExampleNotificationId));

  // Revoke permission for any origin to display notifications.
  profile_.SetNotificationPermissionStatus(
      blink::mojom::PermissionStatus::DENIED);

  // Now simulate a click on the notification. It should be automatically closed
  // by the PersistentNotificationHandler.
  {
    EXPECT_CALL(*mock_logger_,
                LogPersistentNotificationClickWithoutPermission());

    display_service_tester_.SimulateClick(
        NotificationHandler::Type::WEB_PERSISTENT, kExampleNotificationId,
        base::nullopt /* action_index */, base::nullopt /* reply */);
  }

  EXPECT_FALSE(display_service_tester_.GetNotification(kExampleNotificationId));
}

TEST_F(PersistentNotificationHandlerTest, OnClose_ByUser) {
  EXPECT_CALL(*mock_logger_, LogPersistentNotificationClosedByUser());

  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();

  handler->OnClose(&profile_, origin_, kExampleNotificationId,
                   /* by_user= */ true, base::DoNothing());
}

TEST_F(PersistentNotificationHandlerTest, OnClose_Programmatically) {
  EXPECT_CALL(*mock_logger_, LogPersistentNotificationClosedProgrammatically());

  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();

  handler->OnClose(&profile_, origin_, kExampleNotificationId,
                   /* by_user= */ false, base::DoNothing());
}

TEST_F(PersistentNotificationHandlerTest, DisableNotifications) {
  std::unique_ptr<NotificationPermissionContext> permission_context =
      std::make_unique<NotificationPermissionContext>(&profile_);

  ASSERT_EQ(permission_context
                ->GetPermissionStatus(nullptr /* render_frame_host */, origin_,
                                      origin_)
                .content_setting,
            CONTENT_SETTING_ASK);

  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();
  handler->DisableNotifications(&profile_, origin_);

  ASSERT_EQ(permission_context
                ->GetPermissionStatus(nullptr /* render_frame_host */, origin_,
                                      origin_)
                .content_setting,
            CONTENT_SETTING_BLOCK);
}
