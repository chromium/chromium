// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/volume_manager.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_file_system_instance.h"
#include "ash/constants/ash_switches.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_running_on_chromeos.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/ash/arc/fileapi/arc_media_view_util.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume.h"
#include "chrome/browser/ash/file_manager/volume_manager_observer.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "chromeos/components/disks/disks_prefs.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/storage_monitor/storage_info.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "services/device/public/mojom/mtp_storage_info.mojom.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace {

using ::ash::MountError;
using ::ash::MountType;
using ::ash::disks::Disk;
using ::ash::disks::DiskMountManager;
using ::ash::disks::FakeDiskMountManager;
using base::FilePath;

std::vector<std::string> arc_volume_ids = {
    arc::kImagesRootId, arc::kVideosRootId, arc::kAudioRootId,
    arc::kDocumentsRootId, "android_files:0"};

class LoggingObserver : public VolumeManagerObserver {
 public:
  struct Event {
    enum EventType {
      DISK_ADDED,
      DISK_REMOVED,
      DEVICE_ADDED,
      DEVICE_REMOVED,
      VOLUME_MOUNTED,
      VOLUME_UNMOUNTED,
      FORMAT_STARTED,
      FORMAT_COMPLETED,
      PARTITION_STARTED,
      PARTITION_COMPLETED,
      RENAME_STARTED,
      RENAME_COMPLETED
    } type;

    // Available on DEVICE_ADDED, DEVICE_REMOVED, VOLUME_MOUNTED,
    // VOLUME_UNMOUNTED, FORMAT_STARTED, FORMAT_COMPLETED. PARTITION_STARTED,
    // PARTITION_COMPLETED.
    std::string device_path;

    // Available on FORMAT_STARTED, FORMAT_COMPLETED, RENAME_STARTED and
    // RENAME_COMPLETED, PARTITION_STARTED, PARTITION_COMPLETED.
    std::string device_label;

    // Available on VOLUME_MOUNTED and VOLUME_UNMOUNTED.
    std::string volume_id;

    // Available on DISK_ADDED.
    bool mounting;

    // Available on VOLUME_MOUNTED and VOLUME_UNMOUNTED.
    ash::MountError mount_error;

    // Available on FORMAT_STARTED and FORMAT_COMPLETED, PARTITION_STARTED,
    // PARTITION_COMPLETED.
    bool success;
  };

  LoggingObserver() = default;

  LoggingObserver(const LoggingObserver&) = delete;
  LoggingObserver& operator=(const LoggingObserver&) = delete;

  ~LoggingObserver() override = default;

  const std::vector<Event>& events() const { return events_; }

  // VolumeManagerObserver overrides.
  void OnDiskAdded(const Disk& disk, bool mounting) override {
    Event event;
    event.type = Event::DISK_ADDED;
    event.device_path = disk.device_path();  // Keep only device_path.
    event.mounting = mounting;
    events_.push_back(event);
  }

  void OnDiskRemoved(const Disk& disk) override {
    Event event;
    event.type = Event::DISK_REMOVED;
    event.device_path = disk.device_path();  // Keep only device_path.
    events_.push_back(event);
  }

  void OnDeviceAdded(const std::string& device_path) override {
    Event event;
    event.type = Event::DEVICE_ADDED;
    event.device_path = device_path;
    events_.push_back(event);
  }

  void OnDeviceRemoved(const std::string& device_path) override {
    Event event;
    event.type = Event::DEVICE_REMOVED;
    event.device_path = device_path;
    events_.push_back(event);
  }

  void OnVolumeMounted(ash::MountError error_code,
                       const Volume& volume) override {
    Event event;
    event.type = Event::VOLUME_MOUNTED;
    event.device_path = volume.source_path().AsUTF8Unsafe();
    event.volume_id = volume.volume_id();
    event.mount_error = error_code;
    events_.push_back(event);
  }

  void OnVolumeUnmounted(ash::MountError error_code,
                         const Volume& volume) override {
    Event event;
    event.type = Event::VOLUME_UNMOUNTED;
    event.device_path = volume.source_path().AsUTF8Unsafe();
    event.volume_id = volume.volume_id();
    event.mount_error = error_code;
    events_.push_back(event);
  }

  void OnFormatStarted(const std::string& device_path,
                       const std::string& device_label,
                       bool success) override {
    Event event;
    event.type = Event::FORMAT_STARTED;
    event.device_path = device_path;
    event.device_label = device_label;
    event.success = success;
    events_.push_back(event);
  }

  void OnFormatCompleted(const std::string& device_path,
                         const std::string& device_label,
                         bool success) override {
    Event event;
    event.type = Event::FORMAT_COMPLETED;
    event.device_path = device_path;
    event.device_label = device_label;
    event.success = success;
    events_.push_back(event);
  }

  void OnPartitionStarted(const std::string& device_path,
                          const std::string& device_label,
                          bool success) override {
    Event event;
    event.type = Event::PARTITION_STARTED;
    event.device_path = device_path;
    event.device_label = device_label;
    event.success = success;
    events_.push_back(event);
  }

  void OnPartitionCompleted(const std::string& device_path,
                            const std::string& device_label,
                            bool success) override {
    Event event;
    event.type = Event::PARTITION_COMPLETED;
    event.device_path = device_path;
    event.device_label = device_label;
    event.success = success;
    events_.push_back(event);
  }

  void OnRenameStarted(const std::string& device_path,
                       const std::string& device_label,
                       bool success) override {
    Event event;
    event.type = Event::RENAME_STARTED;
    event.device_path = device_path;
    event.device_label = device_label;
    event.success = success;
    events_.push_back(event);
  }

  void OnRenameCompleted(const std::string& device_path,
                         const std::string& device_label,
                         bool success) override {
    Event event;
    event.type = Event::RENAME_COMPLETED;
    event.device_path = device_path;
    event.device_label = device_label;
    event.success = success;
    events_.push_back(event);
  }

  void OnShutdownStart(VolumeManager* volume_manager) override {
    // Each test should remove its observer manually, so that they're all gone
    // by the time VolumeManager shuts down, and this handler is never reached.
    // In fact, it's more likely for UAF crash to happen before this code is
    // reached.
    NOTREACHED_IN_MIGRATION();
  }

 private:
  std::vector<Event> events_;
};

}  // namespace

