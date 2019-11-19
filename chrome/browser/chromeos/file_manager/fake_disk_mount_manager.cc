// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/fake_disk_mount_manager.h"

#include <utility>

#include "base/bind.h"
#include "chromeos/disks/disk.h"

namespace file_manager {

FakeDiskMountManager::MountRequest::MountRequest(
    const std::string& source_path,
    const std::string& source_format,
    const std::string& mount_label,
    const std::vector<std::string>& mount_options,
    chromeos::MountType type,
    chromeos::MountAccessMode access_mode)
    : source_path(source_path),
      source_format(source_format),
      mount_label(mount_label),
      mount_options(mount_options),
      type(type),
      access_mode(access_mode) {}

FakeDiskMountManager::MountRequest::MountRequest(const MountRequest& other) =
    default;

FakeDiskMountManager::MountRequest::~MountRequest() = default;

FakeDiskMountManager::RemountAllRequest::RemountAllRequest(
    chromeos::MountAccessMode access_mode)
    : access_mode(access_mode) {}

FakeDiskMountManager::FakeDiskMountManager() = default;

FakeDiskMountManager::~FakeDiskMountManager() = default;

void FakeDiskMountManager::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void FakeDiskMountManager::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

const chromeos::disks::DiskMountManager::DiskMap&
FakeDiskMountManager::disks() const {
  return disks_;
}

const chromeos::disks::Disk* FakeDiskMountManager::FindDiskBySourcePath(
    const std::string& source_path) const {
  DiskMap::const_iterator iter = disks_.find(source_path);
  return iter != disks_.end() ? iter->second.get() : nullptr;
}

const chromeos::disks::DiskMountManager::MountPointMap&
FakeDiskMountManager::mount_points() const {
  return mount_points_;
}

void FakeDiskMountManager::EnsureMountInfoRefreshed(
    EnsureMountInfoRefreshedCallback callback,
    bool force) {
  std::move(callback).Run(true);
}

void FakeDiskMountManager::MountPath(
    const std::string& source_path,
    const std::string& source_format,
    const std::string& mount_label,
    const std::vector<std::string>& mount_options,
    chromeos::MountType type,
    chromeos::MountAccessMode access_mode) {
  mount_requests_.emplace_back(source_path, source_format, mount_label,
                               mount_options, type, access_mode);

  const MountPointInfo mount_point(
      source_path,
      source_path,
      type,
      chromeos::disks::MOUNT_CONDITION_NONE);
  mount_points_.insert(make_pair(source_path, mount_point));
  for (auto& observer : observers_) {
    observer.OnMountEvent(DiskMountManager::MOUNTING,
                          chromeos::MOUNT_ERROR_NONE, mount_point);
  }
}

void FakeDiskMountManager::UnmountPath(const std::string& mount_path,
                                       UnmountPathCallback callback) {
  unmount_requests_.emplace_back(mount_path);

  chromeos::MountError error = chromeos::MOUNT_ERROR_NONE;
  auto unmount_iter = unmount_errors_.find(mount_path);
  if (unmount_iter != unmount_errors_.end()) {
    error = unmount_iter->second;
    unmount_errors_.erase(unmount_iter);
  } else {
    MountPointMap::iterator iter = mount_points_.find(mount_path);
    if (iter == mount_points_.end())
      return;

    const MountPointInfo mount_point = iter->second;
    mount_points_.erase(iter);
    for (auto& observer : observers_) {
      observer.OnMountEvent(DiskMountManager::UNMOUNTING,
                            chromeos::MOUNT_ERROR_NONE, mount_point);
    }
  }

  // Enqueue callback so that |FakeDiskMountManager::FinishAllUnmountRequest()|
  // can call them.
  if (callback) {
    // Some tests pass a null |callback|.
    pending_unmount_callbacks_.push(base::BindOnce(std::move(callback), error));
  }
}

void FakeDiskMountManager::RemountAllRemovableDrives(
    chromeos::MountAccessMode access_mode) {
  remount_all_requests_.emplace_back(access_mode);
}

bool FakeDiskMountManager::FinishAllUnmountPathRequests() {
  if (pending_unmount_callbacks_.empty())
    return false;

  while (!pending_unmount_callbacks_.empty()) {
    std::move(pending_unmount_callbacks_.front()).Run();
    pending_unmount_callbacks_.pop();
  }
  return true;
}

void FakeDiskMountManager::FailUnmountRequest(const std::string& mount_path,
                                              chromeos::MountError error_code) {
  unmount_errors_[mount_path] = error_code;
}

void FakeDiskMountManager::FormatMountedDevice(
    const std::string& mount_path,
    chromeos::disks::FormatFileSystemType filesystem,
    const std::string& label) {}

void FakeDiskMountManager::RenameMountedDevice(const std::string& mount_path,
                                               const std::string& volume_name) {
}

void FakeDiskMountManager::UnmountDeviceRecursively(
    const std::string& device_path,
    UnmountDeviceRecursivelyCallbackType callback) {}

bool FakeDiskMountManager::AddDiskForTest(
    std::unique_ptr<chromeos::disks::Disk> disk) {
  DCHECK(disk);
  return disks_.insert(make_pair(disk->device_path(), std::move(disk))).second;
}

bool FakeDiskMountManager::AddMountPointForTest(
    const MountPointInfo& mount_point) {
  return false;
}

void FakeDiskMountManager::InvokeDiskEventForTest(
    chromeos::disks::DiskMountManager::DiskEvent event,
    const chromeos::disks::Disk* disk) {
  for (auto& observer : observers_) {
    disk->is_auto_mountable() ? observer.OnAutoMountableDiskEvent(event, *disk)
                              : observer.OnBootDeviceDiskEvent(event, *disk);
  }
}

}  // namespace file_manager
