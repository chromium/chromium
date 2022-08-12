// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FUSEBOX_MOUNTER_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FUSEBOX_MOUNTER_H_

#include <string>

#include "ash/components/disks/disk_mount_manager.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"

namespace file_manager {

using FuseBoxDiskMountManager = ::ash::disks::DiskMountManager;

using FuseBoxMountInfo = ::ash::disks::DiskMountManager::MountPoint;

class FuseBoxMounter {
 public:
  // Creates mounter for fusebox mountpoint |uri|.
  static FuseBoxMounter* Create(std::string uri = "fusebox://fusebox");

  FuseBoxMounter(const FuseBoxMounter&) = delete;
  FuseBoxMounter& operator=(const FuseBoxMounter&) = delete;

  ~FuseBoxMounter();

  // Mount fusebox daemon.
  void Mount(FuseBoxDiskMountManager* disk_mount_manager);

  // Storage result: |error| is a POSIX errno value.
  using StorageResult = base::OnceCallback<void(int error)>;

  // Attach fusebox storage: adds fusebox daemon <mount-point>/subdir used to
  // serve the content of the Chrome storage::FileSystemURL |url| via FUSE to
  // the Linux file system. The <mount-point>/subdir content is read-write by
  // default: use |read_only| true to make the content read-only.
  void AttachStorage(const std::string& subdir,
                     const std::string& url,
                     bool read_only,
                     StorageResult callback);

  // Detach fusebox storage: removes fusebox <mountpoint>/subdir.
  void DetachStorage(const std::string& subdir, StorageResult callback);

  // Unmount fusebox daemon.
  void Unmount(FuseBoxDiskMountManager* disk_mount_manager);

 private:
  // Use Create().
  explicit FuseBoxMounter(std::string uri);

  // Returns base::WeakPtr{this}.
  base::WeakPtr<FuseBoxMounter> GetWeakPtr();

  // Mount response.
  void MountResponse(ash::MountError error, const FuseBoxMountInfo& info);

  // Unmount response.
  void UnmountResponse(ash::MountError error);

 private:
  // Cros-disks fusebox mountpoint URI.
  std::string const uri_;

  // True if |uri_| is mounted.
  bool mounted_ = false;

  // base::WeakPtr{this} factory.
  base::WeakPtrFactory<FuseBoxMounter> weak_ptr_factory_{this};
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FUSEBOX_MOUNTER_H_
