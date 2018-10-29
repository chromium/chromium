// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/volume_manager.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_observer.h"
#include "chrome/browser/chromeos/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "chromeos/disks/disk.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/drive/chromeos/dummy_file_system.h"
#include "components/drive/service/dummy_drive_service.h"
#include "components/prefs/pref_service.h"
#include "components/storage_monitor/storage_info.h"
#include "components/user_manager/user.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_service_manager_context.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::disks::Disk;
using chromeos::disks::DiskMountManager;

namespace file_manager {
namespace {

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
      RENAME_STARTED,
      RENAME_COMPLETED
    } type;

    // Available on DEVICE_ADDED, DEVICE_REMOVED, VOLUME_MOUNTED,
    // VOLUME_UNMOUNTED, FORMAT_STARTED and FORMAT_COMPLETED.
    std::string device_path;

    // Available on DISK_ADDED.
    bool mounting;

    // Available on VOLUME_MOUNTED and VOLUME_UNMOUNTED.
    chromeos::MountError mount_error;

    // Available on FORMAT_STARTED and FORMAT_COMPLETED.
    bool success;
  };

  LoggingObserver() = default;
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

  void OnVolumeMounted(chromeos::MountError error_code,
                       const Volume& volume) override {
    Event event;
    event.type = Event::VOLUME_MOUNTED;
    event.device_path = volume.source_path().AsUTF8Unsafe();
    event.mount_error = error_code;
    events_.push_back(event);
  }

  void OnVolumeUnmounted(chromeos::MountError error_code,
                         const Volume& volume) override {
    Event event;
    event.type = Event::VOLUME_UNMOUNTED;
    event.device_path = volume.source_path().AsUTF8Unsafe();
    event.mount_error = error_code;
    events_.push_back(event);
  }

  void OnFormatStarted(const std::string& device_path, bool success) override {
    Event event;
    event.type = Event::FORMAT_STARTED;
    event.device_path = device_path;
    event.success = success;
    events_.push_back(event);
  }

  void OnFormatCompleted(const std::string& device_path,
                         bool success) override {
    Event event;
    event.type = Event::FORMAT_COMPLETED;
    event.device_path = device_path;
    event.success = success;
    events_.push_back(event);
  }

  void OnRenameStarted(const std::string& device_path, bool success) override {
    Event event;
    event.type = Event::RENAME_STARTED;
    event.device_path = device_path;
    event.success = success;
    events_.push_back(event);
  }

  void OnRenameCompleted(const std::string& device_path,
                         bool success) override {
    Event event;
    event.type = Event::RENAME_COMPLETED;
    event.device_path = device_path;
    event.success = success;
    events_.push_back(event);
  }

 private:
  std::vector<Event> events_;

  DISALLOW_COPY_AND_ASSIGN(LoggingObserver);
};

class FakeUser : public user_manager::User {
 public:
  explicit FakeUser(const AccountId& account_id) : User(account_id) {}

  user_manager::UserType GetType() const override {
    return user_manager::USER_TYPE_REGULAR;
  }
};

}  // namespace

class VolumeManagerTest : public testing::Test {
 protected:
  // Helper class that contains per-profile objects.
  class ProfileEnvironment {
   public:
    ProfileEnvironment(chromeos::PowerManagerClient* power_manager_client,
                       DiskMountManager* disk_manager)
        : profile_(std::make_unique<TestingProfile>()),
          extension_registry_(
              std::make_unique<extensions::ExtensionRegistry>(profile_.get())),
          file_system_provider_service_(
              std::make_unique<chromeos::file_system_provider::Service>(
                  profile_.get(),
                  extension_registry_.get())),
          drive_integration_service_(
              std::make_unique<drive::DriveIntegrationService>(
                  profile_.get(),
                  nullptr,
                  new drive::DummyDriveService(),
                  std::string(),
                  base::FilePath(),
                  new drive::DummyFileSystem())),
          volume_manager_(std::make_unique<VolumeManager>(
              profile_.get(),
              drive_integration_service_.get(),  // DriveIntegrationService
              power_manager_client,
              disk_manager,
              file_system_provider_service_.get(),
              base::Bind(&ProfileEnvironment::GetFakeMtpStorageInfo,
                         base::Unretained(this)))),
          account_id_(
              AccountId::FromUserEmailGaiaId(profile_->GetProfileUserName(),
                                             "id")),
          user_(account_id_) {
      chromeos::ProfileHelper::Get()->SetProfileToUserMappingForTesting(&user_);
      chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
          &user_, profile_.get());
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
    std::unique_ptr<chromeos::file_system_provider::Service>
        file_system_provider_service_;
    std::unique_ptr<drive::DriveIntegrationService> drive_integration_service_;
    std::unique_ptr<VolumeManager> volume_manager_;
    AccountId account_id_;
    FakeUser user_;
  };

