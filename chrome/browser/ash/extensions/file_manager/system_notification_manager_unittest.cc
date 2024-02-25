// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/system_notification_manager.h"

#include <optional>
#include <set>
#include <string_view>

#include "ash/components/arc/arc_prefs.h"
#include "ash/webui/file_manager/url_constants.h"
#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/extensions/file_manager/device_event_router.h"
#include "chrome/browser/ash/extensions/file_manager/event_router.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/ash/policy/dlp/test/mock_files_policy_notification_manager.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_platform_bridge_delegator.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace file_manager {
namespace {

namespace fmp = extensions::api::file_manager_private;

using ash::DeviceType;
using ash::disks::Disk;
using base::BindOnce;
using base::FilePath;
using extensions::Event;
using message_center::NotificationDelegate;
using testing::IsEmpty;

using enum extensions::events::HistogramValue;

// Strings that would be seen on a notification.
struct Strings {
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
    Strings strings;
    notification_ids_.insert(notification.id());
    strings.title = notification.title();
    strings.message = notification.message();
    for (const message_center::ButtonInfo& button : notification.buttons()) {
      strings.buttons.push_back(button.title);
    }
    notifications_[notification.id()] = std::move(strings);
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

  // Gets the strings that would have been seen on the notification.
  Strings GetStrings(const std::string& notification_id) {
    const auto it = notifications_.find(notification_id);
    if (it == notifications_.end()) {
      LOG(ERROR) << "Cannot find notification " << notification_id;
      return {};
    }

    return it->second;
  }

  // Clicks a notification button.
  void ClickButton(const std::string& notification_id, int button_index) {
    const auto it = delegates_.find(notification_id);
    if (it == delegates_.end()) {
      LOG(ERROR) << "Cannot find delegate " << notification_id;
      return;
    }

    it->second->Click(button_index, std::nullopt);
  }

  // Clicks a notification body.
  void ClickNotification(const std::string& notification_id) {
    const auto it = delegates_.find(notification_id);
    if (it == delegates_.end()) {
      LOG(ERROR) << "Cannot find delegate " << notification_id;
      return;
    }

    it->second->Click(std::nullopt, std::nullopt);
  }

 private:
  // Notification IDs.
  std::set<std::string> notification_ids_;

  // Maps a notification ID to its displayed title and message.
  base::flat_map<std::string, Strings> notifications_;

  // Maps a notification ID to its delegate to verify click handlers.
  base::flat_map<std::string, scoped_refptr<NotificationDelegate>> delegates_;
};

// DeviceEventRouter implementation for testing.
class DeviceEventRouterImpl : public DeviceEventRouter {
 public:
  DeviceEventRouterImpl(SystemNotificationManager* notification_manager,
                        Profile* profile)
      : DeviceEventRouter(notification_manager) {}

  // DeviceEventRouter overrides.
  void OnDeviceEvent(fmp::DeviceEventType type,
                     const std::string& device_path,
                     const std::string& device_label) override {
    fmp::DeviceEvent event;
    event.type = type;
    event.device_path = device_path;
    event.device_label = device_label;

    system_notification_manager()->HandleDeviceEvent(event);
  }

  // DeviceEventRouter overrides.
  // Hard set to disabled for the ExternalStorageDisabled test to work.
  bool IsExternalStorageDisabled() override { return true; }
};

constexpr char kDevicePath[] = "/device/test";
constexpr char kMountPath[] = "/mnt/media/sda1";
std::u16string kRemovableDeviceTitle = u"Removable device detected";

}  // namespace

