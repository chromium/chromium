// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/system_notification_manager.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/webui/file_manager/url_constants.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/extensions/file_manager/device_event_router.h"
#include "chrome/browser/ash/extensions/file_manager/event_router.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_platform_bridge_delegator.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/disks/disk.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace file_manager {

namespace file_manager_private = extensions::api::file_manager_private;

// Struct to reference notification strings for testing.
struct TestNotificationStrings {
  std::u16string title;
  std::u16string message;
  std::vector<std::u16string> buttons;
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
    for (const message_center::ButtonInfo& button : notification.buttons())
      strings.buttons.push_back(button.title);
    notifications_[notification.id()] = strings;
    delegates_[notification.id()] = notification.delegate();
  }

  void Close(NotificationHandler::Type notification_type,
             const std::string& notification_id) override {
    notification_ids_.erase(notification_id);
    notifications_.erase(notification_id);
    delegates_.erase(notification_id);
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

  void ClickButtonIndexById(const std::string& notification_id,
                            int button_index) {
    absl::optional<int> index(button_index);
    absl::optional<std::u16string> empty_reply(u"");

    const auto& notification = delegates_.find(notification_id);
    if (notification != delegates_.end()) {
      notification->second->Click(index, empty_reply);
    }
  }

 private:
  std::set<std::string> notification_ids_;
  // Used to map a notification id to its displayed title and message.
  std::map<std::string, TestNotificationStrings> notifications_;
  // Used to map a notification id to its delegate to verify click handlers.
  std::map<std::string, scoped_refptr<message_center::NotificationDelegate>>
      delegates_;
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

class SystemNotificationManagerTest
    : public io_task::IOTaskController::Observer,
      public ::testing::Test {
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
    notification_manager_ =
        std::make_unique<SystemNotificationManager>(profile_);
    device_event_router_ = std::make_unique<DeviceEventRouterImpl>(
        notification_manager_.get(), profile_);

    VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile_,
        base::BindLambdaForTesting([this](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(std::make_unique<VolumeManager>(
              Profile::FromBrowserContext(context), nullptr, nullptr,
              &disk_mount_manager_, nullptr,
              VolumeManager::GetMtpStorageInfoCallback()));
        }));
    auto* volume_manager = VolumeManager::Get(profile_);

    // SystemNotificationManager needs the IOTaskController to be able to cancel
    // the task.
    io_task_controller = volume_manager->io_task_controller();
    notification_manager_->SetIOTaskController(io_task_controller);
    io_task_controller->AddObserver(this);

    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    ASSERT_TRUE(dir_.GetPath().IsAbsolute());
    file_system_context = storage::CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());

    volume_manager->AddVolumeForTesting(
        CreateTestFile("volume/").path(), VOLUME_TYPE_DOWNLOADS_DIRECTORY,
        ash::DeviceType::kUnknown, false /* read only */);
  }

  void TearDown() override {
    io_task_controller->RemoveObserver(this);

    profile_manager_->DeleteAllTestingProfiles();
    profile_ = nullptr;
    profile_manager_.reset();
  }

  // IOTaskController::Observer override:
  // In production code the observer is EventRouter which forwards the status to
  // SystemNotificationManager.
  void OnIOTaskStatus(const io_task::ProgressStatus& status) override {
    task_statuses[status.task_id].push_back(status.state);
    notification_manager_->HandleIOTaskProgress(status);
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

  size_t GetNotificationCount() {
    auto* notification_display_service = GetNotificationDisplayService();
    notification_display_service->GetDisplayed(
        base::BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
                       weak_ptr_factory_.GetWeakPtr()));
    return notification_count;
  }

  void GetNotificationsCallback(std::set<std::string> displayed_notifications,
                                bool supports_synchronization) {
    notification_count = displayed_notifications.size();
  }

  // Creates a disk instance with |device_path| and |mount_path| for testing.
  std::unique_ptr<ash::disks::Disk> CreateTestDisk(
      const std::string& device_path,
      const std::string& mount_path,
      bool is_read_only_hardware,
      bool is_mounted) {
    return ash::disks::Disk::Builder()
        .SetDevicePath(device_path)
        .SetMountPath(mount_path)
        .SetStorageDevicePath(device_path)
        .SetIsReadOnlyHardware(is_read_only_hardware)
        .SetFileSystemType("vfat")
        .SetIsMounted(is_mounted)
        .Build();
  }

  // Creates a file or directory to use in the test.
  storage::FileSystemURL CreateTestFile(const std::string& path) {
    const blink::StorageKey storage_key =
        blink::StorageKey::CreateFromStringForTesting(
            ash::file_manager::kChromeUIFileManagerURL);

    auto file_url = file_system_context->CreateCrackedFileSystemURL(
        storage_key, storage::kFileSystemTypeTest,
        base::FilePath::FromUTF8Unsafe(path));

    if (base::EndsWith(path, "/")) {
      CHECK(base::File::FILE_OK ==
            storage::AsyncFileTestHelper::CreateDirectory(
                file_system_context.get(), file_url));
    } else {
      CHECK(base::File::FILE_OK == storage::AsyncFileTestHelper::CreateFile(
                                       file_system_context.get(), file_url));
    }

    return file_url;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  file_manager::FakeDiskMountManager disk_mount_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  // Externally owned raw pointers:
  // profile_ is owned by TestingProfileManager.
  TestingProfile* profile_;
  // notification_display_service is owned by NotificationDisplayServiceFactory.
  NotificationDisplayServiceImpl* notification_display_service_;
  std::unique_ptr<SystemNotificationManager> notification_manager_;
  std::unique_ptr<DeviceEventRouterImpl> device_event_router_;

  // Temporary directory used to test IOTask progress.
  base::ScopedTempDir dir_;

 public:
  size_t notification_count;
  // notification_platform_bridge is owned by NotificationDisplayService.
  TestNotificationPlatformBridgeDelegator* notification_platform_bridge;

  // Used for tests with IOTask:
  io_task::IOTaskController* io_task_controller;
  scoped_refptr<storage::FileSystemContext> file_system_context;

  // Keep track of the task state transitions.
  std::map<io_task::IOTaskId, std::vector<io_task::State>> task_statuses;

  base::WeakPtrFactory<SystemNotificationManagerTest> weak_ptr_factory_{this};
};

