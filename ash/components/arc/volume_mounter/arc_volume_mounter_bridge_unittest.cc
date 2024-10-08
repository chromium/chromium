// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/volume_mounter/arc_volume_mounter_bridge.h"

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/mojom/volume_mounter.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_volume_mounter_instance.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "chromeos/components/disks/disks_prefs.h"
#include "components/account_id/account_id.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/cros-disks/dbus-constants.h"

namespace arc {
namespace {

using ash::disks::DiskMountManager;

constexpr int kValidMediaProviderUID = 10062;
constexpr char kMyFilesMountPath[] = "/home/chronos/user/MyFiles";

void SetArcAndroidSdkVersionForTesting(int version) {
  base::SysInfo::SetChromeOSVersionInfoForTest(
      base::StringPrintf("CHROMEOS_ARC_ANDROID_SDK_VERSION=%d", version),
      base::Time::Now());
}

void ResetArcAndroidSdkVersionForTesting(int version) {
  base::SysInfo::ResetChromeOSVersionInfoForTest();
  SetArcAndroidSdkVersionForTesting(version);
}

class FakeArcVolumeMounterBridgeDelegate
    : public ArcVolumeMounterBridge::Delegate {
 public:
  bool IsWatchingFileSystemChanges() override { return true; }

  void StartWatchingRemovableMedia(const std::string& fs_uuid,
                                   const std::string& mount_path,
                                   base::OnceClosure callback) override {
    if (!watched_removable_media_.insert(mount_path).second) {
      LOG(ERROR) << "Attempted to start watching already watched "
                 << "removable media: " << mount_path;
    }
    std::move(callback).Run();
  }

  void StopWatchingRemovableMedia(const std::string& mount_path) override {
    if (watched_removable_media_.erase(mount_path) < 1) {
      LOG(ERROR) << "Attempted to stop watching unwatched removable media: "
                 << mount_path;
    }
  }

  bool is_watching(const std::string& mount_path) {
    return base::Contains(watched_removable_media_, mount_path);
  }

 private:
  std::set<std::string> watched_removable_media_;
};

class ArcVolumeMounterBridgeTest : public testing::Test {
 protected:
  ArcVolumeMounterBridgeTest() = default;
  ArcVolumeMounterBridgeTest(const ArcVolumeMounterBridgeTest&) = delete;
  ArcVolumeMounterBridgeTest& operator=(const ArcVolumeMounterBridgeTest&) =
      delete;
  ~ArcVolumeMounterBridgeTest() override = default;

  void SetUp() override {
    ash::UpstartClient::InitializeFake();
    ash::disks::DiskMountManager::InitializeForTesting(
        new ash::disks::FakeDiskMountManager());

    bridge_ = std::make_unique<ArcVolumeMounterBridge>(
        &context_, arc_service_manager_.arc_bridge_service());
    bridge_->Initialize(&delegate_);

    arc::prefs::RegisterLocalStatePrefs(context_.pref_registry());
    arc::prefs::RegisterProfilePrefs(context_.pref_registry());
    disks::prefs::RegisterProfilePrefs(context_.pref_registry());

    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->volume_mounter()
        ->SetInstance(&volume_mounter_instance_);
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->volume_mounter());

    // Set a non-empty fake user ID.
    ArcServiceManager::Get()->set_account_id(
        AccountId::FromUserEmail("test@gmail.com"));

    // Assume ARC++ P by default to simplify test cases that check sending mount
    // point information to ARC.
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        ash::switches::kEnableArcVm);
    SetArcAndroidSdkVersionForTesting(arc::kArcVersionP);
  }

  void TearDown() override {
    base::SysInfo::ResetChromeOSVersionInfoForTest();
    bridge_.reset();
    ash::disks::DiskMountManager::Shutdown();
    ash::UpstartClient::Shutdown();
  }

  ArcVolumeMounterBridge* bridge() { return bridge_.get(); }

  PrefService* prefs() { return context_.prefs(); }

  FakeVolumeMounterInstance* volume_mounter_instance() {
    return &volume_mounter_instance_;
  }

  ash::disks::DiskMountManager* disk_mount_manager() {
    return ash::disks::DiskMountManager::GetInstance();
  }

