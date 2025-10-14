// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/low_disk_notification.h"

#include <stdint.h>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/user_manager/fake_user_manager.h"
#include "google_apis/gaia/gaia_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/test/message_center_waiter.h"

namespace {

constexpr const char kLowDiskNotificationId[] = "low_disk";

// Copied from low_disk_notification.cc
const uint64_t kMediumNotification = (1 << 30) - 1;
const uint64_t kHighNotification = (512 << 20) - 1;

}  // namespace

namespace ash {

class LowDiskNotificationTest : public BrowserWithTestWindowTest {
 public:
  LowDiskNotificationTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~LowDiskNotificationTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    UserDataAuthClient::InitializeFake();

    GetCrosSettingsHelper()->ReplaceDeviceSettingsProviderWithStub();
    GetCrosSettingsHelper()->SetBoolean(kDeviceShowLowDiskSpaceNotification,
                                        true);

    low_disk_notification_ = std::make_unique<LowDiskNotification>();

    medium_message_.set_disk_free_bytes(kMediumNotification);
    high_message_.set_disk_free_bytes(kHighNotification);
  }

  void TearDown() override {
    low_disk_notification_.reset();
    UserDataAuthClient::Shutdown();
    BrowserWithTestWindowTest::TearDown();
  }

  const message_center::Notification* GetNotification() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        kLowDiskNotificationId);
  }

  void SetNotificationThrottlingInterval(int ms) {
    low_disk_notification_->SetNotificationIntervalForTest(
        base::Milliseconds(ms));
  }

 protected:
  std::unique_ptr<LowDiskNotification> low_disk_notification_;

  // A LowDiskSpace protobuf message that contains `kMediumNotification`.
  ::user_data_auth::LowDiskSpace medium_message_;
  // A LowDiskSpace protobuf message that contains `kHighNotification`.
  ::user_data_auth::LowDiskSpace high_message_;
};

TEST_F(LowDiskNotificationTest, MediumLevelNotification) {
  std::u16string expected_title =
      l10n_util::GetStringUTF16(IDS_LOW_DISK_NOTIFICATION_TITLE);
  message_center::MessageCenterWaiter waiter(kLowDiskNotificationId);
  low_disk_notification_->LowDiskSpace(medium_message_);
  waiter.WaitUntilAdded();
  const auto* notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(expected_title, notification->title());
}

TEST_F(LowDiskNotificationTest, HighLevelReplacesMedium) {
  std::u16string expected_title =
      l10n_util::GetStringUTF16(IDS_CRITICALLY_LOW_DISK_NOTIFICATION_TITLE);

  // Show medium notification.
  {
    message_center::MessageCenterWaiter waiter(kLowDiskNotificationId);
    low_disk_notification_->LowDiskSpace(medium_message_);
    waiter.WaitUntilAdded();
  }
  const auto* medium_notification = GetNotification();
  ASSERT_TRUE(medium_notification);
  const base::Time medium_timestamp = medium_notification->timestamp();

  // Advance time and show high notification.
  task_environment()->FastForwardBy(base::Seconds(1));
  {
    message_center::MessageCenterWaiter waiter(kLowDiskNotificationId);
    low_disk_notification_->LowDiskSpace(high_message_);
    waiter.WaitUntilUpdated();
  }

  const auto* high_notification = GetNotification();
  ASSERT_TRUE(high_notification);
  EXPECT_EQ(high_notification->title(), expected_title);
  EXPECT_NE(high_notification->timestamp(), medium_timestamp);
}

TEST_F(LowDiskNotificationTest, NotificationsAreThrottled) {
  SetNotificationThrottlingInterval(10000000);

  // Show first notification.
  message_center::MessageCenterWaiter waiter(kLowDiskNotificationId);
  low_disk_notification_->LowDiskSpace(high_message_);
  waiter.WaitUntilAdded();
  const auto* notification = GetNotification();
  ASSERT_TRUE(notification);
  const base::Time original_timestamp = notification->timestamp();

  // Immediately trigger another, which should be throttled.
  low_disk_notification_->LowDiskSpace(high_message_);
  task_environment()->FastForwardBy(base::TimeDelta());

  // Verify the original notification is still there and unchanged.
  const auto* final_notification = GetNotification();
  ASSERT_TRUE(final_notification);
  EXPECT_EQ(final_notification->timestamp(), original_timestamp);
}