constexpr char kDevicePath[] = "/device/test";
constexpr char kMountPath[] = "/mnt/media/sda1";
std::u16string kRemovableDeviceTitle = u"Removable device detected";

TEST_F(SystemNotificationManagerTest, ExternalStorageDisabled) {
  base::HistogramTester histogram_tester;
  // Send a removable volume mounted event.
  GetDeviceEventRouter()->OnDeviceAdded(kDevicePath);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
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
  // Check that the correct UMA was emitted.
  histogram_tester.ExpectUniqueSample(
      kNotificationShowHistogramName,
      DeviceNotificationUmaType::DEVICE_EXTERNAL_STORAGE_DISABLED, 1);
}

constexpr char kDeviceLabel[] = "MyUSB";
std::u16string kFormatTitle = u"Format MyUSB";

TEST_F(SystemNotificationManagerTest, FormatStart) {
  base::HistogramTester histogram_tester;
  GetDeviceEventRouter()->OnFormatStarted(kDevicePath, kDeviceLabel,
                                          /*success=*/true);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById("format_start");
  // Check: the expected strings match.
  std::u16string kFormatStartMesssage = u"Formatting MyUSB\x2026";
  EXPECT_EQ(notification_strings.title, kFormatTitle);
  EXPECT_EQ(notification_strings.message, kFormatStartMesssage);
  histogram_tester.ExpectUniqueSample(kNotificationShowHistogramName,
                                      DeviceNotificationUmaType::FORMAT_START,
                                      1);
}