std::unique_ptr<KeyedService> CreateFileSystemOperationRunnerForTesting(
    content::BrowserContext* context) {
  return arc::ArcFileSystemOperationRunner::CreateForTesting(
      context, arc::ArcServiceManager::Get()->arc_bridge_service());
}

class VolumeManagerTest : public testing::Test {
 protected:
  // Helper class that contains per-profile objects.
  class ProfileEnvironment {
   public:
    ProfileEnvironment(std::unique_ptr<TestingProfile> profile,
                       DiskMountManager* disk_manager)
        : profile_(std::move(profile)),
          extension_registry_(
              std::make_unique<extensions::ExtensionRegistry>(profile_.get())),
          file_system_provider_service_(
              std::make_unique<ash::file_system_provider::Service>(
                  profile_.get(),
                  extension_registry_.get())),
          drive_integration_service_(
              std::make_unique<drive::DriveIntegrationService>(
                  profile_.get(),
                  std::string(),
                  base::FilePath())),
          volume_manager_(std::make_unique<VolumeManager>(
              profile_.get(),
              drive_integration_service_.get(),  // DriveIntegrationService
              chromeos::PowerManagerClient::Get(),
              disk_manager,
              file_system_provider_service_.get(),
              base::BindRepeating(&ProfileEnvironment::GetFakeMtpStorageInfo,
                                  base::Unretained(this)))) {}

    ~ProfileEnvironment() {
      // In production, KeyedServices have Shutdown() called before destruction.
      volume_manager_->Shutdown();
      drive_integration_service_->Shutdown();
      file_system_provider_service_->Shutdown();
      extension_registry_->Shutdown();
    }

    Profile* profile() const { return profile_.get(); }
    VolumeManager* volume_manager() const { return volume_manager_.get(); }

   private:
    void GetFakeMtpStorageInfo(
        const std::string& storage_name,
        device::mojom::MtpManager::GetStorageInfoCallback callback) {
      std::move(callback).Run(device::mojom::MtpStorageInfo::New());
    }

    std::unique_ptr<TestingProfile> profile_;
    std::unique_ptr<extensions::ExtensionRegistry> extension_registry_;
    std::unique_ptr<ash::file_system_provider::Service>
        file_system_provider_service_;
    std::unique_ptr<drive::DriveIntegrationService> drive_integration_service_;
    std::unique_ptr<VolumeManager> volume_manager_;
  };

  void SetUp() override {
    // Some test cases exercises the "MyFiles" directory.
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kUseMyFilesInUserDataDirForTesting);

    chromeos::PowerManagerClient::InitializeFake();
    disk_mount_manager_ = std::make_unique<FakeDiskMountManager>();
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

    primary_profile_ = std::make_unique<ProfileEnvironment>(
        AddLoggedInUser(AccountId::FromUserEmail("primary@test")),
        disk_mount_manager_.get());
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
    primary_profile_.reset();

    disk_mount_manager_.reset();
    chromeos::PowerManagerClient::Shutdown();

    // ExternalMountPoints instance for the system is global singleton,
    // so some states can be leaked to another test. Revoke all of them
    // explicitly.
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  }

  virtual std::unique_ptr<TestingProfile> AddLoggedInUser(
      const AccountId& account_id) {
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    std::unique_ptr<TestingProfile> profile =
        std::make_unique<TestingProfile>();
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        fake_user_manager_->FindUserAndModify(account_id), profile.get());
    return profile;
  }

  // Accessors to the primary profile.
  Profile* profile() const { return primary_profile_->profile(); }
  VolumeManager* volume_manager() const {
    return primary_profile_->volume_manager();
  }

  base::test::ScopedCommandLine scoped_command_line_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FakeDiskMountManager> disk_mount_manager_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<ProfileEnvironment> primary_profile_;
};

TEST(VolumeTest, CreateForRemovable) {
  const std::unique_ptr<Volume> volume = Volume::CreateForRemovable(
      {"/source/path", "/mount/path", MountType::kDevice,
       MountError::kUnknownFilesystem},
      nullptr);
  ASSERT_TRUE(volume);
  EXPECT_EQ(volume->source_path(), FilePath("/source/path"));
  EXPECT_EQ(volume->mount_path(), FilePath("/mount/path"));
  EXPECT_EQ(volume->type(), VOLUME_TYPE_REMOVABLE_DISK_PARTITION);
  EXPECT_EQ(volume->mount_condition(), MountError::kUnknownFilesystem);
  EXPECT_EQ(volume->volume_id(), "removable:path");
  EXPECT_EQ(volume->volume_label(), "path");
  EXPECT_EQ(volume->source(), SOURCE_DEVICE);
  EXPECT_FALSE(volume->is_read_only());
  EXPECT_TRUE(volume->watchable());
}