class SystemNotificationManagerTest
    : public io_task::IOTaskController::Observer,
      public ::testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("testing_profile");
    display_service_ = static_cast<NotificationDisplayServiceImpl*>(
        NotificationDisplayServiceFactory::GetForProfile(profile_));
    auto bridge =
        std::make_unique<TestNotificationPlatformBridgeDelegator>(profile_);
    bridge_ = bridge.get();
    display_service_->SetNotificationPlatformBridgeDelegatorForTesting(
        std::move(bridge));
    notification_manager_ =
        std::make_unique<SystemNotificationManager>(profile_);
    event_router_ = std::make_unique<DeviceEventRouterImpl>(
        notification_manager_.get(), profile_);

    VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile_,
        base::BindLambdaForTesting([this](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(std::make_unique<VolumeManager>(
              Profile::FromBrowserContext(context), nullptr, nullptr,
              &disk_mount_manager_, nullptr,
              VolumeManager::GetMtpStorageInfoCallback()));
        }));
    VolumeManager* const volume_manager = VolumeManager::Get(profile_);

    // SystemNotificationManager needs the IOTaskController to be able to cancel
    // the task.
    io_task_controller = volume_manager->io_task_controller();
    notification_manager_->SetIOTaskController(io_task_controller);
    io_task_controller->AddObserver(this);

    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    ASSERT_TRUE(dir_.GetPath().IsAbsolute());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());

    volume_manager->AddVolumeForTesting(
        CreateTestFile("volume/").path(), VOLUME_TYPE_DOWNLOADS_DIRECTORY,
        DeviceType::kUnknown, false /* read only */);
  }

  void TearDown() override {
    io_task_controller->RemoveObserver(this);
    profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
  }

  // IOTaskController::Observer override:
  // In production code the observer is EventRouter which forwards the status to
  // SystemNotificationManager.
  void OnIOTaskStatus(const io_task::ProgressStatus& status) override {
    task_statuses_[status.task_id].push_back(status.state);
    notification_manager_->HandleIOTaskProgress(status);
  }

  size_t GetNotificationCount() {
    display_service_->GetDisplayed(
        BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
                 weak_ptr_factory_.GetWeakPtr()));
    return notification_count_;
  }

  void GetNotificationsCallback(std::set<std::string> displayed_notifications,
                                bool supports_synchronization) {
    notification_count_ = displayed_notifications.size();
  }

  // Creates a file or directory to use in the test.
  storage::FileSystemURL CreateTestFile(const std::string& path) {
    const blink::StorageKey storage_key =
        blink::StorageKey::CreateFromStringForTesting(
            ash::file_manager::kChromeUIFileManagerURL);

    auto file_url = file_system_context_->CreateCrackedFileSystemURL(
        storage_key, storage::kFileSystemTypeTest,
        FilePath::FromUTF8Unsafe(path));

    if (base::EndsWith(path, "/")) {
      CHECK(base::File::FILE_OK ==
            storage::AsyncFileTestHelper::CreateDirectory(
                file_system_context_.get(), file_url));
    } else {
      CHECK(base::File::FILE_OK == storage::AsyncFileTestHelper::CreateFile(
                                       file_system_context_.get(), file_url));
    }

    return file_url;
  }

  content::BrowserTaskEnvironment task_environment_;
  ash::disks::FakeDiskMountManager disk_mount_manager_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  // Externally owned raw pointers:
  // profile_ is owned by TestingProfileManager.
  raw_ptr<TestingProfile> profile_;
  // notification_display_service is owned by NotificationDisplayServiceFactory.
  raw_ptr<NotificationDisplayServiceImpl, DanglingUntriaged> display_service_;
  std::unique_ptr<SystemNotificationManager> notification_manager_;
  std::unique_ptr<DeviceEventRouterImpl> event_router_;

  // Temporary directory used to test IOTask progress.
  base::ScopedTempDir dir_;

  size_t notification_count_ = 0;

  // notification_platform_bridge is owned by NotificationDisplayService.
  raw_ptr<TestNotificationPlatformBridgeDelegator, DanglingUntriaged> bridge_;

  // Used for tests with IOTask:
  raw_ptr<io_task::IOTaskController, DanglingUntriaged> io_task_controller;
  scoped_refptr<storage::FileSystemContext> file_system_context_;

  // Keep track of the task state transitions.
  base::flat_map<io_task::IOTaskId, std::vector<io_task::State>> task_statuses_;

  base::WeakPtrFactory<SystemNotificationManagerTest> weak_ptr_factory_{this};
};

TEST_F(SystemNotificationManagerTest, ExternalStorageDisabled) {
  base::HistogramTester histogram_tester;
  // Send a removable volume mounted event.
  event_router_->OnDeviceAdded(kDevicePath);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings("disabled");
  // Check: the expected strings match.
  std::u16string kExternalStorageDisabledMesssage =
      u"Sorry, your administrator has disabled external storage on your "
      u"account.";
  EXPECT_EQ(strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(strings.message, kExternalStorageDisabledMesssage);
  // Check that the correct UMA was emitted.
  histogram_tester.ExpectUniqueSample(
      kNotificationShowHistogramName,
      DeviceNotificationUmaType::DEVICE_EXTERNAL_STORAGE_DISABLED, 1);
}

constexpr char kDeviceLabel[] = "MyUSB";
std::u16string kFormatTitle = u"Format MyUSB";

TEST_F(SystemNotificationManagerTest, FormatStart) {
  base::HistogramTester histogram_tester;
  event_router_->OnFormatStarted(kDevicePath, kDeviceLabel,
                                 /*success=*/true);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings("format_start");
  // Check: the expected strings match.
  std::u16string kFormatStartMesssage = u"Formatting MyUSB\x2026";
  EXPECT_EQ(strings.title, kFormatTitle);
  EXPECT_EQ(strings.message, kFormatStartMesssage);
  histogram_tester.ExpectUniqueSample(kNotificationShowHistogramName,
                                      DeviceNotificationUmaType::FORMAT_START,
                                      1);
}

TEST_F(SystemNotificationManagerTest, FormatSuccess) {
  base::HistogramTester histogram_tester;
  event_router_->OnFormatCompleted(kDevicePath, kDeviceLabel,
                                   /*success=*/true);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings("format_success");
  // Check: the expected strings match.
  std::u16string kFormatSuccessMesssage = u"Formatted MyUSB";
  EXPECT_EQ(strings.title, kFormatTitle);
  EXPECT_EQ(strings.message, kFormatSuccessMesssage);
  histogram_tester.ExpectUniqueSample(kNotificationShowHistogramName,
                                      DeviceNotificationUmaType::FORMAT_SUCCESS,
                                      1);
}

TEST_F(SystemNotificationManagerTest, FormatFail) {
  base::HistogramTester histogram_tester;
  event_router_->OnFormatCompleted(kDevicePath, kDeviceLabel,
                                   /*success=*/false);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings("format_fail");
  // Check: the expected strings match.
  std::u16string kFormatFailedMesssage = u"Could not format MyUSB";
  EXPECT_EQ(strings.title, kFormatTitle);
  EXPECT_EQ(strings.message, kFormatFailedMesssage);
  histogram_tester.ExpectUniqueSample(kNotificationShowHistogramName,
                                      DeviceNotificationUmaType::FORMAT_FAIL,
                                      1);
}

constexpr char kPartitionLabel[] = "OEM";
std::u16string kPartitionTitle = u"Format OEM";

TEST_F(SystemNotificationManagerTest, PartitionFail) {
  base::HistogramTester histogram_tester;
  event_router_->OnPartitionCompleted(kDevicePath, kPartitionLabel,
                                      /*success=*/false);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings("partition_fail");
  // Check: the expected strings match.
  std::u16string kPartitionFailMesssage = u"Could not format OEM";
  EXPECT_EQ(strings.title, kPartitionTitle);
  EXPECT_EQ(strings.message, kPartitionFailMesssage);
  histogram_tester.ExpectUniqueSample(kNotificationShowHistogramName,
                                      DeviceNotificationUmaType::PARTITION_FAIL,
                                      1);
}

TEST_F(SystemNotificationManagerTest, RenameFail) {
  base::HistogramTester histogram_tester;
  event_router_->OnRenameCompleted(kDevicePath, kPartitionLabel,
                                   /*success=*/false);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings("rename_fail");
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, u"Renaming failed");
  EXPECT_EQ(strings.message, u"Aw, Snap! There was an error during renaming.");
  // Check that the correct UMA was emitted.
  histogram_tester.ExpectUniqueSample(kNotificationShowHistogramName,
                                      DeviceNotificationUmaType::RENAME_FAIL,
                                      1);
}

