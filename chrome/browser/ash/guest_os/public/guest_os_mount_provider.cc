// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/infra/cached_callback.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
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
      : profile_(profile),
        mount_label_(std::move(mount_label)),
        vm_type_(vm_type) {
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
      // We're shutting down or have shut down, but because we're not a keyed
      // service we don't get two-phase shutdown, we just can't call anything.
      // Either the whole system is shutting down (in which case everything
      // gets undone anyway) or it's just the browser (in which case it's
      // basically the same as a browser crash which we also need to handle).
      // So do nothing.
      return;
    }

    auto* vmgr = file_manager::VolumeManager::Get(profile_.get());
    if (vmgr) {
      // vmgr is null in unit tests. Also, this calls disk_manager to unmount
      // for us (and we never unregister the filesystem) hence unmount doesn't
      // seem symmetric with mount.
      vmgr->RemoveSftpGuestOsVolume(
          file_manager::util::GetGuestOsMountDirectory(mount_label_), vm_type_,
          base::DoNothing());
    }
  }

  raw_ptr<Profile> profile_;
  std::string mount_label_;
  const VmType vm_type_;
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
        display_name_(std::move(display_name)),
        container_id_(std::move(container_id)),
        vm_type_(vm_type),
        prepare_(std::move(prepare)) {
    // This profile should be the user's primary profile, not an incognito one.
    DCHECK(!profile->IsOffTheRecord());
  }

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
    if (error_code != ash::MountError::kSuccess) {
      LOG(ERROR) << "Error mounting Guest OS container: error_code="
                 << error_code << ", source_path=" << mount_info.source_path
                 << ", mount_path=" << mount_info.mount_path
                 << ", mount_type=" << static_cast<int>(mount_info.mount_type)
                 << ", mount_error=" << mount_info.mount_error;
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

  raw_ptr<Profile> profile_;
  const std::string display_name_;
  const guest_os::GuestId container_id_;
  std::string mount_label_;
  const VmType vm_type_;
  // Callback to prepare the VM for mounting.
  const base::RepeatingCallback<void(GuestOsMountProvider::PrepareCallback)>
      prepare_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<GuestOsMountProviderInner> weak_ptr_factory_{this};
};

void GuestOsMountProvider::Mount(base::OnceCallback<void(bool)> callback) {
  const bool local_files_allowed =
      policy::local_user_files::LocalUserFilesAllowed();

  // If SkyVaultV2 is enabled (GA version), block all VMs regardless of the
  // type.
  if (!local_files_allowed &&
      base::FeatureList::IsEnabled(features::kSkyVaultV2)) {
    LOG(ERROR) << "Error mounting Guest OS container with guest id="
               << this->GuestId() << ": local user files are disabled";
    std::move(callback).Run(false);
    return;
  }

  // If SkyVaultV2 is disabled (TT version), only block ARC.
  if (!local_files_allowed && vm_type() == VmType::ARCVM) {
    LOG(ERROR) << "Error mounting Guest OS container with guest id="
               << this->GuestId() << ": local user files are disabled";
    std::move(callback).Run(false);
    return;
  }

  if (!callback_) {
    callback_ = std::make_unique<GuestOsMountProviderInner>(
        profile(), DisplayName(), GuestId(), vm_type(),
        base::BindRepeating(&GuestOsMountProvider::Prepare,
                            weak_ptr_factory_.GetWeakPtr()));
  }
  callback_->Get(base::BindOnce(
      [](base::OnceCallback<void(bool)> callback,
         guest_os::GuestOsMountProviderInner::Result result) {
        std::move(callback).Run(result.has_value());
      },
      std::move(callback)));
}

void GuestOsMountProvider::Unmount() {
  callback_->Invalidate();
}

GuestOsMountProvider::GuestOsMountProvider() = default;
GuestOsMountProvider::~GuestOsMountProvider() = default;
}  // namespace guest_os
