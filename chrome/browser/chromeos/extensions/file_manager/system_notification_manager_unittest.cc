// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/system_notification_manager.h"

#include "ash/constants/ash_features.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/extensions/file_manager/device_event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_platform_bridge_delegator.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/disks/disk.h"
#include "components/arc/arc_prefs.h"
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

  SystemNotificationManager* GetSystemNotificationManager() {
    return notification_manager_.get();
  }

  NotificationDisplayService* GetNotificationDisplayService() {
    return static_cast<NotificationDisplayService*>(
        notification_display_service_);
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

TEST_F(SystemNotificationManagerTest, DeviceNavigation) {
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, chromeos::DeviceType::DEVICE_TYPE_USB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_COMPLETED_STATUS_SUCCESS;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
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
          "swa-removable-device-id");
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Explore the device\x2019s content in the Files app.");
}

// Test for notification generated when enterprise read-only policy is set.
// Condition that triggers that is a mount event for a removable device
// and the volume has read only set.
TEST_F(SystemNotificationManagerTest, DeviceNavigationReadOnlyPolicy) {
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, chromeos::DeviceType::DEVICE_TYPE_USB,
      /*read_only=*/true, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_COMPLETED_STATUS_SUCCESS;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
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
          "swa-removable-device-id");
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Explore the device's content in the Files app. The content is "
            u"restricted by an admin and can\x2019t be modified.");
}

// Test for notification generated when ARC++ is enabled on the device.
// Condition that triggers that is a mount event for a removable device
// when the removable access for ARC++ is disabled.
TEST_F(SystemNotificationManagerTest, DeviceNavigationAllowAppAccess) {
  // Set the ARC++ enbled preference on the testing profile.
  PrefService* const service = GetProfile()->GetPrefs();
  service->SetBoolean(arc::prefs::kArcEnabled, true);
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, chromeos::DeviceType::DEVICE_TYPE_USB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_COMPLETED_STATUS_SUCCESS;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
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
          "swa-removable-device-id");
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Explore the device\x2019s content in the Files app. For device "
            u"preferences, go to Settings.");
}

// Test for notification generated when ARC++ is enabled on the device.
// Condition that triggers that is a mount event for a removable device
// when the removable access for ARC++ is enabled.
TEST_F(SystemNotificationManagerTest, DeviceNavigationAppsHaveAccess) {
  // Set the ARC++ enbled preference on the testing profile.
  PrefService* const service = GetProfile()->GetPrefs();
  service->SetBoolean(arc::prefs::kArcEnabled, true);
  service->SetBoolean(arc::prefs::kArcHasAccessToRemovableMedia, true);
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, chromeos::DeviceType::DEVICE_TYPE_USB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_COMPLETED_STATUS_SUCCESS;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
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
          "swa-removable-device-id");
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Explore the device\x2019s content in the Files app. Play Store "
            u"applications have access to this device.");
}

constexpr char kDeviceFailNotificationId[] = "swa-device-fail-id";

// Device unsupported notifications are generated when a removable
// drive is mounted with an unsupported filesystem on a partition.
// The default message is used when there is no device label.
// In the notification generation logic there is a distinction
// between parent and child volumes found by the volume is_parent()
// method. Both parent and child unknown volume filesystems generate
// the same nofication.
TEST_F(SystemNotificationManagerTest, DeviceUnsupportedDefault) {
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, chromeos::DeviceType::DEVICE_TYPE_USB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      "", "FAT32"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status =
      file_manager_private::MOUNT_COMPLETED_STATUS_ERROR_UNSUPPORTED_FILESYSTEM;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
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
          kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(
      notification_strings.message,
      u"Sorry, your external storage device is not supported at this time.");
}

// The named version of the device unsupported notification is
// generated when the device includes a device label.
TEST_F(SystemNotificationManagerTest, DeviceUnsupportedNamed) {
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, chromeos::DeviceType::DEVICE_TYPE_USB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status =
      file_manager_private::MOUNT_COMPLETED_STATUS_ERROR_UNSUPPORTED_FILESYSTEM;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
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
          kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Sorry, the device MyUSB is not supported at this time.");
}