  void SetUp() override {
    power_manager_client_ =
        std::make_unique<chromeos::FakePowerManagerClient>();
    disk_mount_manager_ = std::make_unique<FakeDiskMountManager>();
    main_profile_ = std::make_unique<ProfileEnvironment>(
        power_manager_client_.get(), disk_mount_manager_.get());
  }

  Profile* profile() const { return main_profile_->profile(); }
  VolumeManager* volume_manager() const {
    return main_profile_->volume_manager();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  content::TestServiceManagerContext context_;
  std::unique_ptr<chromeos::FakePowerManagerClient> power_manager_client_;
  std::unique_ptr<FakeDiskMountManager> disk_mount_manager_;
  std::unique_ptr<ProfileEnvironment> main_profile_;
};

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
  EXPECT_EQ(chromeos::MOUNT_ERROR_NONE, event.mount_error);

  volume_manager()->OnFileSystemBeingUnmounted();

  ASSERT_EQ(2U, observer.events().size());
  event = observer.events()[1];
  EXPECT_EQ(LoggingObserver::Event::VOLUME_UNMOUNTED, event.type);
  EXPECT_EQ(drive::DriveIntegrationServiceFactory::GetForProfile(profile())
                ->GetMountPointPath()
                .AsUTF8Unsafe(),
            event.device_path);
  EXPECT_EQ(chromeos::MOUNT_ERROR_NONE, event.mount_error);

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
  profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageDisabled, false);

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
  EXPECT_EQ(chromeos::MOUNT_TYPE_DEVICE, mount_request.type);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnAutoMountableDiskEvent_AddedNonMounting) {
  // Enable external storage.
  profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageDisabled, false);

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
    profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageDisabled, true);

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
  const FakeDiskMountManager::UnmountRequest& unmount_request =
      disk_mount_manager_->unmount_requests()[0];
  EXPECT_EQ("mount_path", unmount_request.mount_path);
  EXPECT_EQ(chromeos::UNMOUNT_OPTIONS_LAZY, unmount_request.options);

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
  EXPECT_EQ(chromeos::MOUNT_ACCESS_MODE_READ_WRITE,
            disk_mount_manager_->mount_requests()[0].access_mode);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnAutoMountableDiskEvent_ChangedInReadonly) {
  profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageReadOnly, true);

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
  // Shoule mount a disk in read-only mode.
  EXPECT_EQ(chromeos::MOUNT_ACCESS_MODE_READ_ONLY,
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

  const DiskMountManager::MountPointInfo kMountPoint(
      "device1", "mount1", chromeos::MOUNT_TYPE_DEVICE,
      chromeos::disks::MOUNT_CONDITION_NONE);

  volume_manager()->OnMountEvent(DiskMountManager::MOUNTING,
                                 chromeos::MOUNT_ERROR_NONE, kMountPoint);

  ASSERT_EQ(1U, observer.events().size());
  LoggingObserver::Event event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::VOLUME_MOUNTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ(chromeos::MOUNT_ERROR_NONE, event.mount_error);

  volume_manager()->OnMountEvent(DiskMountManager::UNMOUNTING,
                                 chromeos::MOUNT_ERROR_NONE, kMountPoint);

  ASSERT_EQ(2U, observer.events().size());
  event = observer.events()[1];
  EXPECT_EQ(LoggingObserver::Event::VOLUME_UNMOUNTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ(chromeos::MOUNT_ERROR_NONE, event.mount_error);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnMountEvent_Remounting) {
  std::unique_ptr<Disk> disk = Disk::Builder()
                                   .SetDevicePath("device1")
                                   .SetFileSystemUUID("uuid1")
                                   .Build();
  disk_mount_manager_->AddDiskForTest(std::move(disk));
  disk_mount_manager_->MountPath("device1", "", "", {},
                                 chromeos::MOUNT_TYPE_DEVICE,
                                 chromeos::MOUNT_ACCESS_MODE_READ_WRITE);

  const DiskMountManager::MountPointInfo kMountPoint(
      "device1", "mount1", chromeos::MOUNT_TYPE_DEVICE,
      chromeos::disks::MOUNT_CONDITION_NONE);

  volume_manager()->OnMountEvent(DiskMountManager::MOUNTING,
                                 chromeos::MOUNT_ERROR_NONE, kMountPoint);

  LoggingObserver observer;

  // Emulate system suspend and then resume.
  {
    power_manager_client_->SendSuspendImminent(
        power_manager::SuspendImminent_Reason_OTHER);
    power_manager_client_->SendSuspendDone();

    // After resume, the device is unmounted and then mounted.
    volume_manager()->OnMountEvent(DiskMountManager::UNMOUNTING,
                                   chromeos::MOUNT_ERROR_NONE, kMountPoint);

    // Observe what happened for the mount event.
    volume_manager()->AddObserver(&observer);

    volume_manager()->OnMountEvent(DiskMountManager::MOUNTING,
                                   chromeos::MOUNT_ERROR_NONE, kMountPoint);
  }

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::VOLUME_MOUNTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ(chromeos::MOUNT_ERROR_NONE, event.mount_error);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnMountEvent_UnmountingWithoutMounting) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  const DiskMountManager::MountPointInfo kMountPoint(
      "device1", "mount1", chromeos::MOUNT_TYPE_DEVICE,
      chromeos::disks::MOUNT_CONDITION_NONE);

  volume_manager()->OnMountEvent(DiskMountManager::UNMOUNTING,
                                 chromeos::MOUNT_ERROR_NONE, kMountPoint);

  // Unmount event for a disk not mounted in this manager is not reported.
  ASSERT_EQ(0U, observer.events().size());

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnFormatEvent_Started) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnFormatEvent(DiskMountManager::FORMAT_STARTED,
                                  chromeos::FORMAT_ERROR_NONE, "device1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::FORMAT_STARTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_TRUE(event.success);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnFormatEvent_StartFailed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnFormatEvent(DiskMountManager::FORMAT_STARTED,
                                  chromeos::FORMAT_ERROR_UNKNOWN, "device1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::FORMAT_STARTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_FALSE(event.success);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnFormatEvent_Completed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnFormatEvent(DiskMountManager::FORMAT_COMPLETED,
                                  chromeos::FORMAT_ERROR_NONE, "device1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::FORMAT_COMPLETED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_TRUE(event.success);

  // When "format" is successfully done, VolumeManager requests to mount it.
  ASSERT_EQ(1U, disk_mount_manager_->mount_requests().size());
  const FakeDiskMountManager::MountRequest& mount_request =
      disk_mount_manager_->mount_requests()[0];
  EXPECT_EQ("device1", mount_request.source_path);
  EXPECT_EQ("", mount_request.source_format);
  EXPECT_EQ("", mount_request.mount_label);
  EXPECT_EQ(chromeos::MOUNT_TYPE_DEVICE, mount_request.type);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnFormatEvent_CompletedFailed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnFormatEvent(DiskMountManager::FORMAT_COMPLETED,
                                  chromeos::FORMAT_ERROR_UNKNOWN, "device1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::FORMAT_COMPLETED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_FALSE(event.success);

  EXPECT_EQ(0U, disk_mount_manager_->mount_requests().size());

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnExternalStorageDisabledChanged) {
  // Here create two mount points.
  disk_mount_manager_->MountPath("mount1", "", "", {},
                                 chromeos::MOUNT_TYPE_DEVICE,
                                 chromeos::MOUNT_ACCESS_MODE_READ_WRITE);
  disk_mount_manager_->MountPath("mount2", "", "", {},
                                 chromeos::MOUNT_TYPE_DEVICE,
                                 chromeos::MOUNT_ACCESS_MODE_READ_ONLY);

  // Initially, there are two mount points.
  ASSERT_EQ(2U, disk_mount_manager_->mount_points().size());
  ASSERT_EQ(0U, disk_mount_manager_->unmount_requests().size());

  // Emulate to set kExternalStorageDisabled to false.
  profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageDisabled, false);
  volume_manager()->OnExternalStorageDisabledChanged();

  // Expect no effects.
  EXPECT_EQ(2U, disk_mount_manager_->mount_points().size());
  EXPECT_EQ(0U, disk_mount_manager_->unmount_requests().size());

  // Emulate to set kExternalStorageDisabled to true.
  profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageDisabled, true);
  volume_manager()->OnExternalStorageDisabledChanged();

  // Wait until all unmount request finishes, so that callback chain to unmount
  // all the mount points will be invoked.
  disk_mount_manager_->FinishAllUnmountPathRequests();

  // The all mount points should be unmounted.
  EXPECT_EQ(0U, disk_mount_manager_->mount_points().size());

  EXPECT_EQ(2U, disk_mount_manager_->unmount_requests().size());
  const FakeDiskMountManager::UnmountRequest& unmount_request1 =
      disk_mount_manager_->unmount_requests()[0];
  EXPECT_EQ("mount1", unmount_request1.mount_path);

  const FakeDiskMountManager::UnmountRequest& unmount_request2 =
      disk_mount_manager_->unmount_requests()[1];
  EXPECT_EQ("mount2", unmount_request2.mount_path);
}

