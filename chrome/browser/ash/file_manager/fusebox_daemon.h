// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FUSEBOX_DAEMON_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FUSEBOX_DAEMON_H_

#include <map>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"

namespace file_manager {

class COMPONENT_EXPORT(ASH_DBUS_CROS_DISKS) FuseBoxDaemon
    : public base::RefCounted<FuseBoxDaemon> {
 public:
  // Returns the fusebox daemon. Creates the fusebox daemon if needed.
  static scoped_refptr<FuseBoxDaemon> GetInstance();

  // Attach fusebox storage: adds fusebox daemon <mount-point>/subdir used to
  // serve the content of the Chrome storage::FileSystemURL |url| via FUSE to
  // the Linux file system. The <mount-point>/subdir content is read-write by
  // default: use |read_only| true to make the content read-only.
  void AttachStorage(const std::string& subdir,
                     const std::string& url,
                     bool read_only);

  // Detach fusebox storage: removes fusebox daemon <mount-point>/subdir.
  void DetachStorage(const std::string& subdir);

 private:
  friend class base::RefCounted<FuseBoxDaemon>;

  FuseBoxDaemon();
  ~FuseBoxDaemon();

  // Cros-disks mount manager.
  using CrosDisksMountManager = ::ash::disks::DiskMountManager;
  raw_ptr<CrosDisksMountManager> cros_disks_mount_manager_ = nullptr;

  // FuseBox daemon URI: cros-disks URI protocol is fusebox://<mount-point>.
  static char* CrosDisksFuseBoxHelperURI() {
    static char kCrosDisksFuseBoxHelperURI[] = "fusebox://fusebox";
    return kCrosDisksFuseBoxHelperURI;
  }

  // Construction creates and mounts the cros-disks fusebox daemon.
  using FuseBoxMountInfo = ::ash::disks::DiskMountManager::MountPoint;
  void MountResponse(ash::MountError error, const FuseBoxMountInfo& info);

  // True if the fusebox daemon was successfully mounted.
  bool mounted_ = false;

  // List of `AttachStorage` calls prior to the fusebox daemon mount.
  using UrlReadOnlyPair = std::pair<std::string, bool>;
  std::map<std::string, UrlReadOnlyPair> pending_attach_storage_calls_;

  // base::WeakPtr{this} factory.
  base::WeakPtrFactory<FuseBoxDaemon> weak_ptr_factory_{this};
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FUSEBOX_DAEMON_H_