  FakeArcVolumeMounterBridgeDelegate* delegate() { return &delegate_; }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  FakeVolumeMounterInstance volume_mounter_instance_;
  user_prefs::TestBrowserContextWithPrefs context_;
  FakeArcVolumeMounterBridgeDelegate delegate_;
  std::unique_ptr<ArcVolumeMounterBridge> bridge_;
};

TEST_F(ArcVolumeMounterBridgeTest, OnMountEvent_RemovableMedia) {
  constexpr char kDevicePath[] = "/dev/foo";
  constexpr char kMountPath[] = "/media/removable/UNTITLED";
  constexpr char kFsUUID[] = "0123-abcd";
  constexpr char kDeviceLabel[] = "removable_label";

  disk_mount_manager()->AddDiskForTest(ash::disks::Disk::Builder()
                                           .SetDevicePath(kDevicePath)
                                           .SetMountPath(kMountPath)
                                           .SetFileSystemUUID(kFsUUID)
                                           .SetDeviceLabel(kDeviceLabel)
                                           .SetDeviceType(ash::DeviceType::kUSB)
                                           .Build());

  ash::MountPoint mount_point(kDevicePath, kMountPath);

  bridge()->OnMountEvent(DiskMountManager::MountEvent::MOUNTING,
                         ash::MountError::kSuccess, mount_point);

  // Check that the mount event is propagated to ARC and the delegate.
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 1);
  mojom::MountPointInfoPtr mount_point_info =
      volume_mounter_instance()->GetMountPointInfo(kMountPath);
  EXPECT_FALSE(mount_point_info.is_null());
  EXPECT_EQ(mount_point_info->mount_event,
            DiskMountManager::MountEvent::MOUNTING);
  EXPECT_EQ(mount_point_info->fs_uuid, kFsUUID);
  EXPECT_EQ(mount_point_info->label, kDeviceLabel);
  EXPECT_EQ(mount_point_info->device_type, ash::DeviceType::kUSB);
  EXPECT_FALSE(mount_point_info->visible);
  EXPECT_TRUE(delegate()->is_watching(kMountPath));

  bridge()->OnMountEvent(DiskMountManager::MountEvent::UNMOUNTING,
                         ash::MountError::kSuccess, mount_point);

  // Check that the unmount event is propagated to ARC and the delegate.
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 2);
  mount_point_info = volume_mounter_instance()->GetMountPointInfo(kMountPath);
  EXPECT_FALSE(mount_point_info.is_null());
  EXPECT_EQ(mount_point_info->mount_event,
            DiskMountManager::MountEvent::UNMOUNTING);
  EXPECT_EQ(mount_point_info->fs_uuid, kFsUUID);
  EXPECT_FALSE(delegate()->is_watching(kMountPath));
}

TEST_F(ArcVolumeMounterBridgeTest, OnMountEvent_IgnoreNonRemovableMedia) {
  // Only the (un)mount events for /media/removable/* are propagated.

  bridge()->OnMountEvent(DiskMountManager::MountEvent::MOUNTING,
                         ash::MountError::kSuccess,
                         ash::MountPoint("/dev/foo", "/media/archive/foo.zip"));
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 0);
  EXPECT_FALSE(delegate()->is_watching("/media/archive/foo.zip"));

  bridge()->OnMountEvent(DiskMountManager::MountEvent::UNMOUNTING,
                         ash::MountError::kSuccess,
                         ash::MountPoint("/dev/foo", "/media/REMOVABLE/foo"));
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 0);
  EXPECT_FALSE(delegate()->is_watching("/media/REMOVABLE/foo"));
}

TEST_F(ArcVolumeMounterBridgeTest, OnMountEvent_MountError) {
  // Mount event with errors are not propagated to ARC.
  bridge()->OnMountEvent(DiskMountManager::MountEvent::MOUNTING,
                         ash::MountError::kInvalidArgument,
                         ash::MountPoint("/dev/foo", "/media/removable/FOO"));
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 0);
  EXPECT_FALSE(delegate()->is_watching("/media/removable/FOO"));
}