TEST_F(VolumeManagerTest, ExternalStorageDisabledPolicyMultiProfile) {
  ProfileEnvironment secondary(power_manager_client_.get(),
                               disk_mount_manager_.get());
  volume_manager()->Initialize();
  secondary.volume_manager()->Initialize();

  // Simulates the case that the main profile has kExternalStorageDisabled set
  // as false, and the secondary profile has the config set to true.
  profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageDisabled, false);
  secondary.profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageDisabled,
                                              true);

  LoggingObserver main_observer, secondary_observer;
  volume_manager()->AddObserver(&main_observer);
  secondary.volume_manager()->AddObserver(&secondary_observer);

  // Add 1 disk.
  std::unique_ptr<const Disk> media_disk =
      Disk::Builder().SetDevicePath("device1").SetHasMedia(true).Build();
  volume_manager()->OnAutoMountableDiskEvent(DiskMountManager::DISK_ADDED,
                                             *media_disk);
  secondary.volume_manager()->OnAutoMountableDiskEvent(
      DiskMountManager::DISK_ADDED, *media_disk);

  // The profile with external storage enabled should have mounted the volume.
  bool has_volume_mounted = false;
  for (size_t i = 0; i < main_observer.events().size(); ++i) {
    if (main_observer.events()[i].type ==
        LoggingObserver::Event::VOLUME_MOUNTED)
      has_volume_mounted = true;
  }
  EXPECT_TRUE(has_volume_mounted);

  // The other profiles with external storage disabled should have not.
  has_volume_mounted = false;
  for (size_t i = 0; i < secondary_observer.events().size(); ++i) {
    if (secondary_observer.events()[i].type ==
        LoggingObserver::Event::VOLUME_MOUNTED)
      has_volume_mounted = true;
  }
  EXPECT_FALSE(has_volume_mounted);

  volume_manager()->RemoveObserver(&main_observer);
  secondary.volume_manager()->RemoveObserver(&secondary_observer);
}

