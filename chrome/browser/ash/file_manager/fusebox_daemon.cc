// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/fusebox_daemon.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"

namespace file_manager {

// static
scoped_refptr<FuseBoxDaemon> FuseBoxDaemon::GetInstance() {
  static base::NoDestructor<base::WeakPtr<FuseBoxDaemon>> daemon;

  scoped_refptr<FuseBoxDaemon> p = daemon->get();
  if (!p) {
    p = new FuseBoxDaemon();
    *daemon = p->weak_ptr_factory_.GetWeakPtr();
  }

  return p;
}

FuseBoxDaemon::FuseBoxDaemon() {
  cros_disks_mount_manager_ = CrosDisksMountManager::GetInstance();

  if (!cros_disks_mount_manager_) {
    return;  // cros_disks_mount_manager_ can be null in unit_tests.
  }

  constexpr auto type = ash::MountType::kNetworkStorage;
  constexpr auto mode = ash::MountAccessMode::kReadWrite;

  cros_disks_mount_manager_->MountPath(
      CrosDisksFuseBoxHelperURI(),
      /*source_format*/ {},
      /*mount_label*/ {},
      /*options*/ {/* add fusebox --flags here */}, type, mode,
      base::BindOnce(&FuseBoxDaemon::MountResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FuseBoxDaemon::MountResponse(ash::MountError error,
                                  const FuseBoxMountInfo& info) {
  if (error == ash::MountError::kSuccess) {
    mounted_ = true;
  } else {
    if (base::SysInfo::IsRunningOnChromeOS()) {
      LOG(ERROR) << CrosDisksFuseBoxHelperURI() << " mount error " << error;
    }
    mounted_ = (error == ash::MountError::kPathAlreadyMounted);
  }

  if (mounted_ && !pending_attach_storage_calls_.empty()) {
    const auto queued = pending_attach_storage_calls_.size();
    VLOG(1) << "Calling " << queued << " queued AttachStorage calls";
    for (const auto& it : pending_attach_storage_calls_) {
      const auto& [url, read_only] = it.second;
      AttachStorage(it.first, url, read_only);
    }
  }

  pending_attach_storage_calls_.clear();
}

void FuseBoxDaemon::AttachStorage(const std::string& subdir,
                                  const std::string& url,
                                  bool read_only) {
  if (!mounted_) {
    VLOG(1) << "FuseBox is not mounted: queued AttachStorage call";
    pending_attach_storage_calls_.emplace(subdir, std::pair{url, read_only});
    return;
  }

  if (auto* fusebox_server = fusebox::Server::GetInstance()) {
    fusebox_server->RegisterFSURLPrefix(subdir, url, read_only);
  } else {
    VLOG(1) << "No FuseBox server available for AttachStorage";
  }
}

void FuseBoxDaemon::DetachStorage(const std::string& subdir) {
  if (!mounted_) {
    VLOG(1) << "Fusebox is not mounted, removing queued AttachStorage call";
    pending_attach_storage_calls_.erase(subdir);
    return;
  }

  if (auto* fusebox_server = fusebox::Server::GetInstance()) {
    fusebox_server->UnregisterFSURLPrefix(subdir);
  } else {
    VLOG(1) << "No FuseBox server available for DetachStorage";
  }
}

FuseBoxDaemon::~FuseBoxDaemon() {
  if (!cros_disks_mount_manager_) {
    return;  // cros_disks_mount_manager_ can be null in unit_tests.
  }

  cros_disks_mount_manager_->UnmountPath(CrosDisksFuseBoxHelperURI(),
                                         base::DoNothing());
}

}  // namespace file_manager
