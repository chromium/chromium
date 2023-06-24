// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/volume_mounter/arc_volume_mounter_bridge.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/mojom/volume_mounter.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_volume_mounter_instance.h"
#include "ash/components/arc/test/test_browser_context.h"
#include "chrome/browser/ash/file_manager/fake_disk_mount_manager.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/components/disks/disks_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/cros-disks/dbus-constants.h"

namespace arc {
namespace {

using ash::disks::DiskMountManager;

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
    ash::disks::DiskMountManager::InitializeForTesting(
        new file_manager::FakeDiskMountManager());

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
  }

  void TearDown() override {
    bridge_.reset();
    ash::disks::DiskMountManager::Shutdown();
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

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  FakeVolumeMounterInstance volume_mounter_instance_;
  TestBrowserContext context_;
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

}  // namespace
}  // namespace arc