TEST_F(SystemNotificationManagerTest, DeviceHardUnplugged) {
  base::HistogramTester histogram_tester;
  const std::unique_ptr<const Disk> disk =
      Disk::Builder()
          .SetDevicePath(kDevicePath)
          .SetMountPath(kMountPath)
          .SetStorageDevicePath(kDevicePath)
          .SetIsReadOnlyHardware(false)
          .SetFileSystemType("vfat")
          .SetIsMounted(true)
          .Build();
  event_router_->OnDiskRemoved(*disk);
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings("hard_unplugged");
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, u"Whoa, there. Be careful.");
  EXPECT_EQ(strings.message,
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
      FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, DeviceType::kUSB,
      /*read_only=*/false, FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  fmp::MountCompletedEvent event;
  event.event_type = fmp::MountCompletedEventType::kMount;
  event.should_notify = true;
  event.status = fmp::MountError::kSuccess;
  notification_manager_->HandleMountCompletedEvent(event, *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings(kRemovableDeviceNotificationId);
  bridge_->ClickButton(kRemovableDeviceNotificationId, /*button_index=*/0);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(strings.message,
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
      FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, DeviceType::kUSB,
      /*read_only=*/true, FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  fmp::MountCompletedEvent event;
  event.event_type = fmp::MountCompletedEventType::kMount;
  event.should_notify = true;
  event.status = fmp::MountError::kSuccess;
  notification_manager_->HandleMountCompletedEvent(event, *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings(kRemovableDeviceNotificationId);
  bridge_->ClickButton(kRemovableDeviceNotificationId, /*button_index=*/0);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(strings.message,
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
  PrefService* const service = profile_->GetPrefs();
  service->SetBoolean(arc::prefs::kArcEnabled, true);
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, DeviceType::kUSB,
      /*read_only=*/false, FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  fmp::MountCompletedEvent event;
  event.event_type = fmp::MountCompletedEventType::kMount;
  event.should_notify = true;
  event.status = fmp::MountError::kSuccess;
  notification_manager_->HandleMountCompletedEvent(event, *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings(kRemovableDeviceNotificationId);
  bridge_->ClickButton(kRemovableDeviceNotificationId, /*button_index=*/0);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(strings.message,
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
  PrefService* const service = profile_->GetPrefs();
  service->SetBoolean(arc::prefs::kArcEnabled, true);
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, DeviceType::kUSB,
      /*read_only=*/false, FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  fmp::MountCompletedEvent event;
  event.event_type = fmp::MountCompletedEventType::kMount;
  event.should_notify = true;
  event.status = fmp::MountError::kSuccess;
  notification_manager_->HandleMountCompletedEvent(event, *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  bridge_->ClickButton(kRemovableDeviceNotificationId, /*button_index=*/1);
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
  PrefService* const service = profile_->GetPrefs();
  service->SetBoolean(arc::prefs::kArcEnabled, true);
  service->SetBoolean(arc::prefs::kArcHasAccessToRemovableMedia, true);
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, DeviceType::kUSB,
      /*read_only=*/false, FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  fmp::MountCompletedEvent event;
  event.event_type = fmp::MountCompletedEventType::kMount;
  event.should_notify = true;
  event.status = fmp::MountError::kSuccess;
  notification_manager_->HandleMountCompletedEvent(event, *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings(kRemovableDeviceNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(strings.message,
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
      FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, DeviceType::kUSB,
      /*read_only=*/false, FilePath(FILE_PATH_LITERAL("/device/test")), "",
      "FAT32"));
  fmp::MountCompletedEvent event;
  event.event_type = fmp::MountCompletedEventType::kMount;
  event.should_notify = true;
  event.status = fmp::MountError::kUnsupportedFilesystem;
  notification_manager_->HandleMountCompletedEvent(event, *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings(kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(
      strings.message,
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
      FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, DeviceType::kUSB,
      /*read_only=*/false, FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  fmp::MountCompletedEvent event;
  event.event_type = fmp::MountCompletedEventType::kMount;
  event.should_notify = true;
  event.status = fmp::MountError::kUnsupportedFilesystem;
  notification_manager_->HandleMountCompletedEvent(event, *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings(kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(strings.message,
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
      FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, DeviceType::kUSB,
      /*read_only=*/false, FilePath(FILE_PATH_LITERAL("/device/test")), "",
      "FAT32"));
  fmp::MountCompletedEvent event;
  event.event_type = fmp::MountCompletedEventType::kMount;
  event.should_notify = true;
  event.status = fmp::MountError::kSuccess;
  notification_manager_->HandleMountCompletedEvent(event, *volume1.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings(kRemovableDeviceNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(strings.message,
            u"Explore the device\x2019s content in the Files app.");
  // Build an unsupported file system volume and mount it on the same device.
  std::unique_ptr<Volume> volume2(Volume::CreateForTesting(
      FilePath(FILE_PATH_LITERAL("/mount/path2")),
      VolumeType::VOLUME_TYPE_TESTING, DeviceType::kUSB,
      /*read_only=*/false, FilePath(FILE_PATH_LITERAL("/device/test")), "",
      "unsupported"));
  event.status = fmp::MountError::kUnsupportedFilesystem;
  notification_manager_->HandleMountCompletedEvent(event, *volume2.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have two notifications.
  ASSERT_EQ(2u, notification_count_);
  // Get the strings for the displayed notification.
  strings = bridge_->GetStrings(kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(strings.message,
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
      FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, DeviceType::kUSB,
      /*read_only=*/false, FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "FAT32"));
  fmp::MountCompletedEvent event;
  event.event_type = fmp::MountCompletedEventType::kMount;
  event.should_notify = true;
  event.status = fmp::MountError::kSuccess;
  notification_manager_->HandleMountCompletedEvent(event, *volume1.get());
  // Ignore checking for the device navigation notification.
  // Build an unsupported file system volume and mount it on the same device.
  std::unique_ptr<Volume> volume2(Volume::CreateForTesting(
      FilePath(FILE_PATH_LITERAL("/mount/path2")),
      VolumeType::VOLUME_TYPE_TESTING, DeviceType::kUSB,
      /*read_only=*/false, FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "unsupported"));
  event.status = fmp::MountError::kUnsupportedFilesystem;
  notification_manager_->HandleMountCompletedEvent(event, *volume2.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have two notifications.
  ASSERT_EQ(2u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings(kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(strings.message,
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
      FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, DeviceType::kUSB,
      /*read_only=*/false, FilePath(FILE_PATH_LITERAL("/device/test")), "",
      "unknown"));
  fmp::MountCompletedEvent event;
  event.event_type = fmp::MountCompletedEventType::kMount;
  event.should_notify = true;
  event.status = fmp::MountError::kUnknownFilesystem;
  notification_manager_->HandleMountCompletedEvent(event, *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings(kDeviceFailNotificationId);
  bridge_->ClickButton(kDeviceFailNotificationId, /*button_index=*/0);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(strings.message,
            u"Sorry, your external storage device could not be recognized.");
  EXPECT_EQ(strings.buttons.size(), 1u);
  EXPECT_EQ(strings.buttons[0], u"Format this device");
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
      FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, DeviceType::kUSB,
      /*read_only=*/false, FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "unknown"));
  fmp::MountCompletedEvent event;
  event.event_type = fmp::MountCompletedEventType::kMount;
  event.should_notify = true;
  event.status = fmp::MountError::kUnknownFilesystem;
  notification_manager_->HandleMountCompletedEvent(event, *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings(kDeviceFailNotificationId);
  bridge_->ClickButton(kDeviceFailNotificationId, /*button_index=*/0);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(strings.message,
            u"Sorry, the device MyUSB could not be recognized.");
  EXPECT_EQ(strings.buttons.size(), 1u);
  EXPECT_EQ(strings.buttons[0], u"Format this device");
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
      FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, DeviceType::kUSB,
      /*read_only=*/true, FilePath(FILE_PATH_LITERAL("/device/test")), "",
      "unknown"));
  fmp::MountCompletedEvent event;
  event.event_type = fmp::MountCompletedEventType::kMount;
  event.should_notify = true;
  event.status = fmp::MountError::kUnknownFilesystem;
  notification_manager_->HandleMountCompletedEvent(event, *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings(kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(strings.message,
            u"Sorry, your external storage device could not be recognized.");
  // Device is read-only, expect no buttons present.
  EXPECT_EQ(strings.buttons.size(), 0u);
  histogram_tester.ExpectUniqueSample(
      kNotificationShowHistogramName,
      DeviceNotificationUmaType::DEVICE_FAIL_UNKNOWN_READONLY, 1);
}

// The named version of the read only device fail unknown notification is
// generated when the device includes a device label.
TEST_F(SystemNotificationManagerTest, DeviceFailUnknownReadOnlyNamed) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      FilePath(FILE_PATH_LITERAL("/mount/path1")),
      VolumeType::VOLUME_TYPE_TESTING, DeviceType::kUSB,
      /*read_only=*/true, FilePath(FILE_PATH_LITERAL("/device/test")),
      kDeviceLabel, "unknown"));
  fmp::MountCompletedEvent event;
  event.event_type = fmp::MountCompletedEventType::kMount;
  event.should_notify = true;
  event.status = fmp::MountError::kUnknownFilesystem;
  notification_manager_->HandleMountCompletedEvent(event, *volume.get());
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings(kDeviceFailNotificationId);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kRemovableDeviceTitle);
  EXPECT_EQ(strings.message,
            u"Sorry, the device MyUSB could not be recognized.");
  histogram_tester.ExpectUniqueSample(
      kNotificationShowHistogramName,
      DeviceNotificationUmaType::DEVICE_FAIL_UNKNOWN_READONLY, 1);
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
                              std::nullopt);
  status.SetDestinationFolder(CreateTestFile("volume/dest_dir/"));

  // Send the copy begin/queued progress.
  notification_manager_->HandleIOTaskProgress(status);

  // Check: We have the 1 notification.
  ASSERT_EQ(1u, GetNotificationCount());

  Strings strings = bridge_->GetStrings("swa-file-operation-1");

  // Check: the expected strings match.
  EXPECT_EQ(strings.title, u"Files");
  EXPECT_EQ(strings.message, u"Copying src_file.txt\x2026");

  // Send the copy progress.
  status.bytes_transferred = 30;
  status.state = file_manager::io_task::State::kInProgress;
  notification_manager_->HandleIOTaskProgress(status);

  // Check: We have the same notification.
  ASSERT_EQ(1u, GetNotificationCount());
  strings = bridge_->GetStrings("swa-file-operation-1");
  EXPECT_EQ(strings.title, u"Files");
  EXPECT_EQ(strings.message, u"Copying src_file.txt\x2026");

  // Send the success progress status.
  status.bytes_transferred = 100;
  status.state = file_manager::io_task::State::kSuccess;
  notification_manager_->HandleIOTaskProgress(status);

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
                              std::nullopt);
  status.SetDestinationFolder(CreateTestFile("volume/src_file/"));

  // Send the copy begin/queued progress.
  notification_manager_->HandleIOTaskProgress(status);

  // Check: We have the 1 notification.
  ASSERT_EQ(1u, GetNotificationCount());

  Strings strings = bridge_->GetStrings("swa-file-operation-1");

  // Check: the expected strings match.
  EXPECT_EQ(strings.title, u"Files");
  EXPECT_EQ(strings.message, u"Extracting src_file.zip\x2026");

  // Send the copy progress.
  status.bytes_transferred = 30;
  status.state = file_manager::io_task::State::kInProgress;
  notification_manager_->HandleIOTaskProgress(status);

  // Check: We have the same notification.
  ASSERT_EQ(1u, GetNotificationCount());
  strings = bridge_->GetStrings("swa-file-operation-1");
  EXPECT_EQ(strings.title, u"Files");
  EXPECT_EQ(strings.message, u"Extracting src_file.zip\x2026");

  // Send the success progress status.
  status.bytes_transferred = 100;
  status.state = file_manager::io_task::State::kSuccess;
  notification_manager_->HandleIOTaskProgress(status);

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
  status.sources.emplace_back(src, std::nullopt);
  auto dst = CreateTestFile("volume/dest_dir/");
  status.SetDestinationFolder(dst);

  auto task = std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
      file_manager::io_task::OperationType::kCopy,
      std::vector<storage::FileSystemURL>({src}), dst, profile_,
      file_system_context_);

  // Send the copy begin/queued progress.
  const io_task::IOTaskId task_id = io_task_controller->Add(std::move(task));

  // Check: We have the 1 notification.
  ASSERT_EQ(1u, GetNotificationCount());

  // Click on the cancel button.
  bridge_->ClickButton("swa-file-operation-1", /*button_index=*/0);

  // Notification should disappear.
  ASSERT_EQ(0u, GetNotificationCount());

  // The last status observed should be Cancelled.
  ASSERT_EQ(io_task::State::kCancelled, task_statuses_[task_id].back());
}

TEST_F(SystemNotificationManagerTest, HandleIOTaskProgressPolicyScanning) {
  // The system notification only sees the IOTask ProgressStatus.
  file_manager::io_task::ProgressStatus status;
  status.task_id = 1;
  status.state = file_manager::io_task::State::kScanning;
  status.type = file_manager::io_task::OperationType::kCopy;
  status.total_bytes = 100;
  status.bytes_transferred = 123;
  status.sources_scanned = 1;
  status.sources.emplace_back(CreateTestFile("volume/src_file.txt"),
                              std::nullopt);
  status.SetDestinationFolder(CreateTestFile("volume/dest_dir/"));

  // Send the scanning progress.
  notification_manager_->HandleIOTaskProgress(status);

  // Check: We have the 1 notification.
  ASSERT_EQ(1u, GetNotificationCount());

  Strings strings = bridge_->GetStrings("swa-file-operation-1");

  // Check: the expected strings match.
  EXPECT_EQ(strings.title, u"Files");
  EXPECT_EQ(strings.message,
            l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SCANNING_LABEL));

  // Send the success progress status.
  status.bytes_transferred = 100;
  status.state = file_manager::io_task::State::kSuccess;
  notification_manager_->HandleIOTaskProgress(status);

  // Notification should disappear.
  ASSERT_EQ(0u, GetNotificationCount());
}

std::u16string kGoogleDrive = u"Google Drive";

// Tests the bulk-pinning notifications.
TEST_F(SystemNotificationManagerTest, BulkPinningNotification) {
  using List = base::Value::List;
  const std::string_view event_name = "unused-event-name";

  // Event with no args should be ignored.
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_BULK_PIN_PROGRESS, event_name, List()));

  // There should be no notification.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, notification_count_);

  file_manager_private::BulkPinProgress progress;
  progress.should_pin = true;

  // Handle an unparsable event.
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_BULK_PIN_PROGRESS, event_name,
            List().Append(progress.ToValue())));

  // There should be no notification.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, notification_count_);

  // Not enough space without going through syncing phase.
  EXPECT_FALSE(progress.emptied_queue);
  progress.stage = fmp::BulkPinStage::kNotEnoughSpace;
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_BULK_PIN_PROGRESS, event_name,
            List().Append(progress.ToValue())));

  // There should be no notification.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, notification_count_);

  // Listing files.
  progress.stage = fmp::BulkPinStage::kListingFiles;
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_BULK_PIN_PROGRESS, event_name,
            List().Append(progress.ToValue())));

  // There should be no notification.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, notification_count_);

  // Not enough space after listing phase.
  progress.stage = fmp::BulkPinStage::kNotEnoughSpace;
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_BULK_PIN_PROGRESS, event_name,
            List().Append(progress.ToValue())));

  // There should be one notification.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(1u, notification_count_);

  // Get the strings for the displayed notification.
  const std::string notification_id = "drive-bulk-pinning-error";
  {
    const Strings strings = bridge_->GetStrings(notification_id);
    EXPECT_EQ(strings.title, u"Couldn’t finish setting up file sync");
    EXPECT_EQ(strings.message,
              u"There isn’t enough storage space to sync all of your files");
    EXPECT_THAT(strings.buttons, IsEmpty());
  }

  // Click the notification body.
  bridge_->ClickNotification(notification_id);

  // The notification should have been closed.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, notification_count_);

  // Syncing.
  progress.stage = fmp::BulkPinStage::kSyncing;
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_BULK_PIN_PROGRESS, event_name,
            List().Append(progress.ToValue())));

  // There should be no notification.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, notification_count_);

  // Not enough space after syncing phase.
  progress.stage = fmp::BulkPinStage::kNotEnoughSpace;
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_BULK_PIN_PROGRESS, event_name,
            List().Append(progress.ToValue())));

  // There should be one notification.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(1u, notification_count_);

  // Get the strings for the displayed notification.
  {
    const Strings strings = bridge_->GetStrings(notification_id);
    EXPECT_EQ(strings.title, u"Couldn’t finish setting up file sync");
    EXPECT_EQ(strings.message,
              u"There isn’t enough storage space to sync all of your files");
    EXPECT_THAT(strings.buttons, IsEmpty());
  }

  // Click the notification body.
  bridge_->ClickNotification(notification_id);

  // The notification should have been closed.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, notification_count_);

  // Not enough space without going through syncing phase.
  progress.stage = fmp::BulkPinStage::kNotEnoughSpace;
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_BULK_PIN_PROGRESS, event_name,
            List().Append(progress.ToValue())));

  // There should be no notification.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, notification_count_);

  // Back to syncing stage. Pretend that everything has been synced.
  progress.stage = fmp::BulkPinStage::kSyncing;
  progress.emptied_queue = true;
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_BULK_PIN_PROGRESS, event_name,
            List().Append(progress.ToValue())));

  // There should be no notification.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, notification_count_);

  // Not enough space after syncing phase.
  progress.stage = fmp::BulkPinStage::kNotEnoughSpace;
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_BULK_PIN_PROGRESS, event_name,
            List().Append(progress.ToValue())));

  // There should be one notification.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(1u, notification_count_);

  // Get the strings for the displayed notification.
  {
    const Strings strings = bridge_->GetStrings(notification_id);
    EXPECT_EQ(strings.title, u"File sync turned off");
    EXPECT_EQ(strings.message,
              u"There isn’t enough storage space to continue syncing files");
    EXPECT_THAT(strings.buttons, IsEmpty());
  }

  // Click the notification body.
  bridge_->ClickNotification(notification_id);

  // The notification should have been closed.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, notification_count_);

  // Back to syncing stage.
  progress.stage = fmp::BulkPinStage::kSyncing;
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_BULK_PIN_PROGRESS, event_name,
            List().Append(progress.ToValue())));

  // There should be no notification.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, notification_count_);

  // Error during syncing phase.
  progress.stage = fmp::BulkPinStage::kCannotGetFreeSpace;
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_BULK_PIN_PROGRESS, event_name,
            List().Append(progress.ToValue())));

  // There should be one notification.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(1u, notification_count_);

  // Get the strings for the displayed notification.
  {
    const Strings strings = bridge_->GetStrings(notification_id);
    EXPECT_EQ(strings.title, u"File sync turned off");
    EXPECT_EQ(strings.message, u"Something went wrong.");
    EXPECT_THAT(strings.buttons, IsEmpty());
  }

  // Click the notification body.
  bridge_->ClickNotification(notification_id);

  // The notification should have been closed.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, notification_count_);

  // Listing files without having the intent of pinning them.
  progress.stage = fmp::BulkPinStage::kListingFiles;
  progress.should_pin = false;
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_BULK_PIN_PROGRESS, event_name,
            List().Append(progress.ToValue())));

  // There should be no notification.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, notification_count_);

  // Not enough space after listing phase.
  progress.stage = fmp::BulkPinStage::kNotEnoughSpace;
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_BULK_PIN_PROGRESS, event_name,
            List().Append(progress.ToValue())));

  // There should be no notification.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, notification_count_);
}