TEST_F(VolumeManagerTest, OnExternalStorageReadOnlyChanged) {
  // Emulate updates of kExternalStorageReadOnly (change to true, then false).
  profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageReadOnly, true);
  volume_manager()->OnExternalStorageReadOnlyChanged();
  profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageReadOnly, false);
  volume_manager()->OnExternalStorageReadOnlyChanged();

  // Verify that remount of removable disks is triggered for each update.
  ASSERT_EQ(2U, disk_mount_manager_->remount_all_requests().size());
  const FakeDiskMountManager::RemountAllRequest& remount_request1 =
      disk_mount_manager_->remount_all_requests()[0];
  EXPECT_EQ(chromeos::MOUNT_ACCESS_MODE_READ_ONLY,
            remount_request1.access_mode);
  const FakeDiskMountManager::RemountAllRequest& remount_request2 =
      disk_mount_manager_->remount_all_requests()[1];
  EXPECT_EQ(chromeos::MOUNT_ACCESS_MODE_READ_WRITE,
            remount_request2.access_mode);
}

TEST_F(VolumeManagerTest, GetVolumeList) {
  volume_manager()->Initialize();  // Adds "Downloads"
  std::vector<base::WeakPtr<Volume>> volume_list =
      volume_manager()->GetVolumeList();
  ASSERT_EQ(1u, volume_list.size());
  EXPECT_EQ("downloads:Downloads", volume_list[0]->volume_id());
  EXPECT_EQ(VOLUME_TYPE_DOWNLOADS_DIRECTORY, volume_list[0]->type());
}

