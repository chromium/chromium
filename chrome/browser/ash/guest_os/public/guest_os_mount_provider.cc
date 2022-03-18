// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"
#include <memory>

#include "ash/components/disks/disk_mount_manager.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/borealis/infra/expected.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/guest_os/infra/cached_callback.h"
#include "storage/browser/file_system/external_mount_points.h"

namespace guest_os {

// An RAII-style class controlling the lifetime of the SFTP volume. Will add the
// volume on creation and remove it on destruction.
class ScopedVolume {
 public:
  explicit ScopedVolume(
      Profile* profile,
      std::string display_name,
      std::string mount_label,
      base::FilePath homedir,
      const ash::disks::DiskMountManager::MountPointInfo& mount_info)
      : profile_(profile), mount_label_(mount_label) {
    base::FilePath mount_path = base::FilePath(mount_info.mount_path);
    if (!storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
            mount_label_, storage::kFileSystemTypeLocal,
            storage::FileSystemMountOption(), mount_path)) {
      // We don't revoke the filesystem on unmount and this call fails if a
      // filesystem of the same name already exists, so ignore errors.
      // TODO(crbug/1293229): This follows the logic of existing code, but we
      // can probably change it to revoke the filesystem on unmount.
    }

    auto* vmgr = file_manager::VolumeManager::Get(profile_);
    if (vmgr) {
      // vmgr is null in unit tests.
      vmgr->AddSftpGuestOsVolume(display_name, mount_path, homedir);
    }
  }

  ~ScopedVolume() {
    if (profile_->ShutdownStarted()) {
      // We're shutting down, but because we're not a keyed service we don't get
      // two-phase shutdown, we just can't call anything. Either the whole
      // system is shutting down (in which case everything gets undone anyway)
      // or it's just the browser (in which case it's basically the same as a
      // browser crash which we also need to handle).
      // So do nothing.
      return;
    }

    auto* vmgr = file_manager::VolumeManager::Get(profile_);
    if (vmgr) {
      // vmgr is null in unit tests. Also, this calls disk_manager to unmount
      // for us (and we never unregister the filesystem) hence unmount doesn't
      // seem symmetric with mount.
      vmgr->RemoveSftpGuestOsVolume(
          file_manager::util::GetGuestOsMountDirectory(mount_label_),
          base::DoNothing());
    }
  }

  Profile* profile_;
  std::string mount_label_;
};

class GuestOsMountProviderInner : public CachedCallback<ScopedVolume, bool> {
 public:
  explicit GuestOsMountProviderInner(Profile* profile,
                                     std::string display_name,
                                     crostini::ContainerId container_id,
                                     int cid,
                                     int port,
                                     base::FilePath homedir)
      : profile_(profile),
        display_name_(display_name),
        container_id_(container_id),
        cid_(cid),
        port_(port),
        homedir_(homedir) {}

  // Mount.
  void Build(RealCallback callback) override {
    mount_label_ =
        file_manager::util::GetGuestOsMountPointName(profile_, container_id_);
    auto* dmgr = ash::disks::DiskMountManager::GetInstance();

    // Call to sshfs to mount.
    std::string source_path = base::StringPrintf("sftp://%d:%d", cid_, port_);

    dmgr->MountPath(
        source_path, "", mount_label_, {}, chromeos::MOUNT_TYPE_NETWORK_STORAGE,
        chromeos::MOUNT_ACCESS_MODE_READ_WRITE,
        base::BindOnce(&GuestOsMountProviderInner::OnMountEvent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
  void OnMountEvent(
      RealCallback callback,
      chromeos::MountError error_code,
      const ash::disks::DiskMountManager::MountPointInfo& mount_info) {
    if (error_code != chromeos::MountError::MOUNT_ERROR_NONE) {
      LOG(ERROR) << "Error mounting Guest OS container: error_code="
                 << error_code << ", source_path=" << mount_info.source_path
                 << ", mount_path=" << mount_info.mount_path
                 << ", mount_type=" << mount_info.mount_type
                 << ", mount_condition=" << mount_info.mount_condition;
      std::move(callback).Run(Failure(false));
      return;
    }
    auto scoped_volume = std::make_unique<ScopedVolume>(
        profile_, display_name_, mount_label_, homedir_, mount_info);

    // CachedCallback magic keeps the scope alive until we're destroyed or it's
    // invalidated.
    std::move(callback).Run(RealResult(std::move(scoped_volume)));
  }

  Profile* profile_;
  std::string display_name_;
  crostini::ContainerId container_id_;
  std::string mount_label_;
  int cid_;
  int port_;  // vsock port
  base::FilePath homedir_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<GuestOsMountProviderInner> weak_ptr_factory_{this};
};

void GuestOsMountProvider::Mount(base::OnceCallback<void(bool)> callback) {
  if (!callback_) {
    callback_ = std::make_unique<GuestOsMountProviderInner>(
        profile(), DisplayName(), ContainerId(), cid(), port(), homedir());
  }
  callback_->Get(base::BindOnce(
      [](base::OnceCallback<void(bool)> callback,
         guest_os::GuestOsMountProviderInner::Result result) {
        std::move(callback).Run(!!result);
      },
      std::move(callback)));
}

void GuestOsMountProvider::Unmount() {
  callback_->Invalidate();
}

GuestOsMountProvider::GuestOsMountProvider() = default;
GuestOsMountProvider::~GuestOsMountProvider() = default;
int GuestOsMountProvider::cid() {
  return 0;
}
int GuestOsMountProvider::port() {
  return 0;
}
base::FilePath GuestOsMountProvider::homedir() {
  return base::FilePath("/home/fake");
}
}  // namespace guest_os