// Tests all the various error notifications.
TEST_F(SystemNotificationManagerTest, Errors) {
  // Build a Drive sync error object.
  fmp::DriveSyncErrorEvent sync_error;
  sync_error.type = fmp::DriveSyncErrorType::kDeleteWithoutPermission;
  sync_error.file_url = "drivefs://fake.txt";

  // Send the delete without permission sync error event.
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_DRIVE_SYNC_ERROR,
            file_manager_private::OnDriveSyncError::kEventName,
            file_manager_private::OnDriveSyncError::Create(sync_error)));
  // Get the number of notifications from the NotificationDisplayService.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  const char* id = ToString(sync_error.type);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings(id);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kGoogleDrive);
  EXPECT_EQ(strings.message,
            u"\"fake.txt\" has been shared with you. You cannot delete it "
            u"because you do not own it.");

  // Setup for the service unavailable error.
  sync_error.type = fmp::DriveSyncErrorType::kServiceUnavailable;

  // Send the service unavailable sync error event.
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_DRIVE_SYNC_ERROR,
            file_manager_private::OnDriveSyncError::kEventName,
            file_manager_private::OnDriveSyncError::Create(sync_error)));
  // Get the number of notifications from the NotificationDisplayService.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have two notifications.
  ASSERT_EQ(2u, notification_count_);
  id = ToString(sync_error.type);
  // Get the strings for the displayed notification.
  strings = bridge_->GetStrings(id);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kGoogleDrive);
  EXPECT_EQ(strings.message,
            u"Google Drive is not available right now. Uploading will "
            u"automatically restart once Google Drive is back.");

  // Setup for the no server space error.
  sync_error.type = fmp::DriveSyncErrorType::kNoServerSpace;

  // Send the service unavailable sync error event.
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_DRIVE_SYNC_ERROR,
            file_manager_private::OnDriveSyncError::kEventName,
            file_manager_private::OnDriveSyncError::Create(sync_error)));
  // Get the number of notifications from the NotificationDisplayService.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have three notifications.
  ASSERT_EQ(3u, notification_count_);
  id = ToString(sync_error.type);
  // Get the strings for the displayed notification.
  strings = bridge_->GetStrings(id);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kGoogleDrive);
  EXPECT_EQ(strings.message,
            u"There is not enough free space in your Google Drive to complete "
            u"the upload.");

  // Setup for the no local space error.
  sync_error.type = fmp::DriveSyncErrorType::kNoLocalSpace;

  // Send the service unavailable sync error event.
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_DRIVE_SYNC_ERROR,
            file_manager_private::OnDriveSyncError::kEventName,
            file_manager_private::OnDriveSyncError::Create(sync_error)));
  // Get the number of notifications from the NotificationDisplayService.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have four notifications.
  ASSERT_EQ(4u, notification_count_);
  id = ToString(sync_error.type);
  // Get the strings for the displayed notification.
  strings = bridge_->GetStrings(id);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kGoogleDrive);
  EXPECT_EQ(strings.message, u"You have run out of space");

  // Setup for the miscellaneous sync error.
  sync_error.type = fmp::DriveSyncErrorType::kMisc;

  // Send the service unavailable sync error event.
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_DRIVE_SYNC_ERROR,
            file_manager_private::OnDriveSyncError::kEventName,
            file_manager_private::OnDriveSyncError::Create(sync_error)));
  // Get the number of notifications from the NotificationDisplayService.
  display_service_->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have five notifications.
  ASSERT_EQ(5u, notification_count_);
  id = ToString(sync_error.type);
  // Get the strings for the displayed notification.
  strings = bridge_->GetStrings(id);
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kGoogleDrive);
  EXPECT_EQ(strings.message,
            u"Google Drive was unable to sync \"fake.txt\" right now. Google "
            u"Drive will try again later.");
}