TEST_F(SystemNotificationManagerTest, FormatSuccess) {
  base::HistogramTester histogram_tester;
  GetDeviceEventRouter()->OnFormatCompleted(kDevicePath, kDeviceLabel,
                                            /*success=*/true);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          "format_success");
  // Check: the expected strings match.
  std::u16string kFormatSuccessMesssage = u"Formatted MyUSB";
  EXPECT_EQ(notification_strings.title, kFormatTitle);
  EXPECT_EQ(notification_strings.message, kFormatSuccessMesssage);
  histogram_tester.ExpectUniqueSample(kNotificationShowHistogramName,
                                      DeviceNotificationUmaType::FORMAT_SUCCESS,
                                      1);
}

TEST_F(SystemNotificationManagerTest, FormatFail) {
  base::HistogramTester histogram_tester;
  GetDeviceEventRouter()->OnFormatCompleted(kDevicePath, kDeviceLabel,
                                            /*success=*/false);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById("format_fail");
  // Check: the expected strings match.
  std::u16string kFormatFailedMesssage = u"Could not format MyUSB";
  EXPECT_EQ(notification_strings.title, kFormatTitle);
  EXPECT_EQ(notification_strings.message, kFormatFailedMesssage);
  histogram_tester.ExpectUniqueSample(kNotificationShowHistogramName,
                                      DeviceNotificationUmaType::FORMAT_FAIL,
                                      1);
}

constexpr char kPartitionLabel[] = "OEM";
std::u16string kPartitionTitle = u"Format OEM";

TEST_F(SystemNotificationManagerTest, PartitionFail) {
  base::HistogramTester histogram_tester;
  GetDeviceEventRouter()->OnPartitionCompleted(kDevicePath, kPartitionLabel,
                                               /*success=*/false);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          "partition_fail");
  // Check: the expected strings match.
  std::u16string kPartitionFailMesssage = u"Could not format OEM";
  EXPECT_EQ(notification_strings.title, kPartitionTitle);
  EXPECT_EQ(notification_strings.message, kPartitionFailMesssage);
  histogram_tester.ExpectUniqueSample(kNotificationShowHistogramName,
                                      DeviceNotificationUmaType::PARTITION_FAIL,
                                      1);
}

TEST_F(SystemNotificationManagerTest, RenameFail) {
  base::HistogramTester histogram_tester;
  GetDeviceEventRouter()->OnRenameCompleted(kDevicePath, kPartitionLabel,
                                            /*success=*/false);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById("rename_fail");
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, u"Renaming failed");
  EXPECT_EQ(notification_strings.message,
            u"Aw, Snap! There was an error during renaming.");
  // Check that the correct UMA was emitted.
  histogram_tester.ExpectUniqueSample(kNotificationShowHistogramName,
                                      DeviceNotificationUmaType::RENAME_FAIL,
                                      1);
}

TEST_F(SystemNotificationManagerTest, DeviceHardUnplugged) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<ash::disks::Disk> disk =
      CreateTestDisk(kDevicePath, kMountPath, /*is_read_only_hardware=*/false,
                     /*is_mounted=*/true);
  GetDeviceEventRouter()->OnDiskRemoved(*disk);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
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
  // Check that the correct UMA was emitted.
  histogram_tester.ExpectUniqueSample(
      kNotificationShowHistogramName,
      DeviceNotificationUmaType::DEVICE_HARD_UNPLUGGED, 1);
}

constexpr char kRemovableDeviceNotificationId[] = "swa-removable-device-id";

TEST_F(SystemNotificationManagerTest, DeviceNavigation) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, ash::DeviceType::kUSB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_ERROR_SUCCESS;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          kRemovableDeviceNotificationId);
  notification_platform_bridge->ClickButtonIndexById(
      kRemovableDeviceNotificationId,
      /*button_index=*/0);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Explore the device\x2019s content in the Files app.");
  // Check that the correct UMA was emitted.
  histogram_tester.ExpectUniqueSample(
      kNotificationShowHistogramName,
      DeviceNotificationUmaType::DEVICE_NAVIGATION, 1);
  histogram_tester.ExpectUniqueSample(
      kNotificationUserActionHistogramName,
      DeviceNotificationUserActionUmaType::OPEN_MEDIA_DEVICE_NAVIGATION, 1);
}