// Multipart device unsupported notifications are generated when there is
// a multi-partition device inserted and one partition has a known filesystem
// but other partitions have unsupported file systems.
// Note: In such cases the Chrome OS device will generate 2 notifications:
//       1) A device navigation notification for the supported file system
//       2) The multipart device unsupported notification.
TEST_F(SystemNotificationManagerTest, MultipartDeviceUnsupportedDefault) {
  // Build a supported file system volume and mount it.
  std::unique_ptr<Volume> volume1(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, chromeos::DeviceType::DEVICE_TYPE_USB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      "", "FAT32"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_COMPLETED_STATUS_SUCCESS;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume1.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          "swa-removable-device-id");
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Explore the device\x2019s content in the Files app.");
  // Build an unsupported file system volume and mount it on the same device.
  std::unique_ptr<Volume> volume2(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path2")),
      VolumeType::VOLUME_TYPE_TESTING, chromeos::DeviceType::DEVICE_TYPE_USB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      "", "unsupported"));
  event.status =
      file_manager_private::MOUNT_COMPLETED_STATUS_ERROR_UNSUPPORTED_FILESYSTEM;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume2.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have two notifications.
  ASSERT_EQ(2, notification_count);
  // Get the strings for the displayed notification.
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Sorry, at least one partition on your external storage device "
            u"could not be mounted.");
}

// The named version of the multipart device unsupported notification is
// generated when the device label exists and at least one partition can be
// mounted on a device with an unsupported file system on another partition.
TEST_F(SystemNotificationManagerTest, MultipartDeviceUnsupportedNamed) {
  // Build a supported file system volume and mount it.
  std::unique_ptr<Volume> volume1(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, chromeos::DeviceType::DEVICE_TYPE_USB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_COMPLETED_STATUS_SUCCESS;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume1.get());
  // Ignore checking for the device navigation notification.
  // Build an unsupported file system volume and mount it on the same device.
  std::unique_ptr<Volume> volume2(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path2")),
      VolumeType::VOLUME_TYPE_TESTING, chromeos::DeviceType::DEVICE_TYPE_USB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "unsupported"));
  event.status =
      file_manager_private::MOUNT_COMPLETED_STATUS_ERROR_UNSUPPORTED_FILESYSTEM;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume2.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have two notifications.
  ASSERT_EQ(2, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Sorry, at least one partition on the device MyUSB could not be "
            u"mounted.");
}

// Device fail unknown notifications are generated when the type of filesystem
// on a removable device cannot be determined.
// The default version of the notifiication is generated when there is
// no drive label.
// These notifications are similar to the device unsupported notifications,
// the difference being an unknown vs. unsupported file system.
TEST_F(SystemNotificationManagerTest, DeviceFailUnknownDefault) {
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, chromeos::DeviceType::DEVICE_TYPE_USB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      "", "unknown"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status =
      file_manager_private::MOUNT_COMPLETED_STATUS_ERROR_UNKNOWN_FILESYSTEM;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
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
          kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Sorry, your external storage device could not be recognized.");
}

// The named version of the device fail unknown notification is
// generated when the device includes a device label.
TEST_F(SystemNotificationManagerTest, DeviceFailUnknownNamed) {
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, chromeos::DeviceType::DEVICE_TYPE_USB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "unknown"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status =
      file_manager_private::MOUNT_COMPLETED_STATUS_ERROR_UNKNOWN_FILESYSTEM;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
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
          kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Sorry, the device MyUSB could not be recognized.");
}

// Device fail unknown read only notifications are generated when
// volumes with read only set and an unknown file system is mounted.
// The default notification message is generated when there is
// no device label.
TEST_F(SystemNotificationManagerTest, DeviceFailUnknownReadOnlyDefault) {
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, chromeos::DeviceType::DEVICE_TYPE_USB,
      /*read_only=*/true, base::FilePath(FILE_PATH_LITERAL("/device/test")), "",
      "unknown"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status =
      file_manager_private::MOUNT_COMPLETED_STATUS_ERROR_UNKNOWN_FILESYSTEM;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
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
          kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Sorry, your external storage device could not be recognized.");
}

// The named version of the read only device fail unknown notification is
// generated when the device includes a device label.
TEST_F(SystemNotificationManagerTest, DeviceFailUnknownReadOnlyNamed) {
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, chromeos::DeviceType::DEVICE_TYPE_USB,
      /*read_only=*/true, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "unknown"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status =
      file_manager_private::MOUNT_COMPLETED_STATUS_ERROR_UNKNOWN_FILESYSTEM;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
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
          kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Sorry, the device MyUSB could not be recognized.");
}