// Tests the generation of the DriveFS enable offline notification.
// This is triggered when users make files available offline and the
// Google Drive offline setting at https://drive.google.com isn't enabled.
TEST_F(SystemNotificationManagerTest, EnableDocsOffline) {
  file_manager_private::DriveConfirmDialogEvent drive_event;
  drive_event.type =
      file_manager_private::DriveConfirmDialogType::kEnableDocsOffline;
  drive_event.file_url = "drivefs://fake";
  notification_manager_->HandleEvent(
      Event(FILE_MANAGER_PRIVATE_ON_DRIVE_CONFIRM_DIALOG,
            file_manager_private::OnDriveConfirmDialog::kEventName,
            file_manager_private::OnDriveConfirmDialog::Create(drive_event)));
  // Get the number of notifications from the NotificationDisplayService.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->GetDisplayed(
      BindOnce(&SystemNotificationManagerTest::GetNotificationsCallback,
               weak_ptr_factory_.GetWeakPtr()));
  // Check: We have one notification.
  ASSERT_EQ(1u, notification_count_);
  // Get the strings for the displayed notification.
  Strings strings = bridge_->GetStrings("swa-drive-confirm-dialog");
  // Check: the expected strings match.
  EXPECT_EQ(strings.title, kGoogleDrive);
  EXPECT_EQ(strings.message,
            u"Enable Google Docs Offline to make Docs, Sheets and Slides "
            u"available offline.");
}