// Test for notification generated when enterprise read-only policy is set.
// Condition that triggers that is a mount event for a removable device
// and the volume has read only set.
TEST_F(SystemNotificationManagerTest, DeviceNavigationReadOnlyPolicy) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, ash::DeviceType::kUSB,
      /*read_only=*/true, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_ERROR_SUCCESS;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          kRemovableDeviceNotificationId);
  notification_platform_bridge->ClickButtonIndexById(
      kRemovableDeviceNotificationId,
      /*button_index=*/0);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Explore the device's content in the Files app. The content is "
            u"restricted by an admin and can\x2019t be modified.");
  // Check that the correct UMA was emitted.
  histogram_tester.ExpectUniqueSample(
      kNotificationShowHistogramName,
      DeviceNotificationUmaType::DEVICE_NAVIGATION_READONLY_POLICY, 1);
  histogram_tester.ExpectUniqueSample(
      kNotificationUserActionHistogramName,
      DeviceNotificationUserActionUmaType::OPEN_MEDIA_DEVICE_NAVIGATION, 1);
}

// Test for notification generated when ARC++ is enabled on the device.
// Condition that triggers that is a mount event for a removable device
// when the removable access for ARC++ is disabled.
TEST_F(SystemNotificationManagerTest, DeviceNavigationAllowAppAccess) {
  base::HistogramTester histogram_tester;
  // Set the ARC++ enbled preference on the testing profile.
  PrefService* const service = GetProfile()->GetPrefs();
  service->SetBoolean(arc::prefs::kArcEnabled, true);
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, ash::DeviceType::kUSB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_ERROR_SUCCESS;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          kRemovableDeviceNotificationId);
  notification_platform_bridge->ClickButtonIndexById(
      kRemovableDeviceNotificationId,
      /*button_index=*/0);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Explore the device\x2019s content in the Files app. For device "
            u"preferences, go to Settings.");
  // Check that the correct UMA was emitted.
  histogram_tester.ExpectUniqueSample(
      kNotificationShowHistogramName,
      DeviceNotificationUmaType::DEVICE_NAVIGATION_ALLOW_APP_ACCESS, 1);
  histogram_tester.ExpectUniqueSample(
      kNotificationUserActionHistogramName,
      DeviceNotificationUserActionUmaType::OPEN_MEDIA_DEVICE_NAVIGATION_ARC, 1);
}

TEST_F(SystemNotificationManagerTest,
       DeviceNavigationAllowAppAccessSecondButton) {
  base::HistogramTester histogram_tester;
  // Set the ARC++ enbled preference on the testing profile.
  PrefService* const service = GetProfile()->GetPrefs();
  service->SetBoolean(arc::prefs::kArcEnabled, true);
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, ash::DeviceType::kUSB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_ERROR_SUCCESS;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
  notification_platform_bridge->ClickButtonIndexById(
      kRemovableDeviceNotificationId,
      /*button_index=*/1);
  // Check that the correct UMA was emitted.
  histogram_tester.ExpectUniqueSample(
      kNotificationUserActionHistogramName,
      DeviceNotificationUserActionUmaType::OPEN_SETTINGS_FOR_ARC_STORAGE, 1);
}

// Test for notification generated when ARC++ is enabled on the device.
// Condition that triggers that is a mount event for a removable device
// when the removable access for ARC++ is enabled.
TEST_F(SystemNotificationManagerTest, DeviceNavigationAppsHaveAccess) {
  base::HistogramTester histogram_tester;
  // Set the ARC++ enbled preference on the testing profile.
  PrefService* const service = GetProfile()->GetPrefs();
  service->SetBoolean(arc::prefs::kArcEnabled, true);
  service->SetBoolean(arc::prefs::kArcHasAccessToRemovableMedia, true);
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, ash::DeviceType::kUSB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_ERROR_SUCCESS;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          kRemovableDeviceNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Explore the device\x2019s content in the Files app. Play Store "
            u"applications have access to this device.");
  // Check that the correct UMA was emitted.
  histogram_tester.ExpectUniqueSample(
      kNotificationShowHistogramName,
      DeviceNotificationUmaType::DEVICE_NAVIGATION_APPS_HAVE_ACCESS, 1);
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
  base::HistogramTester histogram_tester;
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, ash::DeviceType::kUSB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      "", "FAT32"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_ERROR_UNSUPPORTED_FILESYSTEM;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
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
  // Check that the correct UMA was emitted.
  histogram_tester.ExpectUniqueSample(kNotificationShowHistogramName,
                                      DeviceNotificationUmaType::DEVICE_FAIL,
                                      1);
}

