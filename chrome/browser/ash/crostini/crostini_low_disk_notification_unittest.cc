// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_low_disk_notification.h"

#include <stdint.h>

#include <utility>

#include "ash/strings/grit/ash_strings.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "google_apis/gaia/gaia_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/public/cpp/notification.h"

namespace crostini {

class CrostiniLowDiskNotificationTest
    : public BrowserWithTestWindowTest,
      public message_center::MessageCenterObserver {
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

    message_center_observation_.Observe(message_center::MessageCenter::Get());
    low_disk_notification_ = std::make_unique<CrostiniLowDiskNotification>();
    notification_count_ = 0;
    medium_notification_.set_free_bytes(600ll * 1024 * 1024);
    medium_notification_.set_vm_name(kCrostiniDefaultVmName);
    high_notification.set_free_bytes(300ll * 1024 * 1024);
    high_notification.set_vm_name(kCrostiniDefaultVmName);
  }

  void TearDown() override {
    low_disk_notification_.reset();
    message_center_observation_.Reset();
    ash::SeneschalClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    BrowserWithTestWindowTest::TearDown();
  }

  const message_center::Notification* GetNotification() {
    return message_center::MessageCenter::Get()->FindNotificationById(
        "crostini_low_disk");
  }

  void SetNotificationThrottlingInterval(int ms) {
    low_disk_notification_->SetNotificationIntervalForTest(
        base::Milliseconds(ms));
  }

  void OnNotificationAdded(const std::string& notification_id) override {
    if (notification_id == "crostini_low_disk") {
      notification_count_++;
    }
  }

  void OnNotificationUpdated(const std::string& notification_id) override {
    OnNotificationAdded(notification_id);
  }

 protected:
  base::ScopedObservation<message_center::MessageCenter,
                          message_center::MessageCenterObserver>
      message_center_observation_{this};
  std::unique_ptr<CrostiniLowDiskNotification> low_disk_notification_;
  vm_tools::cicerone::LowDiskSpaceTriggeredSignal medium_notification_;
  vm_tools::cicerone::LowDiskSpaceTriggeredSignal high_notification;
  int notification_count_;
};

TEST_F(CrostiniLowDiskNotificationTest, MediumLevelNotification) {
  std::u16string expected_title =
      l10n_util::GetStringUTF16(IDS_CROSTINI_LOW_DISK_NOTIFICATION_TITLE);
  low_disk_notification_->OnLowDiskSpaceTriggered(medium_notification_);
  auto* notification = GetNotification();
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
  auto* notification = GetNotification();
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
  user_manager()->AddGaiaUser(
      AccountId::FromUserEmailGaiaId("test_user1@example.com",
                                     GaiaId("1234567891")),
      user_manager::UserType::kRegular);
  user_manager()->AddGaiaUser(
      AccountId::FromUserEmailGaiaId("test_user2@example.com",
                                     GaiaId("1234567892")),
      user_manager::UserType::kRegular);

  SetNotificationThrottlingInterval(-1);
  low_disk_notification_->OnLowDiskSpaceTriggered(high_notification);
  EXPECT_EQ(1, notification_count_);
}

TEST_F(CrostiniLowDiskNotificationTest, SupressedForMultipleUsersWhenEnrolled) {
  user_manager()->AddGaiaUser(
      AccountId::FromUserEmailGaiaId("test_user1@example.com",
                                     GaiaId("1234567891")),
      user_manager::UserType::kRegular);
  user_manager()->AddGaiaUser(
      AccountId::FromUserEmailGaiaId("test_user2@example.com",
                                     GaiaId("1234567892")),
      user_manager::UserType::kRegular);

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