TEST_F(LowDiskNotificationTest, HighNotificationsAreShownAfterThrottling) {
  SetNotificationThrottlingInterval(-1);

  // Show first notification.
  {
    message_center::MessageCenterWaiter waiter(kLowDiskNotificationId);
    low_disk_notification_->LowDiskSpace(high_message_);
    waiter.WaitUntilAdded();
  }
  const auto* first_notification = GetNotification();
  ASSERT_TRUE(first_notification);
  const base::Time first_timestamp = first_notification->timestamp();

  // Advance time and show second notification.
  task_environment()->FastForwardBy(base::Seconds(1));
  {
    message_center::MessageCenterWaiter waiter(kLowDiskNotificationId);
    low_disk_notification_->LowDiskSpace(high_message_);
    waiter.WaitUntilUpdated();
  }
  const auto* second_notification = GetNotification();
  ASSERT_TRUE(second_notification);
  EXPECT_NE(second_notification->timestamp(), first_timestamp);
}

TEST_F(LowDiskNotificationTest, MediumNotificationsAreNotShownAfterThrottling) {
  SetNotificationThrottlingInterval(-1);

  // Show first notification.
  message_center::MessageCenterWaiter waiter(kLowDiskNotificationId);
  low_disk_notification_->LowDiskSpace(medium_message_);
  waiter.WaitUntilAdded();
  const auto* notification = GetNotification();
  ASSERT_TRUE(notification);
  const base::Time original_timestamp = notification->timestamp();

  // Immediately trigger another, which should be throttled.
  low_disk_notification_->LowDiskSpace(medium_message_);
  task_environment()->FastForwardBy(base::TimeDelta());

  // Verify the original notification is still there and unchanged.
  const auto* final_notification = GetNotification();
  ASSERT_TRUE(final_notification);
  EXPECT_EQ(final_notification->timestamp(), original_timestamp);
}

TEST_F(LowDiskNotificationTest, ShowForMultipleUsersWhenEnrolled) {
  user_manager()->AddGaiaUser(
      AccountId::FromUserEmailGaiaId("test_user1@example.com",
                                     GaiaId("1234567891")),
      user_manager::UserType::kRegular);
  user_manager()->AddGaiaUser(
      AccountId::FromUserEmailGaiaId("test_user2@example.com",
                                     GaiaId("1234567892")),
      user_manager::UserType::kRegular);

  SetNotificationThrottlingInterval(-1);
  message_center::MessageCenterWaiter waiter(kLowDiskNotificationId);
  low_disk_notification_->LowDiskSpace(high_message_);
  waiter.WaitUntilAdded();
  ASSERT_TRUE(GetNotification());
}

TEST_F(LowDiskNotificationTest, SupressedForMultipleUsersWhenEnrolled) {
  user_manager()->AddGaiaUser(
      AccountId::FromUserEmailGaiaId("test_user1@example.com",
                                     GaiaId("1234567891")),
      user_manager::UserType::kRegular);
  user_manager()->AddGaiaUser(
      AccountId::FromUserEmailGaiaId("test_user2@example.com",
                                     GaiaId("1234567892")),
      user_manager::UserType::kRegular);

  GetCrosSettingsHelper()->SetBoolean(kDeviceShowLowDiskSpaceNotification,
                                      false);

  SetNotificationThrottlingInterval(-1);
  low_disk_notification_->LowDiskSpace(high_message_);
  task_environment()->FastForwardBy(base::TimeDelta());
  ASSERT_FALSE(GetNotification());
}

TEST_F(LowDiskNotificationTest, DemoModeSkipNotification) {
  GetCrosSettingsHelper()->InstallAttributes()->SetDemoMode();
  SetNotificationThrottlingInterval(-1);
  low_disk_notification_->LowDiskSpace(high_message_);
  task_environment()->FastForwardBy(base::TimeDelta());
  ASSERT_FALSE(GetNotification());
}

}  // namespace ash