// The named version of the device unsupported notification is
// generated when the device includes a device label.
TEST_F(SystemNotificationManagerTest, DeviceUnsupportedNamed) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, ash::DeviceType::kUSB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_ERROR_UNSUPPORTED_FILESYSTEM;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Sorry, the device MyUSB is not supported at this time.");
  // Check that the correct UMA was emitted.
  histogram_tester.ExpectUniqueSample(kNotificationShowHistogramName,
                                      DeviceNotificationUmaType::DEVICE_FAIL,
                                      1);
}

// Multipart device unsupported notifications are generated when there is
// a multi-partition device inserted and one partition has a known filesystem
// but other partitions have unsupported file systems.
// Note: In such cases the Chrome OS device will generate 2 notifications:
//       1) A device navigation notification for the supported file system
//       2) The multipart device unsupported notification.
TEST_F(SystemNotificationManagerTest, MultipartDeviceUnsupportedDefault) {
  base::HistogramTester histogram_tester;
  // Build a supported file system volume and mount it.
  std::unique_ptr<Volume> volume1(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, ash::DeviceType::kUSB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      "", "FAT32"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_ERROR_SUCCESS;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume1.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          kRemovableDeviceNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Explore the device\x2019s content in the Files app.");
  // Build an unsupported file system volume and mount it on the same device.
  std::unique_ptr<Volume> volume2(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path2")),
      VolumeType::VOLUME_TYPE_TESTING, ash::DeviceType::kUSB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      "", "unsupported"));
  event.status = file_manager_private::MOUNT_ERROR_UNSUPPORTED_FILESYSTEM;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume2.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have two notifications.
  ASSERT_EQ(2u, notification_count);
  // Get the strings for the displayed notification.
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Sorry, at least one partition on your external storage device "
            u"could not be mounted.");
  // A DEVICE_NAVIGATION UMA is emitted during the setup so just check for the
  // occurrence of the DEVICE_FAIL sample instead.
  histogram_tester.ExpectBucketCount(kNotificationShowHistogramName,
                                     DeviceNotificationUmaType::DEVICE_FAIL, 1);
}

// The named version of the multipart device unsupported notification is
// generated when the device label exists and at least one partition can be
// mounted on a device with an unsupported file system on another partition.
TEST_F(SystemNotificationManagerTest, MultipartDeviceUnsupportedNamed) {
  base::HistogramTester histogram_tester;
  // Build a supported file system volume and mount it.
  std::unique_ptr<Volume> volume1(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, ash::DeviceType::kUSB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_ERROR_SUCCESS;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume1.get());
  // Ignore checking for the device navigation notification.
  // Build an unsupported file system volume and mount it on the same device.
  std::unique_ptr<Volume> volume2(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path2")),
      VolumeType::VOLUME_TYPE_TESTING, ash::DeviceType::kUSB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "unsupported"));
  event.status = file_manager_private::MOUNT_ERROR_UNSUPPORTED_FILESYSTEM;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume2.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have two notifications.
  ASSERT_EQ(2u, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Sorry, at least one partition on the device MyUSB could not be "
            u"mounted.");
  // A DEVICE_NAVIGATION UMA is emitted during the setup so just check for the
  // occurrence of the DEVICE_FAIL sample instead.
  histogram_tester.ExpectBucketCount(kNotificationShowHistogramName,
                                     DeviceNotificationUmaType::DEVICE_FAIL, 1);
}

