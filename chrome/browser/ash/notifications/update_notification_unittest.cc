// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/update_notification.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

class UpdateNotificationTest : public BrowserWithTestWindowTest,
                               public testing::WithParamInterface<bool> {
 public:
  UpdateNotificationTest() : BrowserWithTestWindowTest(Browser::TYPE_NORMAL) {
    if (IsUpdateNotificationEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kFeatureManagementUpdateNotification);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kFeatureManagementUpdateNotification);
    }
  }
  ~UpdateNotificationTest() override = default;

  bool IsUpdateNotificationEnabled() const { return GetParam(); }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    display_service_ = static_cast<StubNotificationDisplayService*>(
        NotificationDisplayServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                ProfileManager::GetPrimaryUserProfile(),
                base::BindRepeating(
                    &StubNotificationDisplayService::FactoryForTests)));
  }

  absl::optional<message_center::Notification> GetNotification() {
    UserSessionManager::GetInstance()->MaybeShowUpdateNotification();
    return display_service_->GetNotification("chrome://update_notification");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<StubNotificationDisplayService, ExperimentalAsh> display_service_;
};

INSTANTIATE_TEST_SUITE_P(UpdateNotification,
                         UpdateNotificationTest,
                         testing::Bool());

TEST_P(UpdateNotificationTest, ShowNotification) {
  absl::optional<message_center::Notification> notification = GetNotification();

  // Should not show the update notification if the flag is not enabled.
  if (!IsUpdateNotificationEnabled()) {
    ASSERT_FALSE(notification);
    return;
  }

  // Show the update notification if the flag is enabled.
  ASSERT_TRUE(notification);
  EXPECT_EQ(u"Your Chromebook is updated.", notification->message());
}

}  // namespace ash