TEST_F(VolumeManagerTest, OnDriveFileSystemMountAndUnmount) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnFileSystemMounted();

  ASSERT_EQ(1U, observer.events().size());
  LoggingObserver::Event event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::VOLUME_MOUNTED, event.type);
  EXPECT_EQ(drive::DriveIntegrationServiceFactory::GetForProfile(profile())
                ->GetMountPointPath()
                .AsUTF8Unsafe(),
            event.device_path);
  EXPECT_EQ(ash::MountError::kSuccess, event.mount_error);

  volume_manager()->OnFileSystemBeingUnmounted();

  ASSERT_EQ(2U, observer.events().size());
  event = observer.events()[1];
  EXPECT_EQ(LoggingObserver::Event::VOLUME_UNMOUNTED, event.type);
  EXPECT_EQ(drive::DriveIntegrationServiceFactory::GetForProfile(profile())
                ->GetMountPointPath()
                .AsUTF8Unsafe(),
            event.device_path);
  EXPECT_EQ(ash::MountError::kSuccess, event.mount_error);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnDriveFileSystemUnmountWithoutMount) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);
  volume_manager()->OnFileSystemBeingUnmounted();

  // Unmount event for non-mounted volume is not reported.
  ASSERT_EQ(0U, observer.events().size());
  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnBootDeviceDiskEvent) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  std::unique_ptr<const Disk> disk =
      Disk::Builder().SetDevicePath("device1").SetOnBootDevice(true).Build();

  volume_manager()->OnBootDeviceDiskEvent(DiskMountManager::DISK_ADDED, *disk);
  EXPECT_EQ(0U, observer.events().size());

  volume_manager()->OnBootDeviceDiskEvent(DiskMountManager::DISK_REMOVED,
                                          *disk);
  EXPECT_EQ(0U, observer.events().size());

  volume_manager()->OnBootDeviceDiskEvent(DiskMountManager::DISK_CHANGED,
                                          *disk);
  EXPECT_EQ(0U, observer.events().size());

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnAutoMountableDiskEvent_Hidden) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  std::unique_ptr<const Disk> disk =
      Disk::Builder().SetDevicePath("device1").SetIsHidden(true).Build();

  volume_manager()->OnAutoMountableDiskEvent(DiskMountManager::DISK_ADDED,
                                             *disk);
  EXPECT_EQ(0U, observer.events().size());

  volume_manager()->OnAutoMountableDiskEvent(DiskMountManager::DISK_REMOVED,
                                             *disk);
  EXPECT_EQ(0U, observer.events().size());

  volume_manager()->OnAutoMountableDiskEvent(DiskMountManager::DISK_CHANGED,
                                             *disk);
  EXPECT_EQ(0U, observer.events().size());

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnAutoMountableDiskEvent_Added) {
  // Enable external storage.
  profile()->GetPrefs()->SetBoolean(disks::prefs::kExternalStorageDisabled,
                                    false);

  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  std::unique_ptr<const Disk> empty_device_path_disk = Disk::Builder().Build();
  volume_manager()->OnAutoMountableDiskEvent(DiskMountManager::DISK_ADDED,
                                             *empty_device_path_disk);
  EXPECT_EQ(0U, observer.events().size());

  std::unique_ptr<const Disk> media_disk =
      Disk::Builder().SetDevicePath("device1").SetHasMedia(true).Build();
  volume_manager()->OnAutoMountableDiskEvent(DiskMountManager::DISK_ADDED,
                                             *media_disk);
  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::DISK_ADDED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_TRUE(event.mounting);

  ASSERT_EQ(1U, disk_mount_manager_->mount_requests().size());
  const FakeDiskMountManager::MountRequest& mount_request =
      disk_mount_manager_->mount_requests()[0];
  EXPECT_EQ("device1", mount_request.source_path);
  EXPECT_EQ("", mount_request.source_format);
  EXPECT_EQ("", mount_request.mount_label);
  EXPECT_EQ(ash::MountType::kDevice, mount_request.type);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnAutoMountableDiskEvent_AddedNonMounting) {
  // Enable external storage.
  profile()->GetPrefs()->SetBoolean(disks::prefs::kExternalStorageDisabled,
                                    false);

  // Device which is already mounted.
  {
    LoggingObserver observer;
    volume_manager()->AddObserver(&observer);

    std::unique_ptr<const Disk> mounted_media_disk =
        Disk::Builder()
            .SetDevicePath("device1")
            .SetMountPath("mounted")
            .SetHasMedia(true)
            .Build();
    volume_manager()->OnAutoMountableDiskEvent(DiskMountManager::DISK_ADDED,
                                               *mounted_media_disk);
    ASSERT_EQ(1U, observer.events().size());
    const LoggingObserver::Event& event = observer.events()[0];
    EXPECT_EQ(LoggingObserver::Event::DISK_ADDED, event.type);
    EXPECT_EQ("device1", event.device_path);
    EXPECT_FALSE(event.mounting);

    ASSERT_EQ(0U, disk_mount_manager_->mount_requests().size());

    volume_manager()->RemoveObserver(&observer);
  }

  // Device without media.
  {
    LoggingObserver observer;
    volume_manager()->AddObserver(&observer);

    std::unique_ptr<const Disk> no_media_disk =
        Disk::Builder().SetDevicePath("device1").Build();
    volume_manager()->OnAutoMountableDiskEvent(DiskMountManager::DISK_ADDED,
                                               *no_media_disk);
    ASSERT_EQ(1U, observer.events().size());
    const LoggingObserver::Event& event = observer.events()[0];
    EXPECT_EQ(LoggingObserver::Event::DISK_ADDED, event.type);
    EXPECT_EQ("device1", event.device_path);
    EXPECT_FALSE(event.mounting);

    ASSERT_EQ(0U, disk_mount_manager_->mount_requests().size());

    volume_manager()->RemoveObserver(&observer);
  }

  // External storage is disabled.
  {
    profile()->GetPrefs()->SetBoolean(disks::prefs::kExternalStorageDisabled,
                                      true);

    LoggingObserver observer;
    volume_manager()->AddObserver(&observer);

    std::unique_ptr<const Disk> media_disk =
        Disk::Builder().SetDevicePath("device1").SetHasMedia(true).Build();
    volume_manager()->OnAutoMountableDiskEvent(DiskMountManager::DISK_ADDED,
                                               *media_disk);
    ASSERT_EQ(1U, observer.events().size());
    const LoggingObserver::Event& event = observer.events()[0];
    EXPECT_EQ(LoggingObserver::Event::DISK_ADDED, event.type);
    EXPECT_EQ("device1", event.device_path);
    EXPECT_FALSE(event.mounting);

    ASSERT_EQ(0U, disk_mount_manager_->mount_requests().size());

    volume_manager()->RemoveObserver(&observer);
  }
}