TEST_F(VolumeManagerTest, FindVolumeById) {
  volume_manager()->Initialize();  // Adds "Downloads"
  base::WeakPtr<Volume> bad_volume =
      volume_manager()->FindVolumeById("nonexistent");
  ASSERT_FALSE(bad_volume.get());
  base::WeakPtr<Volume> good_volume =
      volume_manager()->FindVolumeById("downloads:Downloads");
  ASSERT_TRUE(good_volume.get());
  EXPECT_EQ("downloads:Downloads", good_volume->volume_id());
  EXPECT_EQ(VOLUME_TYPE_DOWNLOADS_DIRECTORY, good_volume->type());
}

TEST_F(VolumeManagerTest, ArchiveSourceFiltering) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  // Mount a USB stick.
  volume_manager()->OnMountEvent(
      DiskMountManager::MOUNTING, chromeos::MOUNT_ERROR_NONE,
      DiskMountManager::MountPointInfo("/removable/usb", "/removable/usb",
                                       chromeos::MOUNT_TYPE_DEVICE,
                                       chromeos::disks::MOUNT_CONDITION_NONE));

  // Mount a zip archive in the stick.
  volume_manager()->OnMountEvent(
      DiskMountManager::MOUNTING, chromeos::MOUNT_ERROR_NONE,
      DiskMountManager::MountPointInfo("/removable/usb/1.zip", "/archive/1",
                                       chromeos::MOUNT_TYPE_ARCHIVE,
                                       chromeos::disks::MOUNT_CONDITION_NONE));
  base::WeakPtr<Volume> volume = volume_manager()->FindVolumeById("archive:1");
  ASSERT_TRUE(volume.get());
  EXPECT_EQ("/archive/1", volume->mount_path().AsUTF8Unsafe());
  EXPECT_EQ(2u, observer.events().size());

  // Mount a zip archive in the previous zip archive.
  volume_manager()->OnMountEvent(
      DiskMountManager::MOUNTING, chromeos::MOUNT_ERROR_NONE,
      DiskMountManager::MountPointInfo("/archive/1/2.zip", "/archive/2",
                                       chromeos::MOUNT_TYPE_ARCHIVE,
                                       chromeos::disks::MOUNT_CONDITION_NONE));
  base::WeakPtr<Volume> second_volume =
      volume_manager()->FindVolumeById("archive:2");
  ASSERT_TRUE(second_volume.get());
  EXPECT_EQ("/archive/2", second_volume->mount_path().AsUTF8Unsafe());
  EXPECT_EQ(3u, observer.events().size());

  // A zip file is mounted from other profile. It must be ignored in the current
  // VolumeManager.
  volume_manager()->OnMountEvent(
      DiskMountManager::MOUNTING, chromeos::MOUNT_ERROR_NONE,
      DiskMountManager::MountPointInfo(
          "/other/profile/drive/folder/3.zip", "/archive/3",
          chromeos::MOUNT_TYPE_ARCHIVE, chromeos::disks::MOUNT_CONDITION_NONE));
  base::WeakPtr<Volume> third_volume =
      volume_manager()->FindVolumeById("archive:3");
  ASSERT_FALSE(third_volume.get());
  EXPECT_EQ(3u, observer.events().size());
}

