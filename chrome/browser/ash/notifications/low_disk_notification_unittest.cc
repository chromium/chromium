// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/low_disk_notification.h"

#include <stdint.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/user_manager/fake_user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

// Copied from low_disk_notification.cc
const uint64_t kMediumNotification = (1 << 30) - 1;
const uint64_t kHighNotification = (512 << 20) - 1;

}  // namespace

namespace ash {

class LowDiskNotificationTest : public BrowserWithTestWindowTest {
 public:
  LowDiskNotificationTest() = default;
  ~LowDiskNotificationTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    UserDataAuthClient::InitializeFake();

    GetCrosSettingsHelper()->ReplaceDeviceSettingsProviderWithStub();
    GetCrosSettingsHelper()->SetBoolean(kDeviceShowLowDiskSpaceNotification,
                                        true);

    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    tester_ = std::make_unique<NotificationDisplayServiceTester>(
        nullptr /* profile */);
    tester_->SetNotificationAddedClosure(base::BindRepeating(
        &LowDiskNotificationTest::OnNotificationAdded, base::Unretained(this)));
    low_disk_notification_ = std::make_unique<LowDiskNotification>();
    notification_count_ = 0;

    medium_message_.set_disk_free_bytes(kMediumNotification);
    high_message_.set_disk_free_bytes(kHighNotification);
  }

  void TearDown() override {
    low_disk_notification_.reset();
    UserDataAuthClient::Shutdown();
    BrowserWithTestWindowTest::TearDown();
  }

  std::optional<message_center::Notification> GetNotification() {
    return tester_->GetNotification("low_disk");
  }

  void SetNotificationThrottlingInterval(int ms) {
    low_disk_notification_->SetNotificationIntervalForTest(
        base::Milliseconds(ms));
  }

  void OnNotificationAdded() { notification_count_++; }

 protected:
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
  std::unique_ptr<LowDiskNotification> low_disk_notification_;
  int notification_count_;

  // A LowDiskSpace protobuf message that contains `kMediumNotification`.
  ::user_data_auth::LowDiskSpace medium_message_;
  // A LowDiskSpace protobuf message that contains `kHighNotification`.
  ::user_data_auth::LowDiskSpace high_message_;
};

TEST_F(LowDiskNotificationTest, MediumLevelNotification) {
  std::u16string expected_title =
      l10n_util::GetStringUTF16(IDS_LOW_DISK_NOTIFICATION_TITLE);
  low_disk_notification_->LowDiskSpace(medium_message_);
  auto notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(expected_title, notification->title());
  EXPECT_EQ(1, notification_count_);
}

TEST_F(LowDiskNotificationTest, HighLevelReplacesMedium) {
  std::u16string expected_title =
      l10n_util::GetStringUTF16(IDS_CRITICALLY_LOW_DISK_NOTIFICATION_TITLE);
  low_disk_notification_->LowDiskSpace(medium_message_);
  low_disk_notification_->LowDiskSpace(high_message_);
  auto notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(expected_title, notification->title());
  EXPECT_EQ(2, notification_count_);
}

TEST_F(LowDiskNotificationTest, NotificationsAreThrottled) {
  SetNotificationThrottlingInterval(10000000);
  low_disk_notification_->LowDiskSpace(high_message_);
  low_disk_notification_->LowDiskSpace(high_message_);
  EXPECT_EQ(1, notification_count_);
}

TEST_F(LowDiskNotificationTest, HighNotificationsAreShownAfterThrottling) {
  SetNotificationThrottlingInterval(-1);
  low_disk_notification_->LowDiskSpace(high_message_);
  low_disk_notification_->LowDiskSpace(high_message_);
  EXPECT_EQ(2, notification_count_);
}

TEST_F(LowDiskNotificationTest, MediumNotificationsAreNotShownAfterThrottling) {
  SetNotificationThrottlingInterval(-1);
  low_disk_notification_->LowDiskSpace(medium_message_);
  low_disk_notification_->LowDiskSpace(medium_message_);
  EXPECT_EQ(1, notification_count_);
}

TEST_F(LowDiskNotificationTest, ShowForMultipleUsersWhenEnrolled) {
  user_manager()->AddUser(
      AccountId::FromUserEmailGaiaId("test_user1@example.com", "1234567891"));
  user_manager()->AddUser(
      AccountId::FromUserEmailGaiaId("test_user2@example.com", "1234567892"));

  SetNotificationThrottlingInterval(-1);
  low_disk_notification_->LowDiskSpace(high_message_);
  EXPECT_EQ(1, notification_count_);
}

TEST_F(LowDiskNotificationTest, SupressedForMultipleUsersWhenEnrolled) {
  user_manager()->AddUser(
      AccountId::FromUserEmailGaiaId("test_user1@example.com", "1234567891"));
  user_manager()->AddUser(
      AccountId::FromUserEmailGaiaId("test_user2@example.com", "1234567892"));

  GetCrosSettingsHelper()->SetBoolean(kDeviceShowLowDiskSpaceNotification,
                                      false);

  SetNotificationThrottlingInterval(-1);
  low_disk_notification_->LowDiskSpace(high_message_);
  EXPECT_EQ(0, notification_count_);
}

}  // namespace ash
