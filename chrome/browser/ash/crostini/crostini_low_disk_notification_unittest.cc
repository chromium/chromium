// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_low_disk_notification.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace crostini {

class CrostiniLowDiskNotificationTest : public BrowserWithTestWindowTest {
 public:
  CrostiniLowDiskNotificationTest() = default;
  ~CrostiniLowDiskNotificationTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();

    GetCrosSettingsHelper()->ReplaceDeviceSettingsProviderWithStub();
    GetCrosSettingsHelper()->SetBoolean(
        ash::kDeviceShowLowDiskSpaceNotification, true);

    auto user_manager = std::make_unique<user_manager::FakeUserManager>();
    user_manager_ = user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    tester_ = std::make_unique<NotificationDisplayServiceTester>(
        nullptr /* profile */);
    tester_->SetNotificationAddedClosure(base::BindRepeating(
        &CrostiniLowDiskNotificationTest::OnNotificationAdded,
        base::Unretained(this)));
    low_disk_notification_ = std::make_unique<CrostiniLowDiskNotification>();
    notification_count_ = 0;
    medium_notification_.set_free_bytes(600ll * 1024 * 1024);
    medium_notification_.set_vm_name(kCrostiniDefaultVmName);
    high_notification.set_free_bytes(300ll * 1024 * 1024);
    high_notification.set_vm_name(kCrostiniDefaultVmName);
  }

  void TearDown() override {
    low_disk_notification_.reset();
    ash::SeneschalClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    BrowserWithTestWindowTest::TearDown();
  }

  absl::optional<message_center::Notification> GetNotification() {
    return tester_->GetNotification("crostini_low_disk");
  }

  void SetNotificationThrottlingInterval(int ms) {
    low_disk_notification_->SetNotificationIntervalForTest(
        base::Milliseconds(ms));
  }

  void OnNotificationAdded() { notification_count_++; }

 protected:
  user_manager::FakeUserManager* user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
  std::unique_ptr<CrostiniLowDiskNotification> low_disk_notification_;
  vm_tools::cicerone::LowDiskSpaceTriggeredSignal medium_notification_;
  vm_tools::cicerone::LowDiskSpaceTriggeredSignal high_notification;
  int notification_count_;
};

TEST_F(CrostiniLowDiskNotificationTest, MediumLevelNotification) {
  std::u16string expected_title =
      l10n_util::GetStringUTF16(IDS_CROSTINI_LOW_DISK_NOTIFICATION_TITLE);
  low_disk_notification_->OnLowDiskSpaceTriggered(medium_notification_);
  auto notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(expected_title, notification->title());
  EXPECT_EQ(1, notification_count_);
}

TEST_F(CrostiniLowDiskNotificationTest, IgnoreNonTermina) {
  vm_tools::cicerone::LowDiskSpaceTriggeredSignal notification;
  notification.set_vm_name("wrong");
  low_disk_notification_->OnLowDiskSpaceTriggered(notification);
  EXPECT_EQ(0, notification_count_);
}

TEST_F(CrostiniLowDiskNotificationTest, HighLevelReplacesMedium) {
  std::u16string expected_title = l10n_util::GetStringUTF16(
      IDS_CROSTINI_CRITICALLY_LOW_DISK_NOTIFICATION_TITLE);
  low_disk_notification_->OnLowDiskSpaceTriggered(medium_notification_);
  low_disk_notification_->OnLowDiskSpaceTriggered(high_notification);
  auto notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(expected_title, notification->title());
  EXPECT_EQ(2, notification_count_);
}

TEST_F(CrostiniLowDiskNotificationTest, NotificationsAreThrottled) {
  SetNotificationThrottlingInterval(10000000);
  low_disk_notification_->OnLowDiskSpaceTriggered(high_notification);
  low_disk_notification_->OnLowDiskSpaceTriggered(high_notification);
  EXPECT_EQ(1, notification_count_);
}

TEST_F(CrostiniLowDiskNotificationTest,
       HighNotificationsAreShownAfterThrottling) {
  SetNotificationThrottlingInterval(-1);
  low_disk_notification_->OnLowDiskSpaceTriggered(high_notification);
  low_disk_notification_->OnLowDiskSpaceTriggered(high_notification);
  EXPECT_EQ(2, notification_count_);
}

TEST_F(CrostiniLowDiskNotificationTest,
       MediumNotificationsAreNotShownAfterThrottling) {
  SetNotificationThrottlingInterval(-1);
  low_disk_notification_->OnLowDiskSpaceTriggered(medium_notification_);
  low_disk_notification_->OnLowDiskSpaceTriggered(medium_notification_);
  EXPECT_EQ(1, notification_count_);
}

TEST_F(CrostiniLowDiskNotificationTest, ShowForMultipleUsersWhenEnrolled) {
  user_manager_->AddUser(
      AccountId::FromUserEmailGaiaId("test_user1@example.com", "1234567891"));
  user_manager_->AddUser(
      AccountId::FromUserEmailGaiaId("test_user2@example.com", "1234567892"));

  SetNotificationThrottlingInterval(-1);
  low_disk_notification_->OnLowDiskSpaceTriggered(high_notification);
  EXPECT_EQ(1, notification_count_);
}

TEST_F(CrostiniLowDiskNotificationTest, SupressedForMultipleUsersWhenEnrolled) {
  user_manager_->AddUser(
      AccountId::FromUserEmailGaiaId("test_user1@example.com", "1234567891"));
  user_manager_->AddUser(
      AccountId::FromUserEmailGaiaId("test_user2@example.com", "1234567892"));

  GetCrosSettingsHelper()->SetBoolean(ash::kDeviceShowLowDiskSpaceNotification,
                                      false);

  SetNotificationThrottlingInterval(-1);
  low_disk_notification_->OnLowDiskSpaceTriggered(high_notification);
  EXPECT_EQ(0, notification_count_);
}

TEST_F(CrostiniLowDiskNotificationTest, NoNotificationWhenEnoughFreeSpace) {
  low_disk_notification_->ShowNotificationIfAppropriate(1024 * 1024 * 1024);
  EXPECT_EQ(0, notification_count_);
}

}  // namespace crostini
