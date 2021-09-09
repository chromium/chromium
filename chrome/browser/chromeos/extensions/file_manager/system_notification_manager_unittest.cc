// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/system_notification_manager.h"

#include "ash/constants/ash_features.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/extensions/file_manager/device_event_router.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_platform_bridge_delegator.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/disks/disk.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {

namespace file_manager_private = extensions::api::file_manager_private;

// Struct to reference notification strings for testing.
struct TestNotificationStrings {
  std::u16string title;
  std::u16string message;
};

// Notification platform bridge implementation for testing.
class TestNotificationPlatformBridgeDelegator
    : public NotificationPlatformBridgeDelegator {
 public:
  explicit TestNotificationPlatformBridgeDelegator(Profile* profile)
      : NotificationPlatformBridgeDelegator(profile, base::DoNothing()) {}
  ~TestNotificationPlatformBridgeDelegator() override = default;

  // NotificationPlatformBridgeDelegator:
  void Display(
      NotificationHandler::Type notification_type,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata) override {
    TestNotificationStrings strings;
    notification_ids_.insert(notification.id());
    strings.title = notification.title();
    strings.message = notification.message();
    notifications_[notification.id()] = strings;
  }

  void Close(NotificationHandler::Type notification_type,
             const std::string& notification_id) override {
    notification_ids_.erase(notification_id);
    notifications_.erase(notification_id);
  }

  void GetDisplayed(GetDisplayedNotificationsCallback callback) const override {
    std::move(callback).Run(notification_ids_, /*supports_sync=*/true);
  }

  // Helper to get the strings that would have bee seen on the notification.
  TestNotificationStrings GetNotificationStringsById(
      const std::string& notification_id) {
    TestNotificationStrings result;
    auto strings = notifications_.find(notification_id);
    if (strings != notifications_.end()) {
      result = strings->second;
    }
    return result;
  }

 private:
  std::set<std::string> notification_ids_;
  // Used to map a notification id to its displayed title and message.
  std::map<std::string, TestNotificationStrings> notifications_;
};

// DeviceEventRouter implementation for testing.
class DeviceEventRouterImpl : public DeviceEventRouter {
 public:
  DeviceEventRouterImpl(SystemNotificationManager* notification_manager,
                        Profile* profile)
      : DeviceEventRouter(notification_manager) {}

  // DeviceEventRouter overrides.
  void OnDeviceEvent(file_manager_private::DeviceEventType type,
                     const std::string& device_path,
                     const std::string& device_label) override {
    file_manager_private::DeviceEvent event;
    event.type = type;
    event.device_path = device_path;
    event.device_label = device_label;

    system_notification_manager()->HandleDeviceEvent(event);
  }

  // DeviceEventRouter overrides.
  // Hard set to disabled for the ExternalStorageDisabled test to work.
  bool IsExternalStorageDisabled() override { return true; }
};

class SystemNotificationManagerTest : public ::testing::Test {
 public:
  SystemNotificationManagerTest() {}

  void SetUp() override {
    testing::Test::SetUp();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("testing_profile");
    notification_display_service_ =
        reinterpret_cast<NotificationDisplayServiceImpl*>(
            NotificationDisplayServiceFactory::GetForProfile(profile_));
    auto bridge =
        std::make_unique<TestNotificationPlatformBridgeDelegator>(profile_);
    notification_platform_bridge = bridge.get();
    notification_display_service_
        ->SetNotificationPlatformBridgeDelegatorForTesting(std::move(bridge));
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(chromeos::features::kFilesSWA);
    notification_manager_ =
        std::make_unique<SystemNotificationManager>(profile_);
    device_event_router_ = std::make_unique<DeviceEventRouterImpl>(
        notification_manager_.get(), profile_);
  }

  void TearDown() override {
    profile_manager_->DeleteAllTestingProfiles();
    profile_ = nullptr;
    profile_manager_.reset();
  }

  TestingProfile* GetProfile() { return profile_; }

  DeviceEventRouterImpl* GetDeviceEventRouter() {
    return device_event_router_.get();
  }

  void GetNotificationsCallback(std::set<std::string> displayed_notifications,
                                bool supports_synchronization) {
    notification_count = displayed_notifications.size();
  }