// Device fail unknown notifications are generated when the type of filesystem
// on a removable device cannot be determined.
// The default version of the notifiication is generated when there is
// no drive label.
// These notifications are similar to the device unsupported notifications,
// the difference being an unknown vs. unsupported file system.
TEST_F(SystemNotificationManagerTest, DeviceFailUnknownDefault) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, ash::DeviceType::kUSB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      "", "unknown"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_ERROR_UNKNOWN_FILESYSTEM;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          kDeviceFailNotificationId);
  notification_platform_bridge->ClickButtonIndexById(kDeviceFailNotificationId,
                                                     /*button_index=*/0);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Sorry, your external storage device could not be recognized.");
  EXPECT_EQ(notification_strings.buttons.size(), 1u);
  EXPECT_EQ(notification_strings.buttons[0], u"Format this device");
  histogram_tester.ExpectUniqueSample(
      kNotificationShowHistogramName,
      DeviceNotificationUmaType::DEVICE_FAIL_UNKNOWN, 1);
  histogram_tester.ExpectUniqueSample(
      kNotificationUserActionHistogramName,
      DeviceNotificationUserActionUmaType::OPEN_MEDIA_DEVICE_FAIL, 1);
}

// The named version of the device fail unknown notification is
// generated when the device includes a device label.
TEST_F(SystemNotificationManagerTest, DeviceFailUnknownNamed) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, ash::DeviceType::kUSB,
      /*read_only=*/false, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "unknown"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_ERROR_UNKNOWN_FILESYSTEM;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          kDeviceFailNotificationId);
  notification_platform_bridge->ClickButtonIndexById(kDeviceFailNotificationId,
                                                     /*button_index=*/0);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Sorry, the device MyUSB could not be recognized.");
  EXPECT_EQ(notification_strings.buttons.size(), 1u);
  EXPECT_EQ(notification_strings.buttons[0], u"Format this device");
  histogram_tester.ExpectUniqueSample(
      kNotificationShowHistogramName,
      DeviceNotificationUmaType::DEVICE_FAIL_UNKNOWN, 1);
  histogram_tester.ExpectUniqueSample(
      kNotificationUserActionHistogramName,
      DeviceNotificationUserActionUmaType::OPEN_MEDIA_DEVICE_FAIL, 1);
}

// Device fail unknown read only notifications are generated when
// volumes with read only set and an unknown file system is mounted.
// The default notification message is generated when there is
// no device label.
TEST_F(SystemNotificationManagerTest, DeviceFailUnknownReadOnlyDefault) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, ash::DeviceType::kUSB,
      /*read_only=*/true, base::FilePath(FILE_PATH_LITERAL("/device/test")), "",
      "unknown"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_ERROR_UNKNOWN_FILESYSTEM;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Sorry, your external storage device could not be recognized.");
  // Device is read-only, expect no buttons present.
  EXPECT_EQ(notification_strings.buttons.size(), 0u);
  histogram_tester.ExpectUniqueSample(
      kNotificationShowHistogramName,
      DeviceNotificationUmaType::DEVICE_FAIL_UNKNOWN_READONLY, 1);
}

