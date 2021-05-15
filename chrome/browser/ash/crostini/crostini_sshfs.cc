// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_sshfs.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_manager_factory.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/external_mount_points.h"

namespace crostini {

CrostiniSshfs::CrostiniSshfs(Profile* profile) : profile_(profile) {}

CrostiniSshfs::~CrostiniSshfs() = default;

void CrostiniSshfs::OnContainerShutdown(const ContainerId& container_id) {
  container_shutdown_observer_.Reset();
  SetSshfsMounted(container_id, false);
}

bool CrostiniSshfs::IsSshfsMounted(const ContainerId& container) {
  return (sshfs_mounted_.count(container));
}

void CrostiniSshfs::SetSshfsMounted(const ContainerId& container,
                                    bool mounted) {
  if (mounted) {
    sshfs_mounted_.emplace(container);
  } else {
    sshfs_mounted_.erase(container);
  }
}

void CrostiniSshfs::UnmountCrostiniFiles(const ContainerId& container_id,
                                         MountCrostiniFilesCallback callback) {
  // TODO(crbug/1197986): Unmounting should cancel an in-progress mount.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* vmgr = file_manager::VolumeManager::Get(profile_);
  if (vmgr) {
    // vmgr is NULL in unit tests if not overridden.
    vmgr->RemoveSshfsCrostiniVolume(
        file_manager::util::GetCrostiniMountDirectory(profile_),
        base::BindOnce(&CrostiniSshfs::OnRemoveSshfsCrostiniVolume,
                       weak_ptr_factory_.GetWeakPtr(), container_id,
                       std::move(callback)));
  } else {
    OnRemoveSshfsCrostiniVolume(container_id, std::move(callback), true);
  }
}

void CrostiniSshfs::OnRemoveSshfsCrostiniVolume(
    const ContainerId& container_id,
    MountCrostiniFilesCallback callback,
    bool success) {
  container_shutdown_observer_.Reset();
  SetSshfsMounted(container_id, false);
  std::move(callback).Run(success);
}

void CrostiniSshfs::MountCrostiniFiles(const ContainerId& container_id,
                                       MountCrostiniFilesCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (in_progress_mount_) {
    // A run is already in progress, wait until it finishes.
    pending_requests_.emplace(container_id, std::move(callback));
    return;
  }
  in_progress_mount_ =
      std::make_unique<InProgressMount>(container_id, std::move(callback));

  if (IsSshfsMounted(container_id)) {
    // Already mounted so skip straight to reporting success.
    Finish(true);
    return;
  }

  if (container_id != ContainerId::GetDefault()) {
    LOG(ERROR) << "Unable to mount files for non-default container";
    Finish(false);
    return;
  }

  auto* manager = CrostiniManagerFactory::GetForProfile(profile_);
  absl::optional<ContainerInfo> info = manager->GetContainerInfo(container_id);
  if (!info) {
    LOG(ERROR) << "Unable to mount files for a container that's not running";
    Finish(false);
    return;
  }

  manager->GetContainerSshKeys(
      container_id, base::BindOnce(&CrostiniSshfs::OnGetContainerSshKeys,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniSshfs::OnGetContainerSshKeys(
    bool success,
    const std::string& container_public_key,
    const std::string& host_private_key,
    const std::string& hostname) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    LOG(ERROR) << "Unable to get container ssh keys";
    Finish(false);
    return;
  }

  auto* manager = CrostiniManagerFactory::GetForProfile(profile_);
  absl::optional<ContainerInfo> info =
      manager->GetContainerInfo(in_progress_mount_->container_id);
  if (!info) {
    LOG(ERROR) << "Got ssh keys for a container that's not running. Aborting.";
    Finish(false);
    return;
  }

  // Add ourselves as an observer so we can continue once the path is mounted.
  auto* dmgr = chromeos::disks::DiskMountManager::GetInstance();
  disk_mount_observer_.Observe(dmgr);

  // Call to sshfs to mount.
  in_progress_mount_->source_path = base::StringPrintf(
      "sshfs://%s@%s:", info->username.c_str(), hostname.c_str());
  in_progress_mount_->container_homedir = info->homedir;

  dmgr->MountPath(in_progress_mount_->source_path, "",
                  file_manager::util::GetCrostiniMountPointName(profile_),
                  file_manager::util::GetCrostiniMountOptions(
                      hostname, host_private_key, container_public_key),
                  chromeos::MOUNT_TYPE_NETWORK_STORAGE,
                  chromeos::MOUNT_ACCESS_MODE_READ_WRITE);
}

void CrostiniSshfs::OnMountEvent(
    chromeos::disks::DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const chromeos::disks::DiskMountManager::MountPointInfo& mount_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Ignore any other mount/unmount events.
  if (event != chromeos::disks::DiskMountManager::MountEvent::MOUNTING ||
      mount_info.source_path != in_progress_mount_->source_path) {
    return;
  }

  // Got our mount event, so stop listening for more.
  disk_mount_observer_.Reset();

  if (error_code != chromeos::MountError::MOUNT_ERROR_NONE) {
    LOG(ERROR) << "Error mounting crostini container: error_code=" << error_code
               << ", source_path=" << mount_info.source_path
               << ", mount_path=" << mount_info.mount_path
               << ", mount_type=" << mount_info.mount_type
               << ", mount_condition=" << mount_info.mount_condition;
    Finish(false);
    return;
  }

  base::FilePath mount_path = base::FilePath(mount_info.mount_path);
  if (!storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
          file_manager::util::GetCrostiniMountPointName(profile_),
          storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
          mount_path)) {
    // We don't revoke the filesystem on unmount and this call fails if a
    // filesystem of the same name already exists, so ignore errors.
    // TODO(crbug/1197986): Should we revoke? Keeping it this way for now since
    // that's how it's been for years and it's not come up as an issue before.
    // Since the most common reason for unmounting is to work around an issue
    // with suspend/resume where we promptly remount it's probably good this
    // way.
  }

  auto* vmgr = file_manager::VolumeManager::Get(profile_);
  if (vmgr) {
    // vmgr is NULL in unit tests if not overridden.
    vmgr->AddSshfsCrostiniVolume(mount_path,
                                 in_progress_mount_->container_homedir);
  }

  auto* manager = CrostiniManagerFactory::GetForProfile(profile_);
  container_shutdown_observer_.Observe(manager);
  SetSshfsMounted(in_progress_mount_->container_id, true);
  Finish(true);
}

void CrostiniSshfs::Finish(bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(in_progress_mount_);
  auto callback = std::move(in_progress_mount_->callback);
  std::move(callback).Run(success);
  in_progress_mount_.reset();
  if (!pending_requests_.empty()) {
    auto next = std::move(pending_requests_.front());
    pending_requests_.pop();
    MountCrostiniFiles(next.first, std::move(next.second));
  }
}

CrostiniSshfs::InProgressMount::InProgressMount(
    const ContainerId& container,
    MountCrostiniFilesCallback callback)
    : container_id(container), callback(std::move(callback)) {}
CrostiniSshfs::InProgressMount::~InProgressMount() = default;

}  // namespace crostini
