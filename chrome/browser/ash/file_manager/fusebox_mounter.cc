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
#include "chromeos/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/dbus/fusebox/fusebox_reverse_client.h"

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

  constexpr auto type = chromeos::MOUNT_TYPE_NETWORK_STORAGE;
  constexpr auto mode = chromeos::MOUNT_ACCESS_MODE_READ_WRITE;

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

  constexpr auto strip_trailing_slash_from = [](std::string string) {
    if (string.size() && base::EndsWith(string, "/"))
      string.resize(string.size() - 1);
    return string;
  };

  std::string name = base::JoinString(
      {subdir, strip_trailing_slash_from(url), read_only ? "ro" : ""}, " ");

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

  client->DetachStorage(subdir, std::move(callback));
}

void FuseBoxMounter::Unmount(FuseBoxDiskMountManager* disk_mount_manager) {
  DCHECK(disk_mount_manager);

  disk_mount_manager->UnmountPath(
      uri_, base::BindOnce(&FuseBoxMounter::UnmountResponse, GetWeakPtr()));
}

base::WeakPtr<FuseBoxMounter> FuseBoxMounter::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FuseBoxMounter::MountResponse(chromeos::MountError error,
                                   const FuseBoxMountInfo& info) {
  if (error) {
    LOG(ERROR) << uri_ << " mount error " << error;
  } else {
    mounted_ = true;
  }
}

void FuseBoxMounter::UnmountResponse(chromeos::MountError error) {
  if (error) {
    LOG(ERROR) << uri_ << " unmount error " << error;
  } else {
    mounted_ = false;
  }
}

}  // namespace file_manager
