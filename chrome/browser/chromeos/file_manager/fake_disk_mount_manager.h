// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FAKE_DISK_MOUNT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FAKE_DISK_MOUNT_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/disks/disk_mount_manager.h"

namespace file_manager {

class FakeDiskMountManager : public chromeos::disks::DiskMountManager {
 public:
  struct MountRequest {
    MountRequest(const std::string& source_path,
                 const std::string& source_format,
                 const std::string& mount_label,
                 const std::vector<std::string>& mount_options,
                 chromeos::MountType type,
                 chromeos::MountAccessMode access_mode);
    MountRequest(const MountRequest& other);
    ~MountRequest();

    std::string source_path;
    std::string source_format;
    std::string mount_label;
    std::vector<std::string> mount_options;
    chromeos::MountType type;
    chromeos::MountAccessMode access_mode;
  };

  struct RemountAllRequest {
    explicit RemountAllRequest(chromeos::MountAccessMode access_mode);
    chromeos::MountAccessMode access_mode;
  };

  FakeDiskMountManager();
  ~FakeDiskMountManager() override;

  const std::vector<MountRequest>& mount_requests() const {
    return mount_requests_;
  }
  const std::vector<std::string>& unmount_requests() const {
    return unmount_requests_;
  }
  const std::vector<RemountAllRequest>& remount_all_requests() const {
    return remount_all_requests_;
  }

  // Emulates that all mount request finished.
  // Return true if there was one or more mount request enqueued, or false
  // otherwise.
  bool FinishAllUnmountPathRequests();

  // Fails a future unmount request for |mount_path| with |error_code|.
  void FailUnmountRequest(const std::string& mount_path,
                          chromeos::MountError error_code);

  // DiskMountManager overrides.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  const DiskMap& disks() const override;
  const chromeos::disks::Disk* FindDiskBySourcePath(
      const std::string& source_path) const override;
  const MountPointMap& mount_points() const override;
  void EnsureMountInfoRefreshed(EnsureMountInfoRefreshedCallback callback,
                                bool force) override;
  void MountPath(const std::string& source_path,
                 const std::string& source_format,
                 const std::string& mount_label,
                 const std::vector<std::string>& mount_options,
                 chromeos::MountType type,
                 chromeos::MountAccessMode access_mode) override;
  // In order to simulate asynchronous invocation of callbacks after unmount
  // is finished, |callback| will be invoked only when
  // |FinishAllUnmountRequest()| is called.
  void UnmountPath(const std::string& mount_path,
                   UnmountPathCallback callback) override;
  void RemountAllRemovableDrives(
      chromeos::MountAccessMode access_mode) override;
  void FormatMountedDevice(const std::string& mount_path,
                           chromeos::disks::FormatFileSystemType filesystem,
                           const std::string& label) override;
  void RenameMountedDevice(const std::string& mount_path,
                           const std::string& volume_name) override;
  void UnmountDeviceRecursively(
      const std::string& device_path,
      UnmountDeviceRecursivelyCallbackType callback) override;

  bool AddDiskForTest(std::unique_ptr<chromeos::disks::Disk> disk) override;
  bool AddMountPointForTest(const MountPointInfo& mount_point) override;
  void InvokeDiskEventForTest(DiskEvent event,
                              const chromeos::disks::Disk* disk);

 private:
  base::ObserverList<Observer> observers_;
  base::queue<base::OnceClosure> pending_unmount_callbacks_;

  DiskMap disks_;
  MountPointMap mount_points_;

  std::vector<MountRequest> mount_requests_;
  std::vector<std::string> unmount_requests_;
  std::vector<RemountAllRequest> remount_all_requests_;
  std::map<std::string, chromeos::MountError> unmount_errors_;

  DISALLOW_COPY_AND_ASSIGN(FakeDiskMountManager);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FAKE_DISK_MOUNT_MANAGER_H_