TEST_F(ArcVolumeMounterBridgeTest, OnMountEvent_ExternalStorageDisabled) {
  constexpr char kDevicePath1[] = "/dev/foo";
  constexpr char kDevicePath2[] = "/dev/bar";
  constexpr char kRemovableMountPath1[] = "/media/removable/FOO";
  constexpr char kRemovableMountPath2[] = "/media/removable/BAR";

  disk_mount_manager()->AddDiskForTest(ash::disks::Disk::Builder()
                                           .SetDevicePath(kDevicePath1)
                                           .SetMountPath(kRemovableMountPath1)
                                           .Build());

  // Mount a removable media before disabling external storage.
  bridge()->OnMountEvent(DiskMountManager::MountEvent::MOUNTING,
                         ash::MountError::kSuccess,
                         ash::MountPoint(kDevicePath1, kRemovableMountPath1));
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 1);
  EXPECT_TRUE(delegate()->is_watching(kRemovableMountPath1));

  // Disable external storage by policy.
  prefs()->SetBoolean(disks::prefs::kExternalStorageDisabled, true);

  // No new mount events are propagated to the instance or the delegate.
  bridge()->OnMountEvent(DiskMountManager::MountEvent::MOUNTING,
                         ash::MountError::kSuccess,
                         ash::MountPoint(kDevicePath2, kRemovableMountPath2));
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 1);
  EXPECT_FALSE(delegate()->is_watching(kRemovableMountPath2));

  // Unmounting events are propagated.
  bridge()->OnMountEvent(DiskMountManager::MountEvent::UNMOUNTING,
                         ash::MountError::kSuccess,
                         ash::MountPoint(kDevicePath1, kRemovableMountPath1));
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 2);
  EXPECT_FALSE(delegate()->is_watching(kRemovableMountPath1));
}

TEST_F(ArcVolumeMounterBridgeTest, OnMountEvent_ExternalStorageAccess) {
  constexpr char kDevicePath1[] = "/dev/foo";
  constexpr char kDevicePath2[] = "/dev/bar";
  constexpr char kRemovableMountPath1[] = "/media/removable/FOO";
  constexpr char kRemovableMountPath2[] = "/media/removable/BAR";

  disk_mount_manager()->AddDiskForTest(ash::disks::Disk::Builder()
                                           .SetDevicePath(kDevicePath1)
                                           .SetMountPath(kRemovableMountPath1)
                                           .Build());

  // Mount a removable media before disabling external storage access.
  bridge()->OnMountEvent(DiskMountManager::MountEvent::MOUNTING,
                         ash::MountError::kSuccess,
                         ash::MountPoint(kDevicePath1, kRemovableMountPath1));
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 1);
  EXPECT_TRUE(delegate()->is_watching(kRemovableMountPath1));

  // Disable external storage access by feature.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kExternalStorageAccess);

  // No new mount events are propagated to the instance.
  bridge()->OnMountEvent(DiskMountManager::MountEvent::MOUNTING,
                         ash::MountError::kSuccess,
                         ash::MountPoint(kDevicePath2, kRemovableMountPath2));
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 1);
  EXPECT_FALSE(delegate()->is_watching(kRemovableMountPath2));

  // Unmounting events are propagated.
  bridge()->OnMountEvent(DiskMountManager::MountEvent::UNMOUNTING,
                         ash::MountError::kSuccess,
                         ash::MountPoint(kDevicePath1, kRemovableMountPath1));
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 2);
  EXPECT_FALSE(delegate()->is_watching(kRemovableMountPath1));
}

TEST_F(ArcVolumeMounterBridgeTest, OnMountEvent_VisibleToAndroidApps) {
  constexpr char kDevicePath[] = "/dev/foo";
  constexpr char kMountPath[] = "/media/removable/UNTITLED";
  constexpr char kFsUUID[] = "0123-abcd";

  disk_mount_manager()->AddDiskForTest(ash::disks::Disk::Builder()
                                           .SetDevicePath(kDevicePath)
                                           .SetFileSystemUUID(kFsUUID)
                                           .Build());

  // Add the disk to the set of visible external storages from Android apps.
  base::Value::List visible_external_storages;
  visible_external_storages.Append(kFsUUID);
  prefs()->SetList(prefs::kArcVisibleExternalStorages,
                   std::move(visible_external_storages));

  bridge()->OnMountEvent(DiskMountManager::MountEvent::MOUNTING,
                         ash::MountError::kSuccess,
                         ash::MountPoint(kDevicePath, kMountPath));

  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 1);
  mojom::MountPointInfoPtr mount_point_info =
      volume_mounter_instance()->GetMountPointInfo(kMountPath);
  EXPECT_TRUE(mount_point_info->visible);
  EXPECT_TRUE(delegate()->is_watching(kMountPath));
}