  // Creates a disk instance with |device_path| and |mount_path| for testing.
  std::unique_ptr<chromeos::disks::Disk> CreateTestDisk(
      const std::string& device_path,
      const std::string& mount_path,
      bool is_read_only_hardware,
      bool is_mounted) {
    return chromeos::disks::Disk::Builder()
        .SetDevicePath(device_path)
        .SetMountPath(mount_path)
        .SetStorageDevicePath(device_path)
        .SetIsReadOnlyHardware(is_read_only_hardware)
        .SetFileSystemType("vfat")
        .SetIsMounted(is_mounted)
        .Build();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  // Externally owned raw pointers:
  // profile_ is owned by TestingProfileManager.
  TestingProfile* profile_;
  // notification_display_service is owned by NotificationDisplayServiceFactory.
  NotificationDisplayServiceImpl* notification_display_service_;
  std::unique_ptr<SystemNotificationManager> notification_manager_;
  std::unique_ptr<DeviceEventRouterImpl> device_event_router_;

 public:
  size_t notification_count;
  // notification_platform_bridge is owned by NotificationDisplayService.
  TestNotificationPlatformBridgeDelegator* notification_platform_bridge;
  base::WeakPtrFactory<SystemNotificationManagerTest> weak_ptr_factory_{this};
};

constexpr char kDevicePath[] = "/device/test";
constexpr char kMountPath[] = "/mnt/media/sda1";
std::u16string kRemovableDeviceTitle = u"Removable device detected";

TEST_F(SystemNotificationManagerTest, ExternalStorageDisabled) {
  // Send a removable volume mounted event.
  GetDeviceEventRouter()->OnDeviceAdded(kDevicePath);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById("disabled");
  // Check: the expected strings match.
  std::u16string kExternalStorageDisabledMesssage =
      u"Sorry, your administrator has disabled external storage on your "
      u"account.";
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message, kExternalStorageDisabledMesssage);
}

constexpr char kDeviceLabel[] = "MyUSB";
std::u16string kFormatTitle = u"Format MyUSB";

TEST_F(SystemNotificationManagerTest, FormatStart) {
  GetDeviceEventRouter()->OnFormatStarted(kDevicePath, kDeviceLabel,
                                          /*success=*/true);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById("format_start");
  // Check: the expected strings match.
  std::u16string kFormatStartMesssage = u"Formatting MyUSB\x2026";
  EXPECT_EQ(notification_strings.title, kFormatTitle);
  EXPECT_EQ(notification_strings.message, kFormatStartMesssage);
}

TEST_F(SystemNotificationManagerTest, FormatSuccess) {
  GetDeviceEventRouter()->OnFormatCompleted(kDevicePath, kDeviceLabel,
                                            /*success=*/true);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          "format_success");
  // Check: the expected strings match.
  std::u16string kFormatSuccessMesssage = u"Formatted MyUSB";
  EXPECT_EQ(notification_strings.title, kFormatTitle);
  EXPECT_EQ(notification_strings.message, kFormatSuccessMesssage);
}

TEST_F(SystemNotificationManagerTest, FormatFail) {
  GetDeviceEventRouter()->OnFormatCompleted(kDevicePath, kDeviceLabel,
                                            /*success=*/false);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById("format_fail");
  // Check: the expected strings match.
  std::u16string kFormatFailedMesssage = u"Could not format MyUSB";
  EXPECT_EQ(notification_strings.title, kFormatTitle);
  EXPECT_EQ(notification_strings.message, kFormatFailedMesssage);
}

constexpr char kPartitionLabel[] = "OEM";
std::u16string kPartitionTitle = u"Format OEM";

TEST_F(SystemNotificationManagerTest, PartitionFail) {
  GetDeviceEventRouter()->OnPartitionCompleted(kDevicePath, kPartitionLabel,
                                               /*success=*/false);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          "partition_fail");
  // Check: the expected strings match.
  std::u16string kPartitionFailMesssage = u"Could not format OEM";
  EXPECT_EQ(notification_strings.title, kPartitionTitle);
  EXPECT_EQ(notification_strings.message, kPartitionFailMesssage);
}

TEST_F(SystemNotificationManagerTest, RenameFail) {
  GetDeviceEventRouter()->OnRenameCompleted(kDevicePath, kPartitionLabel,
                                            /*success=*/false);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById("rename_fail");
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, u"Renaming failed");
  EXPECT_EQ(notification_strings.message,
            u"Aw, Snap! There was an error during renaming.");
}

TEST_F(SystemNotificationManagerTest, DeviceHardUnplugged) {
  std::unique_ptr<chromeos::disks::Disk> disk =
      CreateTestDisk(kDevicePath, kMountPath, /*is_read_only_hardware=*/false,
                     /*is_mounted=*/true);
  GetDeviceEventRouter()->OnDiskRemoved(*disk);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          "hard_unplugged");
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, u"Whoa, there. Be careful.");
  EXPECT_EQ(notification_strings.message,
            u"In the future, be sure to eject your removable device in the "
            u"Files app before unplugging it. Otherwise, you might lose data.");
}

TEST_F(SystemNotificationManagerTest, TestCopyEvents) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kFilesSWA);
  SystemNotificationManager notification_manager(nullptr);
  file_manager_private::CopyOrMoveProgressStatus status;

  // Check: an uninitialized status.source_url doesn't crash copy event handler.
  notification_manager.HandleCopyEvent(0, status);
}

}  // namespace file_manager