TEST_F(SystemNotificationManagerTest, TestCopyEvents) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kFilesSWA);
  SystemNotificationManager notification_manager(nullptr);
  file_manager_private::CopyOrMoveProgressStatus status;

  // Check: an uninitialized status.source_url doesn't crash copy event handler.
  notification_manager.HandleCopyEvent(0, status);
}

TEST_F(SystemNotificationManagerTest, CopyProgress) {
  // Setup a copy operation status object.
  file_manager_private::CopyOrMoveProgressStatus status;
  int copy_id = 1;
  double copy_size = 100.0;
  std::string copy_file_dest_url =
      "filesystem:chrome://file-manager/external/Downloads-test-user/NewFolder/"
      "file.txt";
  status.destination_url = std::make_unique<std::string>(copy_file_dest_url);
  status.size = std::make_unique<double>(copy_size);
  std::string copy_file_src_url =
      "filesystem:chrome://file-manager/external/Downloads-test-user/file.txt";
  status.source_url = std::make_unique<std::string>(copy_file_src_url);
  status.type = file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_BEGIN;

  // Send the copy begin event.
  SystemNotificationManager* notification_manager =
      GetSystemNotificationManager();
  notification_manager->HandleCopyStart(copy_id, status);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayService* notification_display_service =
      GetNotificationDisplayService();
  notification_display_service->GetDisplayed(
      base::BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  // Check: We have zero notifications.
  ASSERT_EQ(0, notification_count);

  // Send progress event.
  status.type =
      file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_PROGRESS;
  copy_size = 50.0;
  notification_manager->HandleCopyEvent(copy_id, status);
  notification_display_service->GetDisplayed(
      base::BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  // Check: We have 1 notification.
  ASSERT_EQ(1, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          "swa-file-operation-1");
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, u"Files");
  EXPECT_EQ(notification_strings.message, u"Copying file.txt\x2026");

  // Send copy success event.
  status.type = file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_SUCCESS;
  notification_manager->HandleCopyEvent(copy_id, status);
  notification_display_service->GetDisplayed(
      base::BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  // Check: We have zero notifications (copy progress has been closed).
  ASSERT_EQ(0, notification_count);
  // Start another copy that ends in error.
  copy_id = 2;
  copy_size = 100.0;
  status.type = file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_BEGIN;

  // Send the copy begin event.
  notification_manager->HandleCopyStart(copy_id, status);

  // Send progress event.
  status.type =
      file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_PROGRESS;
  copy_size = 50.0;
  notification_manager->HandleCopyEvent(copy_id, status);
  notification_display_service->GetDisplayed(
      base::BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  // Check: We have 1 notification.
  ASSERT_EQ(1, notification_count);

  // Send copy error event.
  status.type = file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_ERROR;
  notification_manager->HandleCopyEvent(copy_id, status);
  notification_display_service->GetDisplayed(
      base::BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  // Check: We have zero notifications (copy progress has been closed).
  ASSERT_EQ(0, notification_count);
}

std::u16string kGoogleDrive = u"Google Drive";

// Tests all the various error notifications.
TEST_F(SystemNotificationManagerTest, Errors) {
  // Build a Drive sync error object.
  file_manager_private::DriveSyncErrorEvent sync_error;
  sync_error.type =
      file_manager_private::DRIVE_SYNC_ERROR_TYPE_DELETE_WITHOUT_PERMISSION;
  sync_error.file_url = "drivefs://fake.txt";
  std::unique_ptr<extensions::Event> event =
      std::make_unique<extensions::Event>(
          extensions::events::FILE_MANAGER_PRIVATE_ON_DRIVE_SYNC_ERROR,
          file_manager_private::OnDriveSyncError::kEventName,
          file_manager_private::OnDriveSyncError::Create(sync_error));

  // Send the delete without permission sync error event.
  SystemNotificationManager* notification_manager =
      GetSystemNotificationManager();
  notification_manager->HandleEvent(*event.get());
  NotificationDisplayService* notification_display_service =
      GetNotificationDisplayService();
  // Get the number of notifications from the NotificationDisplayService.
  notification_display_service->GetDisplayed(
      base::BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1, notification_count);
  const char* id = file_manager_private::ToString(sync_error.type);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings =
      notification_platform_bridge->GetNotificationStringsById(id);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kGoogleDrive);
  EXPECT_EQ(notification_strings.message,
            u"\"fake.txt\" has been shared with you. You cannot delete it "
            u"because you do not own it.");

  // Setup for the service unavailable error.
  sync_error.type =
      file_manager_private::DRIVE_SYNC_ERROR_TYPE_SERVICE_UNAVAILABLE;
  event = std::make_unique<extensions::Event>(
      extensions::events::FILE_MANAGER_PRIVATE_ON_DRIVE_SYNC_ERROR,
      file_manager_private::OnDriveSyncError::kEventName,
      file_manager_private::OnDriveSyncError::Create(sync_error));

  // Send the service unavailable sync error event.
  notification_manager->HandleEvent(*event.get());
  // Get the number of notifications from the NotificationDisplayService.
  notification_display_service->GetDisplayed(
      base::BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  // Check: We have two notifications.
  ASSERT_EQ(2, notification_count);
  id = file_manager_private::ToString(sync_error.type);
  // Get the strings for the displayed notification.
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(id);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kGoogleDrive);
  EXPECT_EQ(notification_strings.message,
            u"Google Drive is not available right now. Uploading will "
            u"automatically restart once Google Drive is back.");

  // Setup for the no server space error.
  sync_error.type = file_manager_private::DRIVE_SYNC_ERROR_TYPE_NO_SERVER_SPACE;
  event = std::make_unique<extensions::Event>(
      extensions::events::FILE_MANAGER_PRIVATE_ON_DRIVE_SYNC_ERROR,
      file_manager_private::OnDriveSyncError::kEventName,
      file_manager_private::OnDriveSyncError::Create(sync_error));

  // Send the service unavailable sync error event.
  notification_manager->HandleEvent(*event.get());
  // Get the number of notifications from the NotificationDisplayService.
  notification_display_service->GetDisplayed(
      base::BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  // Check: We have three notifications.
  ASSERT_EQ(3, notification_count);
  id = file_manager_private::ToString(sync_error.type);
  // Get the strings for the displayed notification.
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(id);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kGoogleDrive);
  EXPECT_EQ(notification_strings.message,
            u"\"fake.txt\" was not uploaded. There is not enough free space in "
            u"your Google Drive.");

  // Setup for the no local space error.
  sync_error.type = file_manager_private::DRIVE_SYNC_ERROR_TYPE_NO_LOCAL_SPACE;
  event = std::make_unique<extensions::Event>(
      extensions::events::FILE_MANAGER_PRIVATE_ON_DRIVE_SYNC_ERROR,
      file_manager_private::OnDriveSyncError::kEventName,
      file_manager_private::OnDriveSyncError::Create(sync_error));

  // Send the service unavailable sync error event.
  notification_manager->HandleEvent(*event.get());
  // Get the number of notifications from the NotificationDisplayService.
  notification_display_service->GetDisplayed(
      base::BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  // Check: We have four notifications.
  ASSERT_EQ(4, notification_count);
  id = file_manager_private::ToString(sync_error.type);
  // Get the strings for the displayed notification.
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(id);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kGoogleDrive);
  EXPECT_EQ(notification_strings.message, u"You have run out of space");

  // Setup for the miscellaneous sync error.
  sync_error.type = file_manager_private::DRIVE_SYNC_ERROR_TYPE_MISC;
  event = std::make_unique<extensions::Event>(
      extensions::events::FILE_MANAGER_PRIVATE_ON_DRIVE_SYNC_ERROR,
      file_manager_private::OnDriveSyncError::kEventName,
      file_manager_private::OnDriveSyncError::Create(sync_error));

  // Send the service unavailable sync error event.
  notification_manager->HandleEvent(*event.get());
  // Get the number of notifications from the NotificationDisplayService.
  notification_display_service->GetDisplayed(
      base::BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  // Check: We have five notifications.
  ASSERT_EQ(5, notification_count);
  id = file_manager_private::ToString(sync_error.type);
  // Get the strings for the displayed notification.
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(id);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kGoogleDrive);
  EXPECT_EQ(notification_strings.message,
            u"Google Drive was unable to sync \"fake.txt\" right now. Google "
            u"Drive will try again later.");
}

// Tests the generation of the DriveFS enable offline notification.
// This is triggered when users make files available offline and the
// Google Drive offline setting at https://drive.google.com isn't enabled.
TEST_F(SystemNotificationManagerTest, EnableDocsOffline) {
  file_manager_private::DriveConfirmDialogEvent drive_event;
  drive_event.type =
      file_manager_private::DRIVE_CONFIRM_DIALOG_TYPE_ENABLE_DOCS_OFFLINE;
  drive_event.file_url = "drivefs://fake";
  std::unique_ptr<extensions::Event> event =
      std::make_unique<extensions::Event>(
          extensions::events::FILE_MANAGER_PRIVATE_ON_DRIVE_CONFIRM_DIALOG,
          file_manager_private::OnDriveConfirmDialog::kEventName,
          file_manager_private::OnDriveConfirmDialog::Create(drive_event));
  GetSystemNotificationManager()->HandleEvent(*event.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          "swa-drive-confirm-dialog");
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kGoogleDrive);
  EXPECT_EQ(notification_strings.message,
            u"Enable Google Docs Offline to make Docs, Sheets and Slides "
            u"available offline.");
}

TEST_F(SystemNotificationManagerTest, SyncProgressSingle) {
  // Setup a sync progress status object.
  file_manager_private::FileTransferStatus transfer_status;
  transfer_status.transfer_state =
      file_manager_private::TRANSFER_STATE_IN_PROGRESS;
  transfer_status.num_total_jobs = 1;
  transfer_status.file_url =
      "filesystem:chrome://file-manager/drive/MyDrive-test-user/file.txt";
  transfer_status.processed = 0;
  transfer_status.total = 100;
  std::unique_ptr<extensions::Event> event =
      std::make_unique<extensions::Event>(
          extensions::events::FILE_MANAGER_PRIVATE_ON_FILE_TRANSFERS_UPDATED,
          file_manager_private::OnFileTransfersUpdated::kEventName,
          file_manager_private::OnFileTransfersUpdated::Create(
              transfer_status));

  // Send the transfers updated event.
  GetSystemNotificationManager()->HandleEvent(*event.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          "swa-drive-sync");
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, u"Files");
  EXPECT_EQ(notification_strings.message, u"Syncing file.txt\x2026");
  // Setup an completed transfer event.
  transfer_status.transfer_state =
      file_manager_private::TRANSFER_STATE_COMPLETED;
  transfer_status.num_total_jobs = 0;
  event = std::make_unique<extensions::Event>(
      extensions::events::FILE_MANAGER_PRIVATE_ON_FILE_TRANSFERS_UPDATED,
      file_manager_private::OnFileTransfersUpdated::kEventName,
      file_manager_private::OnFileTransfersUpdated::Create(transfer_status));

  // Send the completed transfer event.
  GetSystemNotificationManager()->HandleEvent(*event.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have 0 notifications (notification closed on end).
  ASSERT_EQ(0, notification_count);
  // Start another transfer that ends in error.
  transfer_status.transfer_state =
      file_manager_private::TRANSFER_STATE_IN_PROGRESS;
  event = std::make_unique<extensions::Event>(
      extensions::events::FILE_MANAGER_PRIVATE_ON_FILE_TRANSFERS_UPDATED,
      file_manager_private::OnFileTransfersUpdated::kEventName,
      file_manager_private::OnFileTransfersUpdated::Create(transfer_status));

  // Send the transfers updated event.
  GetSystemNotificationManager()->HandleEvent(*event.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1, notification_count);
  // Setup an completed transfer event.
  transfer_status.transfer_state = file_manager_private::TRANSFER_STATE_FAILED;
  transfer_status.num_total_jobs = 0;
  event = std::make_unique<extensions::Event>(
      extensions::events::FILE_MANAGER_PRIVATE_ON_FILE_TRANSFERS_UPDATED,
      file_manager_private::OnFileTransfersUpdated::kEventName,
      file_manager_private::OnFileTransfersUpdated::Create(transfer_status));

  // Send the completed transfer event.
  GetSystemNotificationManager()->HandleEvent(*event.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have 0 notifications (notification closed on end).
  ASSERT_EQ(0, notification_count);
}

TEST_F(SystemNotificationManagerTest, SyncProgressMultiple) {
  // Setup a sync progress status object.
  file_manager_private::FileTransferStatus transfer_status;
  transfer_status.transfer_state =
      file_manager_private::TRANSFER_STATE_IN_PROGRESS;
  transfer_status.num_total_jobs = 10;
  transfer_status.file_url =
      "filesystem:chrome://file-manager/drive/MyDrive-test-user/file.txt";
  transfer_status.processed = 0;
  transfer_status.total = 100;
  std::unique_ptr<extensions::Event> event =
      std::make_unique<extensions::Event>(
          extensions::events::FILE_MANAGER_PRIVATE_ON_FILE_TRANSFERS_UPDATED,
          file_manager_private::OnFileTransfersUpdated::kEventName,
          file_manager_private::OnFileTransfersUpdated::Create(
              transfer_status));

  // Send the transfers updated event.
  GetSystemNotificationManager()->HandleEvent(*event.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          "swa-drive-sync");
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, u"Files");
  EXPECT_EQ(notification_strings.message, u"Syncing 10 items\x2026");
}

TEST_F(SystemNotificationManagerTest, PinProgressSingle) {
  // Setup a pin progress status object.
  file_manager_private::FileTransferStatus pin_status;
  pin_status.transfer_state = file_manager_private::TRANSFER_STATE_IN_PROGRESS;
  pin_status.num_total_jobs = 1;
  pin_status.file_url =
      "filesystem:chrome://file-manager/drive/MyDrive-test-user/file.txt";
  pin_status.processed = 0;
  pin_status.total = 100;
  std::unique_ptr<extensions::Event> event =
      std::make_unique<extensions::Event>(
          extensions::events::FILE_MANAGER_PRIVATE_ON_PIN_TRANSFERS_UPDATED,
          file_manager_private::OnFileTransfersUpdated::kEventName,
          file_manager_private::OnFileTransfersUpdated::Create(pin_status));

  // Send the transfers updated event.
  GetSystemNotificationManager()->HandleEvent(*event.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings =
      notification_platform_bridge->GetNotificationStringsById("swa-drive-pin");
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, u"Files");
  EXPECT_EQ(notification_strings.message, u"Making file.txt available offline");
  // Setup an completed transfer event.
  pin_status.transfer_state = file_manager_private::TRANSFER_STATE_COMPLETED;
  pin_status.num_total_jobs = 0;
  event = std::make_unique<extensions::Event>(
      extensions::events::FILE_MANAGER_PRIVATE_ON_PIN_TRANSFERS_UPDATED,
      file_manager_private::OnFileTransfersUpdated::kEventName,
      file_manager_private::OnFileTransfersUpdated::Create(pin_status));

  // Send the completed transfer event.
  GetSystemNotificationManager()->HandleEvent(*event.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have 0 notifications (notification closed on end).
  ASSERT_EQ(0, notification_count);

  // Start another transfer that ends in error.
  pin_status.transfer_state = file_manager_private::TRANSFER_STATE_IN_PROGRESS;
  event = std::make_unique<extensions::Event>(
      extensions::events::FILE_MANAGER_PRIVATE_ON_PIN_TRANSFERS_UPDATED,
      file_manager_private::OnFileTransfersUpdated::kEventName,
      file_manager_private::OnFileTransfersUpdated::Create(pin_status));

  // Send the transfers updated event.
  GetSystemNotificationManager()->HandleEvent(*event.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1, notification_count);
  // Setup an completed transfer event.
  pin_status.transfer_state = file_manager_private::TRANSFER_STATE_FAILED;
  pin_status.num_total_jobs = 0;
  event = std::make_unique<extensions::Event>(
      extensions::events::FILE_MANAGER_PRIVATE_ON_PIN_TRANSFERS_UPDATED,
      file_manager_private::OnFileTransfersUpdated::kEventName,
      file_manager_private::OnFileTransfersUpdated::Create(pin_status));

  // Send the completed transfer event.
  GetSystemNotificationManager()->HandleEvent(*event.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have 0 notifications (notification closed on end).
  ASSERT_EQ(0, notification_count);
}

TEST_F(SystemNotificationManagerTest, PinProgressMultiple) {
  // Setup a pin progress status object.
  file_manager_private::FileTransferStatus pin_status;
  pin_status.transfer_state = file_manager_private::TRANSFER_STATE_IN_PROGRESS;
  pin_status.num_total_jobs = 10;
  pin_status.file_url =
      "filesystem:chrome://file-manager/drive/MyDrive-test-user/file.txt";
  pin_status.processed = 0;
  pin_status.total = 100;
  std::unique_ptr<extensions::Event> event =
      std::make_unique<extensions::Event>(
          extensions::events::FILE_MANAGER_PRIVATE_ON_PIN_TRANSFERS_UPDATED,
          file_manager_private::OnFileTransfersUpdated::kEventName,
          file_manager_private::OnFileTransfersUpdated::Create(pin_status));

  // Send the transfers updated event.
  GetSystemNotificationManager()->HandleEvent(*event.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings =
      notification_platform_bridge->GetNotificationStringsById("swa-drive-pin");
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, u"Files");
  EXPECT_EQ(notification_strings.message, u"Making 10 files available offline");
}

}  // namespace file_manager
