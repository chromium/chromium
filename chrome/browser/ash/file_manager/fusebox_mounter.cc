// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/fusebox_mounter.h"

#include <cerrno>
#include <utility>

#include "ash/components/disks/disk_mount_manager.h"
#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/dbus/fusebox/fusebox_reverse_client.h"

namespace file_manager {

// static
FuseBoxMounter* FuseBoxMounter::Create(std::string uri) {
  if (!ash::FuseBoxReverseClient::Get())
    return nullptr;
  if (!ash::features::IsFileManagerFuseBoxEnabled())
    return nullptr;
  return new FuseBoxMounter(std::move(uri));
}

FuseBoxMounter::FuseBoxMounter(std::string uri) : uri_(uri) {}

FuseBoxMounter::~FuseBoxMounter() = default;

void FuseBoxMounter::Mount(FuseBoxDiskMountManager* disk_mount_manager) {
  DCHECK(disk_mount_manager);

  constexpr auto type = ash::MountType::kNetworkStorage;
  constexpr auto mode = ash::MountAccessMode::kReadWrite;

  disk_mount_manager->MountPath(
      uri_, /*source_format*/ {}, /*mount_label*/ {}, /*options*/ {}, type,
      mode, base::BindOnce(&FuseBoxMounter::MountResponse, GetWeakPtr()));
}

void FuseBoxMounter::AttachStorage(const std::string& subdir,
                                   const std::string& url,
                                   bool read_only,
                                   StorageResult callback) {
  auto* client = ash::FuseBoxReverseClient::Get();
  DCHECK(client);

  if (!mounted_) {
    std::move(callback).Run(ENODEV);
    return;
  }

  fusebox::Server* fusebox_server = fusebox::Server::GetInstance();
  if (fusebox_server) {
    fusebox_server->RegisterFSURLPrefix(subdir, url, read_only);
  }

  std::string name =
      base::JoinString({subdir, subdir, read_only ? "ro" : ""}, " ");

  client->AttachStorage(name, std::move(callback));
}

void FuseBoxMounter::DetachStorage(const std::string& subdir,
                                   StorageResult callback) {
  auto* client = ash::FuseBoxReverseClient::Get();
  DCHECK(client);

  if (!mounted_) {
    std::move(callback).Run(ENODEV);
    return;
  }

  fusebox::Server* fusebox_server = fusebox::Server::GetInstance();
  if (fusebox_server) {
    fusebox_server->UnregisterFSURLPrefix(subdir);
  }

  client->DetachStorage(subdir, std::move(callback));
}

void FuseBoxMounter::Unmount(FuseBoxDiskMountManager* disk_mount_manager) {
  DCHECK(disk_mount_manager);

  if (!mounted_) {
    VLOG(1) << "FuseBoxMounter::Unmount ignored: not mounted";
    return;
  }

  disk_mount_manager->UnmountPath(
      uri_, base::BindOnce(&FuseBoxMounter::UnmountResponse, GetWeakPtr()));
}

base::WeakPtr<FuseBoxMounter> FuseBoxMounter::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FuseBoxMounter::MountResponse(ash::MountError error,
                                   const FuseBoxMountInfo& info) {
  if (error != ash::MountError::kNone) {
    LOG(ERROR) << uri_ << " mount error " << error;
  } else {
    mounted_ = true;
  }
}

void FuseBoxMounter::UnmountResponse(ash::MountError error) {
  if (error != ash::MountError::kNone) {
    LOG(ERROR) << uri_ << " unmount error " << error;
  } else {
    mounted_ = false;
  }
}

}  // namespace file_manager