TEST_F(VolumeManagerTest, MTPPlugAndUnplug) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  storage_monitor::StorageInfo info(
      storage_monitor::StorageInfo::MakeDeviceId(
          storage_monitor::StorageInfo::MTP_OR_PTP, "dummy-device-id"),
      FILE_PATH_LITERAL("/dummy/device/location"),
      base::UTF8ToUTF16("label"),
      base::UTF8ToUTF16("vendor"),
      base::UTF8ToUTF16("model"),
      12345 /* size */);

  storage_monitor::StorageInfo non_mtp_info(
      storage_monitor::StorageInfo::MakeDeviceId(
          storage_monitor::StorageInfo::FIXED_MASS_STORAGE, "dummy-device-id2"),
      FILE_PATH_LITERAL("/dummy/device/location2"),
      base::UTF8ToUTF16("label2"),
      base::UTF8ToUTF16("vendor2"),
      base::UTF8ToUTF16("model2"),
      12345 /* size */);

  // Attach
  volume_manager()->OnRemovableStorageAttached(info);
  ASSERT_EQ(1u, observer.events().size());
  EXPECT_EQ(LoggingObserver::Event::VOLUME_MOUNTED, observer.events()[0].type);

  base::WeakPtr<Volume> volume = volume_manager()->FindVolumeById("mtp:model");
  EXPECT_EQ(VOLUME_TYPE_MTP, volume->type());

  // Non MTP events from storage monitor are ignored.
  volume_manager()->OnRemovableStorageAttached(non_mtp_info);
  EXPECT_EQ(1u, observer.events().size());

  // Detach
  volume_manager()->OnRemovableStorageDetached(info);
  ASSERT_EQ(2u, observer.events().size());
  EXPECT_EQ(LoggingObserver::Event::VOLUME_UNMOUNTED,
            observer.events()[1].type);

  EXPECT_FALSE(volume.get());
}

TEST_F(VolumeManagerTest, OnRenameEvent_Started) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnRenameEvent(DiskMountManager::RENAME_STARTED,
                                  chromeos::RENAME_ERROR_NONE, "device1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::RENAME_STARTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_TRUE(event.success);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnRenameEvent_StartFailed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnRenameEvent(DiskMountManager::RENAME_STARTED,
                                  chromeos::RENAME_ERROR_UNKNOWN, "device1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::RENAME_STARTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_FALSE(event.success);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnRenameEvent_Completed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnRenameEvent(DiskMountManager::RENAME_COMPLETED,
                                  chromeos::RENAME_ERROR_NONE, "device1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::RENAME_COMPLETED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_TRUE(event.success);

  // When "rename" is successfully done, VolumeManager requests to mount it.
  ASSERT_EQ(1U, disk_mount_manager_->mount_requests().size());
  const FakeDiskMountManager::MountRequest& mount_request =
      disk_mount_manager_->mount_requests()[0];
  EXPECT_EQ("device1", mount_request.source_path);
  EXPECT_EQ("", mount_request.source_format);
  EXPECT_EQ(chromeos::MOUNT_TYPE_DEVICE, mount_request.type);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnRenameEvent_CompletedFailed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnRenameEvent(DiskMountManager::RENAME_COMPLETED,
                                  chromeos::RENAME_ERROR_UNKNOWN, "device1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::RENAME_COMPLETED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_FALSE(event.success);

  EXPECT_EQ(1U, disk_mount_manager_->mount_requests().size());

  volume_manager()->RemoveObserver(&observer);
}

}  // namespace file_manager