// The named version of the read only device fail unknown notification is
// generated when the device includes a device label.
TEST_F(SystemNotificationManagerTest, DeviceFailUnknownReadOnlyNamed) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, ash::DeviceType::kUSB,
      /*read_only=*/true, base::FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "unknown"));
  file_manager_private::MountCompletedEvent event;
  event.event_type = file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT;
  event.should_notify = true;
  event.status = file_manager_private::MOUNT_ERROR_UNKNOWN_FILESYSTEM;
  GetSystemNotificationManager()->HandleMountCompletedEvent(event,
                                                            *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(GetProfile())
      ->GetDisplayed(base::BindOnce(
          &SystemNotificationManagerTest::GetNotificationsCallback,
          weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings;
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(notification_strings.message,
            u"Sorry, the device MyUSB could not be recognized.");
  histogram_tester.ExpectUniqueSample(
      kNotificationShowHistogramName,
      DeviceNotificationUmaType::DEVICE_FAIL_UNKNOWN_READONLY, 1);
}

storage::FileSystemURL CreateFileSystemURL(std::string url) {
  return storage::FileSystemURL::CreateForTest(GURL(url));
}

TEST_F(SystemNotificationManagerTest, HandleIOTaskProgressCopy) {
  // The system notification only sees the IOTask ProgressStatus.
  file_manager::io_task::ProgressStatus status;
  status.task_id = 1;
  status.state = file_manager::io_task::State::kQueued;
  status.type = file_manager::io_task::OperationType::kCopy;
  status.total_bytes = 100;
  status.bytes_transferred = 0;
  status.sources.emplace_back(CreateTestFile("volume/src_file.txt"),
                              absl::nullopt);
  status.destination_folder = CreateTestFile("volume/dest_dir/");

  // Send the copy begin/queued progress.
  auto* notification_manager = GetSystemNotificationManager();
  notification_manager->HandleIOTaskProgress(status);

  // Check: We have the 1 notification.
  ASSERT_EQ(1u, GetNotificationCount());

  TestNotificationStrings notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          "swa-file-operation-1");

  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, u"Files");
  EXPECT_EQ(notification_strings.message, u"Copying src_file.txt\x2026");

  // Send the copy progress.
  status.bytes_transferred = 30;
  status.state = file_manager::io_task::State::kInProgress;
  notification_manager->HandleIOTaskProgress(status);

  // Check: We have the same notification.
  ASSERT_EQ(1u, GetNotificationCount());
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          "swa-file-operation-1");
  EXPECT_EQ(notification_strings.title, u"Files");
  EXPECT_EQ(notification_strings.message, u"Copying src_file.txt\x2026");

  // Send the success progress status.
  status.bytes_transferred = 100;
  status.state = file_manager::io_task::State::kSuccess;
  notification_manager->HandleIOTaskProgress(status);

  // Notification should disappear.
  ASSERT_EQ(0u, GetNotificationCount());
}

TEST_F(SystemNotificationManagerTest, HandleIOTaskProgressExtract) {
  // The system notification only sees the IOTask ProgressStatus.
  file_manager::io_task::ProgressStatus status;
  status.task_id = 1;
  status.state = file_manager::io_task::State::kQueued;
  status.type = file_manager::io_task::OperationType::kExtract;
  status.total_bytes = 100;
  status.bytes_transferred = 0;
  status.sources.emplace_back(CreateTestFile("volume/src_file.zip"),
                              absl::nullopt);
  status.destination_folder = CreateTestFile("volume/src_file/");

  // Send the copy begin/queued progress.
  auto* notification_manager = GetSystemNotificationManager();
  notification_manager->HandleIOTaskProgress(status);

  // Check: We have the 1 notification.
  ASSERT_EQ(1u, GetNotificationCount());

  TestNotificationStrings notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          "swa-file-operation-1");

  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, u"Files");
  EXPECT_EQ(notification_strings.message, u"Extracting src_file.zip\x2026");

  // Send the copy progress.
  status.bytes_transferred = 30;
  status.state = file_manager::io_task::State::kInProgress;
  notification_manager->HandleIOTaskProgress(status);

  // Check: We have the same notification.
  ASSERT_EQ(1u, GetNotificationCount());
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(
          "swa-file-operation-1");
  EXPECT_EQ(notification_strings.title, u"Files");
  EXPECT_EQ(notification_strings.message, u"Extracting src_file.zip\x2026");

  // Send the success progress status.
  status.bytes_transferred = 100;
  status.state = file_manager::io_task::State::kSuccess;
  notification_manager->HandleIOTaskProgress(status);

  // Notification should disappear.
  ASSERT_EQ(0u, GetNotificationCount());
}