class SystemNotificationManagerPolicyTest
    : public SystemNotificationManagerTest {
 public:
  void SetUp() override {
    SystemNotificationManagerTest::SetUp();
    // Set FilesPolicyNotificationManager.
    policy::FilesPolicyNotificationManagerFactory::GetInstance()
        ->SetTestingFactory(
            profile_.get(),
            base::BindRepeating(&SystemNotificationManagerPolicyTest::
                                    SetFilesPolicyNotificationManager,
                                base::Unretained(this)));
    ASSERT_TRUE(
        policy::FilesPolicyNotificationManagerFactory::GetForBrowserContext(
            profile_.get()));
    ASSERT_TRUE(fpnm_);
  }

  policy::MockFilesPolicyNotificationManager* fpnm() { return fpnm_; }

 private:
  std::unique_ptr<KeyedService> SetFilesPolicyNotificationManager(
      content::BrowserContext* context) {
    auto fpnm = std::make_unique<policy::MockFilesPolicyNotificationManager>(
        profile_.get());
    fpnm_ = fpnm.get();

    return fpnm;
  }

  raw_ptr<policy::MockFilesPolicyNotificationManager, DanglingUntriaged> fpnm_ =
      nullptr;
};

TEST_F(SystemNotificationManagerPolicyTest, HandleIOTaskProgressWarning) {
  // The system notification only sees the IOTask ProgressStatus.
  file_manager::io_task::ProgressStatus status;
  status.task_id = 1;
  status.state = file_manager::io_task::State::kQueued;
  status.type = file_manager::io_task::OperationType::kCopy;
  status.total_bytes = 100;
  status.bytes_transferred = 0;
  status.sources.emplace_back(CreateTestFile("volume/src_file1.txt"),
                              std::nullopt);
  status.sources.emplace_back(CreateTestFile("volume/src_file2.txt"),
                              std::nullopt);
  status.SetDestinationFolder(CreateTestFile("volume/dest_dir/"));

  // Send the copy begin/queued progress.
  notification_manager_->HandleIOTaskProgress(status);

  // Check: We have the 1 notification.
  ASSERT_EQ(1u, GetNotificationCount());

  Strings strings = bridge_->GetStrings("swa-file-operation-1");

  // Check: the expected strings match.
  EXPECT_EQ(strings.title, u"Files");
  EXPECT_EQ(strings.message, u"Copying 2 items\x2026");

  // Set the status to warning.
  status.state = file_manager::io_task::State::kPaused;
  status.pause_params.policy_params =
      io_task::PolicyPauseParams(policy::Policy::kDlp);
  // The manager delegates to FPNM to show the notification.
  EXPECT_CALL(
      *fpnm(),
      ShowFilesPolicyNotification(
          "swa-file-operation-1",
          AllOf(testing::Field(&file_manager::io_task::ProgressStatus::task_id,
                               status.task_id),
                testing::Field(&file_manager::io_task::ProgressStatus::state,
                               file_manager::io_task::State::kPaused),
                testing::Field(
                    &file_manager::io_task::ProgressStatus::pause_params,
                    status.pause_params))))
      .Times(1);
  notification_manager_->HandleIOTaskProgress(status);
}