TEST_F(VolumeManagerTest, OnDiskAutoMountableEvent_Removed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  std::unique_ptr<const Disk> mounted_disk = Disk::Builder()
                                                 .SetDevicePath("device1")
                                                 .SetMountPath("mount_path")
                                                 .Build();
  volume_manager()->OnAutoMountableDiskEvent(DiskMountManager::DISK_REMOVED,
                                             *mounted_disk);

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::DISK_REMOVED, event.type);
  EXPECT_EQ("device1", event.device_path);

  ASSERT_EQ(1U, disk_mount_manager_->unmount_requests().size());
  EXPECT_EQ("mount_path", disk_mount_manager_->unmount_requests()[0]);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnAutoMountableDiskEvent_RemovedNotMounted) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  std::unique_ptr<const Disk> not_mounted_disk =
      Disk::Builder().SetDevicePath("device1").Build();
  volume_manager()->OnAutoMountableDiskEvent(DiskMountManager::DISK_REMOVED,
                                             *not_mounted_disk);

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::DISK_REMOVED, event.type);
  EXPECT_EQ("device1", event.device_path);

  ASSERT_EQ(0U, disk_mount_manager_->unmount_requests().size());

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnAutoMountableDiskEvent_Changed) {
  // Changed event should cause mounting (if possible).
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  std::unique_ptr<const Disk> disk =
      Disk::Builder().SetDevicePath("device1").SetHasMedia(true).Build();
  volume_manager()->OnAutoMountableDiskEvent(DiskMountManager::DISK_CHANGED,
                                             *disk);

  EXPECT_EQ(1U, observer.events().size());
  EXPECT_EQ(1U, disk_mount_manager_->mount_requests().size());
  EXPECT_EQ(0U, disk_mount_manager_->unmount_requests().size());
  // Read-write mode by default.
  EXPECT_EQ(ash::MountAccessMode::kReadWrite,
            disk_mount_manager_->mount_requests()[0].access_mode);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnAutoMountableDiskEvent_ChangedInReadonly) {
  profile()->GetPrefs()->SetBoolean(disks::prefs::kExternalStorageReadOnly,
                                    true);

  // Changed event should cause mounting (if possible).
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  std::unique_ptr<const Disk> disk =
      Disk::Builder().SetDevicePath("device1").SetHasMedia(true).Build();
  volume_manager()->OnAutoMountableDiskEvent(DiskMountManager::DISK_CHANGED,
                                             *disk);

  EXPECT_EQ(1U, observer.events().size());
  EXPECT_EQ(1U, disk_mount_manager_->mount_requests().size());
  EXPECT_EQ(0U, disk_mount_manager_->unmount_requests().size());
  // Should mount a disk in read-only mode.
  EXPECT_EQ(ash::MountAccessMode::kReadOnly,
            disk_mount_manager_->mount_requests()[0].access_mode);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnDeviceEvent_Added) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnDeviceEvent(DiskMountManager::DEVICE_ADDED, "device1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::DEVICE_ADDED, event.type);
  EXPECT_EQ("device1", event.device_path);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnDeviceEvent_Removed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnDeviceEvent(DiskMountManager::DEVICE_REMOVED, "device1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::DEVICE_REMOVED, event.type);
  EXPECT_EQ("device1", event.device_path);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnDeviceEvent_Scanned) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnDeviceEvent(DiskMountManager::DEVICE_SCANNED, "device1");

  // SCANNED event is just ignored.
  EXPECT_EQ(0U, observer.events().size());

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnMountEvent_MountingAndUnmounting) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  const DiskMountManager::MountPoint kMountPoint{"device1", "mount1",
                                                 ash::MountType::kDevice};

  volume_manager()->OnMountEvent(DiskMountManager::MOUNTING,
                                 ash::MountError::kSuccess, kMountPoint);

  ASSERT_EQ(1U, observer.events().size());
  LoggingObserver::Event event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::VOLUME_MOUNTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ(ash::MountError::kSuccess, event.mount_error);

  volume_manager()->OnMountEvent(DiskMountManager::UNMOUNTING,
                                 ash::MountError::kSuccess, kMountPoint);

  ASSERT_EQ(2U, observer.events().size());
  event = observer.events()[1];
  EXPECT_EQ(LoggingObserver::Event::VOLUME_UNMOUNTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ(ash::MountError::kSuccess, event.mount_error);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnMountEvent_Remounting) {
  std::unique_ptr<Disk> disk = Disk::Builder()
                                   .SetDevicePath("device1")
                                   .SetFileSystemUUID("uuid1")
                                   .Build();
  disk_mount_manager_->AddDiskForTest(std::move(disk));
  disk_mount_manager_->MountPath("device1", "", "", {}, ash::MountType::kDevice,
                                 ash::MountAccessMode::kReadWrite,
                                 base::DoNothing());

  const DiskMountManager::MountPoint kMountPoint{"device1", "mount1",
                                                 ash::MountType::kDevice};

  volume_manager()->OnMountEvent(DiskMountManager::MOUNTING,
                                 ash::MountError::kSuccess, kMountPoint);

  LoggingObserver observer;

  // Emulate system suspend and then resume.
  {
    chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
        power_manager::SuspendImminent_Reason_OTHER);
    chromeos::FakePowerManagerClient::Get()->SendSuspendDone();

    // After resume, the device is unmounted and then mounted.
    volume_manager()->OnMountEvent(DiskMountManager::UNMOUNTING,
                                   ash::MountError::kSuccess, kMountPoint);

    // Observe what happened for the mount event.
    volume_manager()->AddObserver(&observer);

    volume_manager()->OnMountEvent(DiskMountManager::MOUNTING,
                                   ash::MountError::kSuccess, kMountPoint);
  }

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::VOLUME_MOUNTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ(ash::MountError::kSuccess, event.mount_error);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnMountEvent_UnmountingWithoutMounting) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  const DiskMountManager::MountPoint kMountPoint{"device1", "mount1",
                                                 ash::MountType::kDevice};

  volume_manager()->OnMountEvent(DiskMountManager::UNMOUNTING,
                                 ash::MountError::kSuccess, kMountPoint);

  // Unmount event for a disk not mounted in this manager is not reported.
  ASSERT_EQ(0U, observer.events().size());

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnFormatEvent_Started) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnFormatEvent(DiskMountManager::FORMAT_STARTED,
                                  ash::FormatError::kSuccess, "device1",
                                  "label1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::FORMAT_STARTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ("label1", event.device_label);
  EXPECT_TRUE(event.success);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnFormatEvent_StartFailed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnFormatEvent(DiskMountManager::FORMAT_STARTED,
                                  ash::FormatError::kUnknownError, "device1",
                                  "label1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::FORMAT_STARTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ("label1", event.device_label);
  EXPECT_FALSE(event.success);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnFormatEvent_Completed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnFormatEvent(DiskMountManager::FORMAT_COMPLETED,
                                  ash::FormatError::kSuccess, "device1",
                                  "label1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::FORMAT_COMPLETED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ("label1", event.device_label);
  EXPECT_TRUE(event.success);

  // When "format" is done, VolumeManager requests to mount it.
  ASSERT_EQ(1U, disk_mount_manager_->mount_requests().size());
  const FakeDiskMountManager::MountRequest& mount_request =
      disk_mount_manager_->mount_requests()[0];
  EXPECT_EQ("device1", mount_request.source_path);
  EXPECT_EQ("", mount_request.source_format);
  EXPECT_EQ("", mount_request.mount_label);
  EXPECT_EQ(ash::MountType::kDevice, mount_request.type);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnFormatEvent_CompletedFailed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnFormatEvent(DiskMountManager::FORMAT_COMPLETED,
                                  ash::FormatError::kUnknownError, "device1",
                                  "label1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::FORMAT_COMPLETED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ("label1", event.device_label);
  EXPECT_FALSE(event.success);

  // When "format" is done, VolumeManager requests to mount it.
  ASSERT_EQ(1U, disk_mount_manager_->mount_requests().size());
  const FakeDiskMountManager::MountRequest& mount_request =
      disk_mount_manager_->mount_requests()[0];
  EXPECT_EQ("device1", mount_request.source_path);
  EXPECT_EQ("", mount_request.source_format);
  EXPECT_EQ("", mount_request.mount_label);
  EXPECT_EQ(ash::MountType::kDevice, mount_request.type);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnPartitionEvent_Started) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnPartitionEvent(DiskMountManager::PARTITION_STARTED,
                                     ash::PartitionError::kSuccess, "device1",
                                     "label1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::PARTITION_STARTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ("label1", event.device_label);
  EXPECT_TRUE(event.success);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnPartitionEvent_StartFailed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnPartitionEvent(DiskMountManager::PARTITION_STARTED,
                                     ash::PartitionError::kUnknownError,
                                     "device1", "label1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::PARTITION_STARTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ("label1", event.device_label);
  EXPECT_FALSE(event.success);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnPartitionEvent_Completed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnPartitionEvent(DiskMountManager::PARTITION_COMPLETED,
                                     ash::PartitionError::kSuccess, "device1",
                                     "label1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::PARTITION_COMPLETED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ("label1", event.device_label);
  EXPECT_TRUE(event.success);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnPartitionEvent_CompletedFailed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnPartitionEvent(DiskMountManager::PARTITION_COMPLETED,
                                     ash::PartitionError::kUnknownError,
                                     "device1", "label1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::PARTITION_COMPLETED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ("label1", event.device_label);
  EXPECT_FALSE(event.success);

  // When "partitioning" fails, VolumeManager requests to mount it for retry.
  ASSERT_EQ(1U, disk_mount_manager_->mount_requests().size());
  const FakeDiskMountManager::MountRequest& mount_request =
      disk_mount_manager_->mount_requests()[0];
  EXPECT_EQ("device1", mount_request.source_path);
  EXPECT_EQ("", mount_request.source_format);
  EXPECT_EQ("", mount_request.mount_label);
  EXPECT_EQ(ash::MountType::kDevice, mount_request.type);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnExternalStorageDisabledChanged) {
  // Here create four mount points.
  disk_mount_manager_->MountPath("mount1", "", "", {}, ash::MountType::kDevice,
                                 ash::MountAccessMode::kReadWrite,
                                 base::DoNothing());
  disk_mount_manager_->MountPath("mount2", "", "", {}, ash::MountType::kDevice,
                                 ash::MountAccessMode::kReadOnly,
                                 base::DoNothing());
  disk_mount_manager_->MountPath(
      "mount3", "", "", {}, ash::MountType::kNetworkStorage,
      ash::MountAccessMode::kReadOnly, base::DoNothing());
  disk_mount_manager_->MountPath(
      "failed_unmount", "", "", {}, ash::MountType::kDevice,
      ash::MountAccessMode::kReadWrite, base::DoNothing());
  disk_mount_manager_->FailUnmountRequest("failed_unmount",
                                          ash::MountError::kUnknownError);

  // Initially, there are four mount points.
  ASSERT_EQ(4U, disk_mount_manager_->mount_points().size());
  ASSERT_EQ(0U, disk_mount_manager_->unmount_requests().size());

  // Emulate to set kExternalStorageDisabled to false.
  profile()->GetPrefs()->SetBoolean(disks::prefs::kExternalStorageDisabled,
                                    false);
  volume_manager()->OnExternalStorageDisabledChanged();

  // Expect no effects.
  EXPECT_EQ(4U, disk_mount_manager_->mount_points().size());
  EXPECT_EQ(0U, disk_mount_manager_->unmount_requests().size());

  // Emulate to set kExternalStorageDisabled to true.
  profile()->GetPrefs()->SetBoolean(disks::prefs::kExternalStorageDisabled,
                                    true);
  volume_manager()->OnExternalStorageDisabledChanged();

  // Wait until all unmount request finishes, so that callback chain to unmount
  // all the mount points will be invoked.
  disk_mount_manager_->FinishAllUnmountPathRequests();

  // The external media mount points should be unmounted. Other mount point
  // types should remain. The failing unmount should also remain.
  EXPECT_EQ(2U, disk_mount_manager_->mount_points().size());

  std::set<std::string> expected_unmount_requests = {
      "mount1",
      "mount2",
      "failed_unmount",
  };
  for (const auto& request : disk_mount_manager_->unmount_requests()) {
    EXPECT_TRUE(base::Contains(expected_unmount_requests, request));
    expected_unmount_requests.erase(request);
  }
  EXPECT_TRUE(expected_unmount_requests.empty());
}

TEST_F(VolumeManagerTest, ExternalStorageDisabledPolicyMultiProfile) {
  auto secondary = std::make_unique<ProfileEnvironment>(
      AddLoggedInUser(AccountId::FromUserEmail("secondary@test")),
      disk_mount_manager_.get());
  volume_manager()->Initialize();
  secondary->volume_manager()->Initialize();

  // Simulates the case that the main profile has kExternalStorageDisabled set
  // as false, and the secondary profile has the config set to true.
  profile()->GetPrefs()->SetBoolean(disks::prefs::kExternalStorageDisabled,
                                    false);
  secondary->profile()->GetPrefs()->SetBoolean(
      disks::prefs::kExternalStorageDisabled, true);

  LoggingObserver main_observer, secondary_observer;
  volume_manager()->AddObserver(&main_observer);
  secondary->volume_manager()->AddObserver(&secondary_observer);

  // Add 1 disk.
  std::unique_ptr<const Disk> media_disk =
      Disk::Builder().SetDevicePath("device1").SetHasMedia(true).Build();
  volume_manager()->OnAutoMountableDiskEvent(DiskMountManager::DISK_ADDED,
                                             *media_disk);
  secondary->volume_manager()->OnAutoMountableDiskEvent(
      DiskMountManager::DISK_ADDED, *media_disk);

  // The profile with external storage enabled should have mounted the volume.
  bool has_volume_mounted = false;
  for (size_t i = 0; i < main_observer.events().size(); ++i) {
    if (main_observer.events()[i].type ==
        LoggingObserver::Event::VOLUME_MOUNTED) {
      has_volume_mounted = true;
    }
  }
  EXPECT_TRUE(has_volume_mounted);

  // The other profiles with external storage disabled should have not.
  has_volume_mounted = false;
  for (size_t i = 0; i < secondary_observer.events().size(); ++i) {
    if (secondary_observer.events()[i].type ==
        LoggingObserver::Event::VOLUME_MOUNTED) {
      has_volume_mounted = true;
    }
  }
  EXPECT_FALSE(has_volume_mounted);

  volume_manager()->RemoveObserver(&main_observer);
  secondary->volume_manager()->RemoveObserver(&secondary_observer);
}

TEST_F(VolumeManagerTest, OnExternalStorageReadOnlyChanged) {
  // Emulate updates of kExternalStorageReadOnly (change to true, then false).
  profile()->GetPrefs()->SetBoolean(disks::prefs::kExternalStorageReadOnly,
                                    true);
  volume_manager()->OnExternalStorageReadOnlyChanged();
  profile()->GetPrefs()->SetBoolean(disks::prefs::kExternalStorageReadOnly,
                                    false);
  volume_manager()->OnExternalStorageReadOnlyChanged();

  // Verify that remount of removable disks is triggered for each update.
  ASSERT_EQ(2U, disk_mount_manager_->remount_all_requests().size());
  const FakeDiskMountManager::RemountAllRequest& remount_request1 =
      disk_mount_manager_->remount_all_requests()[0];
  EXPECT_EQ(ash::MountAccessMode::kReadOnly, remount_request1.access_mode);
  const FakeDiskMountManager::RemountAllRequest& remount_request2 =
      disk_mount_manager_->remount_all_requests()[1];
  EXPECT_EQ(ash::MountAccessMode::kReadWrite, remount_request2.access_mode);
}

TEST_F(VolumeManagerTest, GetVolumeList) {
  volume_manager()->Initialize();  // Adds "Downloads"
  std::vector<base::WeakPtr<Volume>> volume_list =
      volume_manager()->GetVolumeList();
  ASSERT_GT(volume_list.size(), 0u);
}

TEST_F(VolumeManagerTest, VolumeManagerInitializeMyFilesVolume) {
  // Emulate running inside ChromeOS.
  base::test::ScopedRunningOnChromeOS running_on_chromeos;
  volume_manager()->Initialize();  // Adds "Downloads"
  std::vector<base::WeakPtr<Volume>> volume_list =
      volume_manager()->GetVolumeList();
  ASSERT_GT(volume_list.size(), 0u);
  auto volume =
      base::ranges::find(volume_list, "downloads:MyFiles", &Volume::volume_id);
  EXPECT_FALSE(volume == volume_list.end());
  EXPECT_EQ(VOLUME_TYPE_DOWNLOADS_DIRECTORY, (*volume)->type());
}

TEST_F(VolumeManagerTest, FindVolumeById) {
  volume_manager()->Initialize();  // Adds "Downloads"
  base::WeakPtr<Volume> bad_volume =
      volume_manager()->FindVolumeById("nonexistent");
  ASSERT_FALSE(bad_volume.get());
  base::WeakPtr<Volume> good_volume =
      volume_manager()->FindVolumeById("downloads:MyFiles");
  ASSERT_TRUE(good_volume.get());
  EXPECT_EQ("downloads:MyFiles", good_volume->volume_id());
  EXPECT_EQ(VOLUME_TYPE_DOWNLOADS_DIRECTORY, good_volume->type());
}

TEST_F(VolumeManagerTest, VolumeManagerInitializeShareCacheVolume) {
  volume_manager()->Initialize();
  base::WeakPtr<Volume> share_cache_volume =
      volume_manager()->FindVolumeById("system_internal:ShareCache");
  ASSERT_TRUE(share_cache_volume.get());
  EXPECT_EQ("system_internal:ShareCache", share_cache_volume->volume_id());
  EXPECT_EQ(VOLUME_TYPE_SYSTEM_INTERNAL, share_cache_volume->type());
}

TEST_F(VolumeManagerTest, FindVolumeFromPath) {
  volume_manager()->Initialize();  // Adds "Downloads"
  base::WeakPtr<Volume> downloads_volume = volume_manager()->GetVolumeList()[0];
  EXPECT_EQ("downloads:MyFiles", downloads_volume->volume_id());
  base::FilePath downloads_mount_path = downloads_volume->mount_path();
  // FindVolumeFromPath(downloads_mount_path.DirName()) should return null
  // because the path is the parent folder of the Downloads mount path.
  base::WeakPtr<Volume> volume_from_path =
      volume_manager()->FindVolumeFromPath(downloads_mount_path.DirName());
  ASSERT_FALSE(volume_from_path);
  // FindVolumeFromPath("MyFiles") should return null because it's only the last
  // component of the Downloads mount path.
  volume_from_path =
      volume_manager()->FindVolumeFromPath(downloads_mount_path.BaseName());
  ASSERT_FALSE(volume_from_path);
  // FindVolumeFromPath(<Downloads mount path>) should point to the Downloads
  // volume.
  volume_from_path = volume_manager()->FindVolumeFromPath(downloads_mount_path);
  ASSERT_TRUE(volume_from_path);
  EXPECT_EQ("downloads:MyFiles", volume_from_path->volume_id());
  // FindVolumeFromPath(<Downloads mount path>/folder) is on the Downloads
  // volume, it should also point to the Downloads volume, even if the folder
  // doesn't exist.
  volume_from_path = volume_manager()->FindVolumeFromPath(
      downloads_mount_path.Append("folder"));
  ASSERT_TRUE(volume_from_path);
  EXPECT_EQ("downloads:MyFiles", volume_from_path->volume_id());
}

TEST_F(VolumeManagerTest, ArchiveSourceFiltering) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  // Mount a USB stick.
  volume_manager()->OnMountEvent(
      DiskMountManager::MOUNTING, ash::MountError::kSuccess,
      {"/removable/usb", "/removable/usb", ash::MountType::kDevice});

  // Mount a zip archive in the stick.
  volume_manager()->OnMountEvent(
      DiskMountManager::MOUNTING, ash::MountError::kSuccess,
      {"/removable/usb/1.zip", "/archive/1", ash::MountType::kArchive});
  base::WeakPtr<Volume> volume = volume_manager()->FindVolumeById("archive:1");
  ASSERT_TRUE(volume.get());
  EXPECT_EQ("/archive/1", volume->mount_path().AsUTF8Unsafe());
  EXPECT_EQ(2u, observer.events().size());

  // Mount a zip archive in the previous zip archive.
  volume_manager()->OnMountEvent(
      DiskMountManager::MOUNTING, ash::MountError::kSuccess,
      {"/archive/1/2.zip", "/archive/2", ash::MountType::kArchive});
  base::WeakPtr<Volume> second_volume =
      volume_manager()->FindVolumeById("archive:2");
  ASSERT_TRUE(second_volume.get());
  EXPECT_EQ("/archive/2", second_volume->mount_path().AsUTF8Unsafe());
  EXPECT_EQ(3u, observer.events().size());

  // A zip file is mounted from other profile. It must be ignored in the current
  // VolumeManager.
  volume_manager()->OnMountEvent(DiskMountManager::MOUNTING,
                                 ash::MountError::kSuccess,
                                 {"/other/profile/drive/folder/3.zip",
                                  "/archive/3", ash::MountType::kArchive});
  base::WeakPtr<Volume> third_volume =
      volume_manager()->FindVolumeById("archive:3");
  ASSERT_FALSE(third_volume.get());
  EXPECT_EQ(3u, observer.events().size());

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, MTPPlugAndUnplug) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  storage_monitor::StorageInfo info(
      storage_monitor::StorageInfo::MakeDeviceId(
          storage_monitor::StorageInfo::MTP_OR_PTP, "dummy-device-id"),
      FILE_PATH_LITERAL("/dummy/device/location"), u"label", u"vendor",
      u"model", 12345 /* size */);

  storage_monitor::StorageInfo non_mtp_info(
      storage_monitor::StorageInfo::MakeDeviceId(
          storage_monitor::StorageInfo::FIXED_MASS_STORAGE, "dummy-device-id2"),
      FILE_PATH_LITERAL("/dummy/device/location2"), u"label2", u"vendor2",
      u"model2", 12345 /* size */);

  // Attach: expect mount events for the MTP and fusebox MTP volumes.
  volume_manager()->OnRemovableStorageAttached(info);
  ASSERT_EQ(2u, observer.events().size());
  EXPECT_EQ(LoggingObserver::Event::VOLUME_MOUNTED, observer.events()[0].type);
  EXPECT_EQ(LoggingObserver::Event::VOLUME_MOUNTED, observer.events()[1].type);

  // The MTP volume should be mounted.
  base::WeakPtr<Volume> volume = volume_manager()->FindVolumeById("mtp:model");
  ASSERT_TRUE(volume);
  EXPECT_EQ("", volume->file_system_type());
  EXPECT_EQ(VOLUME_TYPE_MTP, volume->type());

  // The fusebox MTP volume should be mounted.
  const auto fusebox_volume_id = base::StrCat({util::kFuseBox, "mtp:model"});
  base::WeakPtr<Volume> fusebox_volume =
      volume_manager()->FindVolumeById(fusebox_volume_id);
  ASSERT_TRUE(fusebox_volume);
  EXPECT_EQ(util::kFuseBox, fusebox_volume->file_system_type());
  EXPECT_EQ(VOLUME_TYPE_MTP, fusebox_volume->type());

  // Non MTP attach events from storage monitor are ignored.
  volume_manager()->OnRemovableStorageAttached(non_mtp_info);
  EXPECT_EQ(2u, observer.events().size());

  // Detach: there should be two more events, bringing the total to four.
  volume_manager()->OnRemovableStorageDetached(info);
  ASSERT_EQ(4u, observer.events().size());
  EXPECT_EQ(LoggingObserver::Event::VOLUME_UNMOUNTED,
            observer.events()[2].type);
  EXPECT_EQ(LoggingObserver::Event::VOLUME_UNMOUNTED,
            observer.events()[3].type);

  // The unmount events should remove the MTP and fusebox MTP volumes.
  EXPECT_FALSE(volume);
  EXPECT_FALSE(fusebox_volume);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnRenameEvent_Started) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnRenameEvent(DiskMountManager::RENAME_STARTED,
                                  ash::RenameError::kSuccess, "device1",
                                  "label1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::RENAME_STARTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ("label1", event.device_label);
  EXPECT_TRUE(event.success);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnRenameEvent_StartFailed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnRenameEvent(DiskMountManager::RENAME_STARTED,
                                  ash::RenameError::kUnknownError, "device1",
                                  "label1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::RENAME_STARTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ("label1", event.device_label);
  EXPECT_FALSE(event.success);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnRenameEvent_Completed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnRenameEvent(DiskMountManager::RENAME_COMPLETED,
                                  ash::RenameError::kSuccess, "device1",
                                  "label1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::RENAME_COMPLETED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ("label1", event.device_label);
  EXPECT_TRUE(event.success);

  // When "rename" is successfully done, VolumeManager requests to mount it.
  ASSERT_EQ(1U, disk_mount_manager_->mount_requests().size());
  const FakeDiskMountManager::MountRequest& mount_request =
      disk_mount_manager_->mount_requests()[0];
  EXPECT_EQ("device1", mount_request.source_path);
  EXPECT_EQ("", mount_request.source_format);
  EXPECT_EQ(ash::MountType::kDevice, mount_request.type);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnRenameEvent_CompletedFailed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnRenameEvent(DiskMountManager::RENAME_COMPLETED,
                                  ash::RenameError::kUnknownError, "device1",
                                  "label1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::RENAME_COMPLETED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ("label1", event.device_label);
  EXPECT_FALSE(event.success);

  EXPECT_EQ(1U, disk_mount_manager_->mount_requests().size());

  volume_manager()->RemoveObserver(&observer);
}

// Test fixture for VolumeManager tests with ARC enabled.
class VolumeManagerArcTest : public VolumeManagerTest {
 public:
  void SetUp() override {
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kArcAvailability, "officially-supported");
    VolumeManagerTest::SetUp();
  }

  void TearDown() override {
    arc_service_manager_->arc_bridge_service()->file_system()->CloseInstance(
        &file_system_instance_);
    arc_service_manager_->set_browser_context(nullptr);
    VolumeManagerTest::TearDown();
  }

  std::unique_ptr<TestingProfile> AddLoggedInUser(
      const AccountId& account_id) override {
    auto profile = VolumeManagerTest::AddLoggedInUser(account_id);

    // Set up an Arc service manager with a fake file system. This must be done
    // before initializing VolumeManager() to make its dependency
    // DocumentsProviderRootManager work.
    CHECK(!arc_service_manager_);
    arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
    arc_service_manager_->set_browser_context(profile.get());
    arc::ArcFileSystemOperationRunner::GetFactory()->SetTestingFactoryAndUse(
        profile.get(),
        base::BindRepeating(&CreateFileSystemOperationRunnerForTesting));
    arc_service_manager_->arc_bridge_service()->file_system()->SetInstance(
        &file_system_instance_);
    arc::WaitForInstanceReady(
        arc_service_manager_->arc_bridge_service()->file_system());
    EXPECT_TRUE(file_system_instance_.InitCalled());
    return profile;
  }

 private:
  arc::FakeFileSystemInstance file_system_instance_;
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
};

TEST_F(VolumeManagerArcTest, OnArcPlayStoreEnabledChanged_Enabled) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnArcPlayStoreEnabledChanged(true);

  ASSERT_EQ(5U, observer.events().size());

  size_t index = 0;
  for (const auto& event : observer.events()) {
    EXPECT_EQ(LoggingObserver::Event::VOLUME_MOUNTED, event.type);
    EXPECT_EQ(ash::MountError::kSuccess, event.mount_error);
    if (index < 4) {
      EXPECT_EQ(arc::GetMediaViewVolumeId(arc_volume_ids[index]),
                event.volume_id);
    } else {
      EXPECT_EQ(arc_volume_ids[index], event.volume_id);
    }
    index++;
  }

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerArcTest, OnArcPlayStoreEnabledChanged_Disabled) {
  // Need to enable it first before disabling it, otherwise
  // it will be no-op.
  volume_manager()->OnArcPlayStoreEnabledChanged(true);

  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnArcPlayStoreEnabledChanged(false);

  ASSERT_EQ(5U, observer.events().size());

  size_t index = 0;
  for (const auto& event : observer.events()) {
    EXPECT_EQ(LoggingObserver::Event::VOLUME_UNMOUNTED, event.type);
    EXPECT_EQ(ash::MountError::kSuccess, event.mount_error);
    if (index < 4) {
      EXPECT_EQ(arc::GetMediaViewVolumeId(arc_volume_ids[index]),
                event.volume_id);
    } else {
      EXPECT_EQ(arc_volume_ids[index], event.volume_id);
    }
    index++;
  }

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerArcTest, ShouldAlwaysMountAndroidVolumesInFilesForTesting) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kArcForceMountAndroidVolumesInFiles);

  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  // Volumes are mounted even when Play Store is not enabled for the profile.
  volume_manager()->OnArcPlayStoreEnabledChanged(false);

  ASSERT_EQ(5U, observer.events().size());

  size_t index = 0;
  for (const auto& event : observer.events()) {
    EXPECT_EQ(LoggingObserver::Event::VOLUME_MOUNTED, event.type);
    EXPECT_EQ(ash::MountError::kSuccess, event.mount_error);
    if (index < 4) {
      EXPECT_EQ(arc::GetMediaViewVolumeId(arc_volume_ids[index]),
                event.volume_id);
    } else {
      EXPECT_EQ(arc_volume_ids[index], event.volume_id);
    }
    index++;
  }

  // No volume-related event happens after Play Store preference changes,
  // because volumes are just kept being mounted.
  volume_manager()->OnArcPlayStoreEnabledChanged(true);
  volume_manager()->OnArcPlayStoreEnabledChanged(false);
  ASSERT_EQ(5U, observer.events().size());

  volume_manager()->RemoveObserver(&observer);
}

// Tests VolumeManager with the LocalUserFilesAllowed policy.
class VolumeManagerLocalUserFilesTest : public VolumeManagerArcTest {
 public:
  VolumeManagerLocalUserFilesTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kSkyVault);
    VolumeManagerArcTest::SetUp();
  }

  void TearDown() override { VolumeManagerArcTest::TearDown(); }

  void SetLocalUserFilesPolicy(bool allowed) {
    scoped_testing_local_state_.Get()->SetBoolean(prefs::kLocalUserFilesAllowed,
                                                  allowed);
  }

  bool ContainsDownloads() {
    std::vector<base::WeakPtr<Volume>> volume_list =
        volume_manager()->GetVolumeList();
    if (volume_list.size() == 0u) {
      return false;
    }
    auto volume = base::ranges::find(volume_list, "downloads:MyFiles",
                                     &Volume::volume_id);
    return volume != volume_list.end() &&
           (*volume)->type() == VOLUME_TYPE_DOWNLOADS_DIRECTORY;
  }

  bool ContainsPlayFiles() {
    std::vector<base::WeakPtr<Volume>> volume_list =
        volume_manager()->GetVolumeList();
    if (volume_list.size() == 0u) {
      return false;
    }
    auto volume =
        base::ranges::find(volume_list, "android_files:0", &Volume::volume_id);
    return volume != volume_list.end() &&
           (*volume)->type() == VOLUME_TYPE_ANDROID_FILES;
  }

 private:
  ScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that VolumeManager removes local volumes when the policy is set to
// false, and adds them when set to true.
TEST_F(VolumeManagerLocalUserFilesTest, DisableEnable) {
  // Enable ARC.
  profile()->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled, true);
  // Emulate running inside ChromeOS.
  base::test::ScopedRunningOnChromeOS running_on_chromeos;
  volume_manager()->Initialize();  // Adds "Downloads" and "Play Files"
  EXPECT_TRUE(ContainsDownloads());
  EXPECT_TRUE(ContainsPlayFiles());

  // Setting the policy to false removes local volumes.
  SetLocalUserFilesPolicy(/*allowed=*/false);
  EXPECT_FALSE(ContainsDownloads());
  EXPECT_FALSE(ContainsPlayFiles());

  // Setting the policy to true adds local volumes.
  SetLocalUserFilesPolicy(/*allowed=*/true);
  EXPECT_TRUE(ContainsDownloads());
  EXPECT_TRUE(ContainsPlayFiles());

  // Another update with the same value shouldn't do anything.
  SetLocalUserFilesPolicy(/*allowed=*/true);
  EXPECT_TRUE(ContainsDownloads());
  EXPECT_TRUE(ContainsPlayFiles());
}

}  // namespace file_manager