TEST_F(SystemNotificationManagerTest, CancelButtonIOTask) {
  // The system notification only sees the IOTask ProgressStatus.
  file_manager::io_task::ProgressStatus status;
  status.task_id = 1;
  status.state = file_manager::io_task::State::kQueued;
  status.type = file_manager::io_task::OperationType::kCopy;
  status.total_bytes = 100;
  status.bytes_transferred = 0;
  auto src = CreateTestFile("volume/src_file.txt");
  status.sources.emplace_back(src, absl::nullopt);
  auto dst = CreateTestFile("volume/dest_dir/");
  status.destination_folder = dst;

  auto task = std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
      file_manager::io_task::OperationType::kCopy,
      std::vector<storage::FileSystemURL>({src}), dst, GetProfile(),
      file_system_context);

  // Send the copy begin/queued progress.
  const io_task::IOTaskId task_id = io_task_controller->Add(std::move(task));

  // Check: We have the 1 notification.
  ASSERT_EQ(1u, GetNotificationCount());

  // Click on the cancel button.
  notification_platform_bridge->ClickButtonIndexById("swa-file-operation-1",
                                                     /*button_index=*/0);

  // Notification should disappear.
  ASSERT_EQ(0u, GetNotificationCount());

  // The last status observed should be Cancelled.
  ASSERT_EQ(io_task::State::kCancelled, task_statuses[task_id].back());
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
  ASSERT_EQ(1u, notification_count);
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
  ASSERT_EQ(2u, notification_count);
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
  ASSERT_EQ(3u, notification_count);
  id = file_manager_private::ToString(sync_error.type);
  // Get the strings for the displayed notification.
  notification_strings =
      notification_platform_bridge->GetNotificationStringsById(id);
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, kGoogleDrive);
  EXPECT_EQ(notification_strings.message,
            u"There is not enough free space in your Google Drive to complete "
            u"the upload.");

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
  ASSERT_EQ(4u, notification_count);
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
  ASSERT_EQ(5u, notification_count);
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
  ASSERT_EQ(1u, notification_count);
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
  transfer_status.show_notification = true;
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
  ASSERT_EQ(1u, notification_count);
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
  ASSERT_EQ(0u, notification_count);
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
  ASSERT_EQ(1u, notification_count);
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
  ASSERT_EQ(0u, notification_count);
}

TEST_F(SystemNotificationManagerTest, SyncProgressIgnoreNotification) {
  // Setup a sync progress status.
  file_manager_private::FileTransferStatus transfer_status;
  transfer_status.transfer_state =
      file_manager_private::TRANSFER_STATE_IN_PROGRESS;
  transfer_status.num_total_jobs = 1;
  transfer_status.file_url =
      "filesystem:chrome://file-manager/drive/MyDrive-test-user/file.txt";
  transfer_status.processed = 25;
  transfer_status.total = 100;
  transfer_status.show_notification = true;
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
  ASSERT_EQ(1u, notification_count);

  // Update the transfer event to hide the notification.
  transfer_status.transfer_state =
      file_manager_private::TRANSFER_STATE_COMPLETED;
  transfer_status.num_total_jobs = 0;
  transfer_status.processed = 0;
  transfer_status.total = 0;
  transfer_status.show_notification = false;
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
  // Check: We have no notification.
  ASSERT_EQ(0u, notification_count);
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
  transfer_status.show_notification = true;
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
  ASSERT_EQ(1u, notification_count);
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
  pin_status.show_notification = true;
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
  ASSERT_EQ(1u, notification_count);
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
  ASSERT_EQ(0u, notification_count);

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
  ASSERT_EQ(1u, notification_count);
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
  ASSERT_EQ(0u, notification_count);
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
  pin_status.show_notification = true;
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
  ASSERT_EQ(1u, notification_count);
  // Get the strings for the displayed notification.
  TestNotificationStrings notification_strings =
      notification_platform_bridge->GetNotificationStringsById("swa-drive-pin");
  // Check: the expected strings match.
  EXPECT_EQ(notification_strings.title, u"Files");
  EXPECT_EQ(notification_strings.message, u"Making 10 files available offline");
}

}  // namespace file_manager