TEST_F(SystemNotificationManagerPolicyTest, HandleIOTaskProgressPolicyError) {
  // The system notification only sees the IOTask ProgressStatus.
  file_manager::io_task::ProgressStatus status;
  status.task_id = 1;
  status.state = file_manager::io_task::State::kQueued;
  status.type = file_manager::io_task::OperationType::kCopy;
  status.total_bytes = 100;
  status.bytes_transferred = 0;
  status.sources.emplace_back(CreateTestFile("volume/src_file.txt"),
                              std::nullopt);
  status.SetDestinationFolder(CreateTestFile("volume/dest_dir/"));

  // Send the copy begin/queued progress.
  notification_manager_->HandleIOTaskProgress(status);

  // Check: We have the 1 notification.
  ASSERT_EQ(1u, GetNotificationCount());

  Strings strings = bridge_->GetStrings("swa-file-operation-1");

  // Check: the expected strings match.
  EXPECT_EQ(strings.title, u"Files");
  EXPECT_EQ(strings.message, u"Copying src_file.txt\x2026");

  // Set the security error value.
  status.state = file_manager::io_task::State::kError;
  status.policy_error.emplace(
      file_manager::io_task::PolicyErrorType::kEnterpriseConnectors,
      /*blocked_files=*/1);
  // The manager delegates to FPNM to show the notification.
  EXPECT_CALL(
      *fpnm(),
      ShowFilesPolicyNotification(
          "swa-file-operation-1",
          AllOf(testing::Field(&file_manager::io_task::ProgressStatus::task_id,
                               status.task_id),
                testing::Field(&file_manager::io_task::ProgressStatus::state,
                               file_manager::io_task::State::kError),
                testing::Field(
                    &file_manager::io_task::ProgressStatus::policy_error,
                    status.policy_error))))
      .Times(1);

  notification_manager_->HandleIOTaskProgress(status);
}

}  // namespace file_manager