TEST_F(ArcVolumeMounterBridgeTest, SendAllMountEvents) {
  constexpr char kDevicePath1[] = "/dev/foo";
  constexpr char kDevicePath2[] = "/dev/bar";
  constexpr char kRemovableMountPath[] = "/media/removable/FOO";
  constexpr char kNonRemovableMountPath[] = "/mount/path/BAR";

  disk_mount_manager()->AddDiskForTest(ash::disks::Disk::Builder()
                                           .SetDevicePath(kDevicePath1)
                                           .SetMountPath(kRemovableMountPath)
                                           .Build());
  disk_mount_manager()->AddDiskForTest(ash::disks::Disk::Builder()
                                           .SetDevicePath(kDevicePath2)
                                           .SetMountPath(kNonRemovableMountPath)
                                           .Build());
  disk_mount_manager()->AddMountPointForTest(
      {kDevicePath1, kRemovableMountPath, ash::MountType::kDevice});
  disk_mount_manager()->AddMountPointForTest(
      {kDevicePath2, kNonRemovableMountPath, ash::MountType::kDevice});

  bridge()->SendAllMountEvents();

  // Mount point info is propagated for /media/removable/FOO and MyFiles, but
  // not for /mount/path/BAR.
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 2);
  mojom::MountPointInfoPtr mount_point_info_removable =
      volume_mounter_instance()->GetMountPointInfo(kRemovableMountPath);
  EXPECT_FALSE(mount_point_info_removable.is_null());
  EXPECT_EQ(mount_point_info_removable->mount_event,
            DiskMountManager::MountEvent::MOUNTING);

  mojom::MountPointInfoPtr mount_point_info_non_removable =
      volume_mounter_instance()->GetMountPointInfo(kNonRemovableMountPath);
  EXPECT_TRUE(mount_point_info_non_removable.is_null());

  mojom::MountPointInfoPtr mount_point_info_myfiles =
      volume_mounter_instance()->GetMountPointInfo(kMyFilesMountPath);
  EXPECT_FALSE(mount_point_info_myfiles.is_null());
  EXPECT_EQ(mount_point_info_myfiles->mount_event,
            DiskMountManager::MountEvent::MOUNTING);
}

TEST_F(ArcVolumeMounterBridgeTest, SendAllMountEvents_ExternalStorageDisabled) {
  constexpr char kDevicePath[] = "/dev/foo";
  constexpr char kRemovableMountPath[] = "/media/removable/FOO";

  disk_mount_manager()->AddDiskForTest(ash::disks::Disk::Builder()
                                           .SetDevicePath(kDevicePath)
                                           .SetMountPath(kRemovableMountPath)
                                           .Build());
  disk_mount_manager()->AddMountPointForTest(
      {kDevicePath, kRemovableMountPath, ash::MountType::kDevice});

  // Disable external storage by policy.
  prefs()->SetBoolean(disks::prefs::kExternalStorageDisabled, true);

  bridge()->SendAllMountEvents();

  // Mount point info is propagated for MyFiles, but not for
  // /media/removable/FOO.
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 1);

  mojom::MountPointInfoPtr mount_point_info_removable =
      volume_mounter_instance()->GetMountPointInfo(kRemovableMountPath);
  EXPECT_TRUE(mount_point_info_removable.is_null());

  mojom::MountPointInfoPtr mount_point_info_myfiles =
      volume_mounter_instance()->GetMountPointInfo(kMyFilesMountPath);
  EXPECT_FALSE(mount_point_info_myfiles.is_null());
  EXPECT_EQ(mount_point_info_myfiles->mount_event,
            DiskMountManager::MountEvent::MOUNTING);
}

