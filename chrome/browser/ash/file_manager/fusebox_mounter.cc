// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/fusebox_mounter.h"

#include <cerrno>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"

namespace file_manager {

namespace {
// The first "fusebox" is the URI scheme that is matched by cros-disks'
// "fusebox_helper.cc". The second "fusebox" is the "foo" in "/media/fuse/foo".
const char kFuseBoxMounterURI[] = "fusebox://fusebox";
}  // namespace

FuseBoxMounter::FuseBoxMounter() = default;

FuseBoxMounter::~FuseBoxMounter() = default;

void FuseBoxMounter::AttachStorage(const std::string& subdir,
                                   const std::string& url,
                                   bool read_only) {
  if (!mounted_) {
    VLOG(1) << "Fusebox isn't mounted, queueing AttachStorage call";
    pending_attach_storage_calls_.emplace(subdir,
                                          std::make_pair(url, read_only));
    return;
  }
  fusebox::Server* fusebox_server = fusebox::Server::GetInstance();
  if (fusebox_server) {
    fusebox_server->RegisterFSURLPrefix(subdir, url, read_only);
  } else {
    VLOG(1) << "No fusebox server available on AttachStorage";
  }
}

void FuseBoxMounter::DetachStorage(const std::string& subdir) {
  if (!mounted_) {
    VLOG(1) << "Fusebox isn't mounted, removing queued AttachStorage call";
    pending_attach_storage_calls_.erase(subdir);
    return;
  }
  fusebox::Server* fusebox_server = fusebox::Server::GetInstance();
  if (fusebox_server) {
    fusebox_server->UnregisterFSURLPrefix(subdir);
  } else {
    VLOG(1) << "No fusebox server available on DetachStorage";
  }
}

void FuseBoxMounter::Mount(FuseBoxDiskMountManager* disk_mount_manager) {
  DCHECK(disk_mount_manager);

  constexpr auto type = ash::MountType::kNetworkStorage;
  constexpr auto mode = ash::MountAccessMode::kReadWrite;

  disk_mount_manager->MountPath(kFuseBoxMounterURI, /*source_format*/ {},
                                /*mount_label*/ {},
                                /*options*/ {}, type, mode,
                                base::BindOnce(&FuseBoxMounter::MountResponse,
                                               weak_ptr_factory_.GetWeakPtr()));
}

void FuseBoxMounter::Unmount(FuseBoxDiskMountManager* disk_mount_manager) {
  DCHECK(disk_mount_manager);

  if (!mounted_) {
    VLOG(1) << "FuseBoxMounter::Unmount ignored: not mounted";
    return;
  }

  disk_mount_manager->UnmountPath(
      kFuseBoxMounterURI, base::BindOnce(&FuseBoxMounter::UnmountResponse,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void FuseBoxMounter::MountResponse(ash::MountError error,
                                   const FuseBoxMountInfo& info) {
  if (error != ash::MountError::kSuccess) {
    LOG(ERROR) << kFuseBoxMounterURI << " mount error " << error;
  } else {
    mounted_ = true;
    if (!pending_attach_storage_calls_.empty()) {
      VLOG(1) << "Calling " << pending_attach_storage_calls_.size()
              << " queued AttachStorage calls";
      for (const auto& it : pending_attach_storage_calls_) {
        const auto& [url, read_only] = it.second;
        AttachStorage(it.first, url, read_only);
      }
    }
  }
  pending_attach_storage_calls_.clear();
}

void FuseBoxMounter::UnmountResponse(ash::MountError error) {
  if (error != ash::MountError::kSuccess) {
    LOG(ERROR) << kFuseBoxMounterURI << " unmount error " << error;
  } else {
    mounted_ = false;
  }
  pending_attach_storage_calls_.clear();
}

}  // namespace file_manager
