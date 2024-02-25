// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/device_event_router.h"

#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chromeos/ash/components/disks/disk.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace {

namespace file_manager_private = extensions::api::file_manager_private;
using ::ash::disks::Disk;

const char kTestDevicePath[] = "/device/test";

struct DeviceEvent {
  extensions::api::file_manager_private::DeviceEventType type;
  std::string device_path;
  std::string device_label;
};

// DeviceEventRouter implementation for testing.
class DeviceEventRouterImpl : public DeviceEventRouter {
 public:
  explicit DeviceEventRouterImpl(
      SystemNotificationManager* notification_manager)
      : DeviceEventRouter(notification_manager, base::Seconds(0)),
        external_storage_disabled(false) {}

  DeviceEventRouterImpl(const DeviceEventRouterImpl&) = delete;
  DeviceEventRouterImpl& operator=(const DeviceEventRouterImpl&) = delete;

  ~DeviceEventRouterImpl() override = default;

  // DeviceEventRouter overrides.
  void OnDeviceEvent(file_manager_private::DeviceEventType type,
                     const std::string& device_path,
                     const std::string& device_label) override {
    DeviceEvent event;
    event.type = type;
    event.device_path = device_path;
    event.device_label = device_label;
    events.push_back(event);
  }

  // DeviceEventRouter overrides.
  bool IsExternalStorageDisabled() override {
    return external_storage_disabled;
  }

  // List of dispatched events.
  std::vector<DeviceEvent> events;

  // Flag returned by |IsExternalStorageDisabled|.
  bool external_storage_disabled;
};

}  // namespace

class DeviceEventRouterTest : public testing::Test {
 protected:
  void SetUp() override {
    device_event_router = std::make_unique<DeviceEventRouterImpl>(nullptr);
  }

  // Creates a disk instance with |device_path| and |mount_path| for testing.
  Disk CreateTestDisk(const std::string& device_path,
                      const std::string& mount_path,
                      bool is_read_only_hardware,
                      bool is_mounted) {
    return *Disk::Builder()
                .SetDevicePath(device_path)
                .SetMountPath(mount_path)
                .SetStorageDevicePath(device_path)
                .SetIsReadOnlyHardware(is_read_only_hardware)
                .SetFileSystemType("vfat")
                .SetIsMounted(is_mounted)
                .Build();
  }

  std::unique_ptr<DeviceEventRouterImpl> device_event_router;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(DeviceEventRouterTest, AddAndRemoveDevice) {
  const Disk disk1 =
      CreateTestDisk("/device/test", "/mount/path1", false, true);
  const Disk disk1_unmounted = CreateTestDisk("/device/test", "", false, false);
  std::unique_ptr<Volume> volume(Volume::CreateForTesting(
      base::FilePath(FILE_PATH_LITERAL("/device/test")),
      base::FilePath(FILE_PATH_LITERAL("/mount/path1"))));
  device_event_router->OnDeviceAdded("/device/test");
  device_event_router->OnDiskAdded(disk1, true);
  device_event_router->OnVolumeMounted(ash::MountError::kSuccess,
                                       *volume.get());
  device_event_router->OnVolumeUnmounted(ash::MountError::kSuccess,
                                         *volume.get());
  device_event_router->OnDiskRemoved(disk1_unmounted);
  device_event_router->OnDeviceRemoved("/device/test");
  ASSERT_EQ(1u, device_event_router->events.size());
  EXPECT_EQ(file_manager_private::DeviceEventType::kRemoved,
            device_event_router->events[0].type);
  EXPECT_EQ("/device/test", device_event_router->events[0].device_path);
}

TEST_F(DeviceEventRouterTest, HardUnplugged) {
  const Disk disk1 =
      CreateTestDisk("/device/test", "/mount/path1", false, true);
  const Disk disk2 =
      CreateTestDisk("/device/test", "/mount/path2", false, true);
  device_event_router->OnDeviceAdded("/device/test");
  device_event_router->OnDiskAdded(disk1, true);
  device_event_router->OnDiskAdded(disk2, true);
  device_event_router->OnDiskRemoved(disk1);
  device_event_router->OnDiskRemoved(disk2);
  device_event_router->OnDeviceRemoved(kTestDevicePath);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, device_event_router->events.size());
  EXPECT_EQ(file_manager_private::DeviceEventType::kHardUnplugged,
            device_event_router->events[0].type);
  EXPECT_EQ("/device/test", device_event_router->events[0].device_path);
  EXPECT_EQ(file_manager_private::DeviceEventType::kRemoved,
            device_event_router->events[1].type);
  EXPECT_EQ("/device/test", device_event_router->events[1].device_path);
}

TEST_F(DeviceEventRouterTest, HardUnplugReadOnlyVolume) {
  const Disk disk1 = CreateTestDisk("/device/test", "/mount/path1", true, true);
  const Disk disk2 = CreateTestDisk("/device/test", "/mount/path2", true, true);
  device_event_router->OnDeviceAdded("/device/test");
  device_event_router->OnDiskAdded(disk1, true);
  device_event_router->OnDiskAdded(disk2, true);
  device_event_router->OnDiskRemoved(disk1);
  device_event_router->OnDiskRemoved(disk2);
  device_event_router->OnDeviceRemoved(kTestDevicePath);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, device_event_router->events.size());
  EXPECT_EQ(file_manager_private::DeviceEventType::kRemoved,
            device_event_router->events[0].type);
  EXPECT_EQ("/device/test", device_event_router->events[0].device_path);
  // Should not warn hard unplug because the volumes are read-only.
}

TEST_F(DeviceEventRouterTest, HardUnpluggedNotMounted) {
  const Disk disk1 = CreateTestDisk("/device/test", "", false, false);
  const Disk disk2 =
      CreateTestDisk("/device/test", "/mount/path2", true, false);
  device_event_router->OnDeviceAdded("/device/test");
  device_event_router->OnDiskAdded(disk1, true);
  device_event_router->OnDiskAdded(disk2, true);
  device_event_router->OnDiskRemoved(disk1);
  device_event_router->OnDiskRemoved(disk2);
  device_event_router->OnDeviceRemoved(kTestDevicePath);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, device_event_router->events.size());
  EXPECT_EQ(file_manager_private::DeviceEventType::kRemoved,
            device_event_router->events[0].type);
  EXPECT_EQ("/device/test", device_event_router->events[0].device_path);
  // Should not warn hard unplug because the volumes are not mounted.
}

}  // namespace file_manager