TEST_F(ArcVolumeMounterBridgeTest, SendAllMountEvents_ExternalStorageAccess) {
  constexpr char kDevicePath[] = "/dev/foo";
  constexpr char kRemovableMountPath[] = "/media/removable/FOO";

  disk_mount_manager()->AddDiskForTest(ash::disks::Disk::Builder()
                                           .SetDevicePath(kDevicePath)
                                           .SetMountPath(kRemovableMountPath)
                                           .Build());
  disk_mount_manager()->AddMountPointForTest(
      {kDevicePath, kRemovableMountPath, ash::MountType::kDevice});

  // Disable external storage access by feature.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kExternalStorageAccess);

  bridge()->SendAllMountEvents();

  // Mount point info is propagated for MyFiles, but not for
  // /media/removable/FOO.
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 1);

  mojom::MountPointInfoPtr mount_point_info_removable =
      volume_mounter_instance()->GetMountPointInfo(kRemovableMountPath);
  EXPECT_TRUE(mount_point_info_removable.is_null());

  mojom::MountPointInfoPtr mount_point_info_myfiles =
      volume_mounter_instance()->GetMountPointInfo(kMyFilesMountPath);
  EXPECT_FALSE(mount_point_info_myfiles.is_null());
  EXPECT_EQ(mount_point_info_myfiles->mount_event,
            DiskMountManager::MountEvent::MOUNTING);
}

TEST_F(ArcVolumeMounterBridgeTest, RequestAllMountPoints_P_Container) {
  // Use ARC++ (container) P.
  ResetArcAndroidSdkVersionForTesting(arc::kArcVersionP);
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ash::switches::kEnableArcVm);

  // In ARC++ P, the bridge is always ready to send mount points.
  bridge()->RequestAllMountPoints();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 1);
}

TEST_F(ArcVolumeMounterBridgeTest, RequestAllMountPoints_R_VM) {
  // Use ARCVM R.
  ResetArcAndroidSdkVersionForTesting(arc::kArcVersionR);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVm);

  // Starting job succeeds only for arcvm-media-sharing-services.
  ash::FakeUpstartClient::Get()->set_start_job_cb(base::BindRepeating(
      [](const std::string& job_name, const std::vector<std::string>& env) {
        return ash::FakeUpstartClient::StartJobResult(
            job_name == kArcVmMediaSharingServicesJobName);
      }));

  // In ARCVM R, the bridge is not ready to send mount points before
  // arcvm-media-sharing-services is started.
  bridge()->RequestAllMountPoints();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 0);

  // Start arcvm-media-sharing-services.
  base::test::TestFuture<bool> future;
  bridge()->SetUpExternalStorageMountPoints(kValidMediaProviderUID,
                                            future.GetCallback());
  EXPECT_TRUE(future.Get());

  // The bridge can now send the mount point info to the instance.
  bridge()->RequestAllMountPoints();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 1);

  // Trigger ArcVolumeMounterBridge::OnConnectionClosed().
  ArcServiceManager::Get()
      ->arc_bridge_service()
      ->volume_mounter()
      ->CloseInstance(volume_mounter_instance());

  // The bridge can no longer send the mount point info to the instance.
  bridge()->RequestAllMountPoints();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 1);
}

TEST_F(ArcVolumeMounterBridgeTest, RequestAllMountPoints_R_Container) {
  // Use ARC++ (container) R.
  ResetArcAndroidSdkVersionForTesting(arc::kArcVersionR);
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ash::switches::kEnableArcVm);

  // Starting job succeeds only for arcpp-media-sharing-services.
  ash::FakeUpstartClient::Get()->set_start_job_cb(base::BindRepeating(
      [](const std::string& job_name, const std::vector<std::string>& env) {
        return ash::FakeUpstartClient::StartJobResult(
            job_name == kArcppMediaSharingServicesJobName);
      }));

  // In R container, the bridge is not ready to send mount points before
  // arcpp-media-sharing-services is started.
  bridge()->RequestAllMountPoints();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 0);

  // Start arcpp-media-sharing-services.
  base::test::TestFuture<bool> future;
  bridge()->SetUpExternalStorageMountPoints(kValidMediaProviderUID,
                                            future.GetCallback());
  EXPECT_TRUE(future.Get());

  // The bridge can now send the mount point info to the instance.
  bridge()->RequestAllMountPoints();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 1);

  // Trigger ArcVolumeMounterBridge::OnConnectionClosed().
  ArcServiceManager::Get()
      ->arc_bridge_service()
      ->volume_mounter()
      ->CloseInstance(volume_mounter_instance());

  // The bridge can no longer send the mount point info to the instance.
  bridge()->RequestAllMountPoints();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 1);
}

