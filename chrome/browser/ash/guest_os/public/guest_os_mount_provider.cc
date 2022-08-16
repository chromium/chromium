// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"
#include <memory>

#include "ash/components/disks/disk_mount_manager.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/borealis/infra/expected.h"
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
      base::FilePath remote_path,
      const ash::disks::DiskMountManager::MountPoint& mount_info,
      VmType vm_type)
      : profile_(profile), mount_label_(mount_label), vm_type_(vm_type) {
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
      vmgr->AddSftpGuestOsVolume(display_name, mount_path, remote_path,
                                 vm_type_);
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
          file_manager::util::GetGuestOsMountDirectory(mount_label_), vm_type_,
          base::DoNothing());
    }
  }

  Profile* profile_;
  std::string mount_label_;
  VmType vm_type_;
};

class GuestOsMountProviderInner : public CachedCallback<ScopedVolume, bool> {
 public:
  explicit GuestOsMountProviderInner(
      Profile* profile,
      std::string display_name,
      guest_os::GuestId container_id,
      VmType vm_type,
      base::RepeatingCallback<void(GuestOsMountProvider::PrepareCallback)>
          prepare)
      : profile_(profile),
        display_name_(display_name),
        container_id_(container_id),
        vm_type_(vm_type),
        prepare_(prepare) {}

  // Mount.
  void Build(RealCallback callback) override {
    prepare_.Run(base::BindOnce(&GuestOsMountProviderInner::MountPath,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(callback)));
  }
  void MountPath(RealCallback callback,
                 bool success,
                 int cid,
                 int port,
                 base::FilePath remote_path) {
    if (!success) {
      LOG(ERROR) << "Error mounting, failed to prepare VM";
      std::move(callback).Run(Failure(false));
      return;
    }
    mount_label_ =
        file_manager::util::GetGuestOsMountPointName(profile_, container_id_);
    auto* dmgr = ash::disks::DiskMountManager::GetInstance();

    // Call to sshfs to mount.
    std::string source_path = base::StringPrintf("sftp://%d:%d", cid, port);

    dmgr->MountPath(source_path, "", mount_label_, {},
                    ash::MountType::kNetworkStorage,
                    ash::MountAccessMode::kReadWrite,
                    base::BindOnce(&GuestOsMountProviderInner::OnMountEvent,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(callback), remote_path));
  }
  void OnMountEvent(
      RealCallback callback,
      base::FilePath remote_path,
      ash::MountError error_code,
      const ash::disks::DiskMountManager::MountPoint& mount_info) {
    if (error_code != ash::MountError::kNone) {
      LOG(ERROR) << "Error mounting Guest OS container: error_code="
                 << error_code << ", source_path=" << mount_info.source_path
                 << ", mount_path=" << mount_info.mount_path
                 << ", mount_type=" << static_cast<int>(mount_info.mount_type)
                 << ", mount_condition=" << mount_info.mount_condition;
      std::move(callback).Run(Failure(false));
      return;
    }
    auto scoped_volume =
        std::make_unique<ScopedVolume>(profile_, display_name_, mount_label_,
                                       remote_path, mount_info, vm_type_);

    // CachedCallback magic keeps the scope alive until we're destroyed or it's
    // invalidated.
    std::move(callback).Run(RealResult(std::move(scoped_volume)));
  }

  Profile* profile_;
  std::string display_name_;
  guest_os::GuestId container_id_;
  std::string mount_label_;
  VmType vm_type_;
  // Callback to prepare the VM for mounting.
  base::RepeatingCallback<void(GuestOsMountProvider::PrepareCallback)> prepare_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<GuestOsMountProviderInner> weak_ptr_factory_{this};
};

void GuestOsMountProvider::Mount(base::OnceCallback<void(bool)> callback) {
  if (!callback_) {
    callback_ = std::make_unique<GuestOsMountProviderInner>(
        profile(), DisplayName(), GuestId(), vm_type(),
        base::BindRepeating(&GuestOsMountProvider::Prepare,
                            weak_ptr_factory_.GetWeakPtr()));
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
}  // namespace guest_os
