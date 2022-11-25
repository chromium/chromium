// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FUSEBOX_MOUNTER_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FUSEBOX_MOUNTER_H_

#include <map>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"

namespace file_manager {

using FuseBoxDiskMountManager = ::ash::disks::DiskMountManager;

using FuseBoxMountInfo = ::ash::disks::DiskMountManager::MountPoint;

using UrlReadOnlyPair = std::pair<std::string, bool>;

class FuseBoxMounter {
 public:
  FuseBoxMounter();
  FuseBoxMounter(const FuseBoxMounter&) = delete;
  FuseBoxMounter& operator=(const FuseBoxMounter&) = delete;
  ~FuseBoxMounter();

  // Attach fusebox storage: adds fusebox daemon <mount-point>/subdir used to
  // serve the content of the Chrome storage::FileSystemURL |url| via FUSE to
  // the Linux file system. The <mount-point>/subdir content is read-write by
  // default: use |read_only| true to make the content read-only.
  void AttachStorage(const std::string& subdir,
                     const std::string& url,
                     bool read_only);

  // Detach fusebox storage: removes fusebox <mountpoint>/subdir.
  void DetachStorage(const std::string& subdir);

  // Mount fusebox daemon.
  void Mount(FuseBoxDiskMountManager* disk_mount_manager);

  // Unmount fusebox daemon.
  void Unmount(FuseBoxDiskMountManager* disk_mount_manager);

 private:
  // Mount response.
  void MountResponse(ash::MountError error, const FuseBoxMountInfo& info);

  // Unmount response.
  void UnmountResponse(ash::MountError error);

 private:
  // True if this fusebox instance is mounted.
  bool mounted_ = false;

  // A list of `AttachStorage` invocations that were called prior to the fusebox
  // mounting, these get called when fusebox successfully mounts.
  std::map<std::string, UrlReadOnlyPair> pending_attach_storage_calls_;

  // base::WeakPtr{this} factory.
  base::WeakPtrFactory<FuseBoxMounter> weak_ptr_factory_{this};
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FUSEBOX_MOUNTER_H_