TEST_F(ArcVolumeMounterBridgeTest,
       SetUpExternalStorageMountPoints_JobAlreadyStarted) {
  // Use ARCVM R.
  ResetArcAndroidSdkVersionForTesting(arc::kArcVersionR);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVm);

  // Inject |kAlreadyStartedError| for starting arcvm-media-sharing-services.
  ash::FakeUpstartClient::Get()->set_start_job_cb(base::BindRepeating(
      [](const std::string& job_name, const std::vector<std::string>& env) {
        if (job_name == kArcVmMediaSharingServicesJobName) {
          return ash::FakeUpstartClient::StartJobResult(
              false /* success */, ash::UpstartClient::kAlreadyStartedError);
        }
        return ash::FakeUpstartClient::StartJobResult(false /* success */);
      }));

  // SetUpExternalStorageMountPoints still succeeds.
  base::test::TestFuture<bool> future;
  bridge()->SetUpExternalStorageMountPoints(kValidMediaProviderUID,
                                            future.GetCallback());
  EXPECT_TRUE(future.Get());

  // The bridge can send the mount point info to the instance.
  bridge()->RequestAllMountPoints();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 1);
}

TEST_F(ArcVolumeMounterBridgeTest,
       SetUpExternalStorageMountPoints_InvalidMediaProviderUID) {
  // Use ARCVM R.
  ResetArcAndroidSdkVersionForTesting(arc::kArcVersionR);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVm);

  // When called with invalid MediaProvider UID, SetUpExternalStorageMountPoints
  // returns false in the callback.
  base::test::TestFuture<bool> future1, future2;
  bridge()->SetUpExternalStorageMountPoints(20000 /* media_provider_uid */,
                                            future1.GetCallback());
  EXPECT_FALSE(future1.Get());
  bridge()->SetUpExternalStorageMountPoints(9999 /* media_provider_uid */,
                                            future2.GetCallback());
  EXPECT_FALSE(future2.Get());

  // The bridge is still not ready to send the mount point info.
  bridge()->RequestAllMountPoints();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 0);
}

TEST_F(ArcVolumeMounterBridgeTest,
       SetUpExternalStorageMountPoints_StartUpstartJobFailed) {
  // Use ARCVM R.
  ResetArcAndroidSdkVersionForTesting(arc::kArcVersionR);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVm);

  // Inject failure for starting arcvm-media-sharing-services.
  ash::FakeUpstartClient::Get()->set_start_job_cb(base::BindRepeating(
      [](const std::string& job_name, const std::vector<std::string>& env) {
        return ash::FakeUpstartClient::StartJobResult(
            job_name != kArcVmMediaSharingServicesJobName);
      }));

  // SetUpExternalStorageMountPoints returns false in the callback.
  base::test::TestFuture<bool> future;
  bridge()->SetUpExternalStorageMountPoints(kValidMediaProviderUID,
                                            future.GetCallback());
  EXPECT_FALSE(future.Get());

  // The bridge is still not ready to send the mount point info.
  bridge()->RequestAllMountPoints();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(volume_mounter_instance()->num_on_mount_event_called(), 0);
}

TEST_F(ArcVolumeMounterBridgeTest, PrepareForRemovableMediaUnmount_Success) {
  base::test::TestFuture<bool> future1, future2;
  // Test that PrepareForRemovableMediaUnmount() can be called serially
  // multiple times and calls back the correct callback.
  bridge()->PrepareForRemovableMediaUnmount(
      ash::CrosDisksClient::GetRemovableDiskMountPoint().Append("UNTITLED1"),
      base::Seconds(0), future1.GetCallback());
  EXPECT_TRUE(future1.Get());
  bridge()->PrepareForRemovableMediaUnmount(
      ash::CrosDisksClient::GetRemovableDiskMountPoint().Append("UNTITLED2"),
      base::Seconds(0), future2.GetCallback());
  EXPECT_TRUE(future2.Get());
}

TEST_F(ArcVolumeMounterBridgeTest,
       PrepareForRemovableMediaUnmount_UnresponsiveARC) {
  // Test the situation where ARC does not call the
  // PrepareForRemovableMediaUnmount callback.
  volume_mounter_instance()
      ->set_call_prepare_for_removable_media_unmount_callback(false);

  base::test::TestFuture<bool> future;
  bridge()->PrepareForRemovableMediaUnmount(
      ash::CrosDisksClient::GetRemovableDiskMountPoint().Append("UNTITLED"),
      base::Seconds(0), future.GetCallback());
  // The callback will be called with false due to timeout.
  EXPECT_FALSE(future.Get());
}

}  // namespace
}  // namespace arc
