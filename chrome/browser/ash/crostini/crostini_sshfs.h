// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SSHFS_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SSHFS_H_

#include <queue>
#include <set>
#include <utility>
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chromeos/disks/disk_mount_manager.h"

namespace crostini {

class CrostiniSshfs : chromeos::disks::DiskMountManager::Observer,
                      ContainerShutdownObserver {
 public:
  explicit CrostiniSshfs(Profile* profile);
  ~CrostiniSshfs() override;
  using MountCrostiniFilesCallback = base::OnceCallback<void(bool succeeded)>;
  // Mounts the user's Crostini home directory so it's accessible from the host.
  // Must be called from the UI thread, no-op if the home directory is already
  // mounted.
  void MountCrostiniFiles(const ContainerId& container_id,
                          MountCrostiniFilesCallback callback);

  // Unmounts the user's Crostini home directory. Must be called from the UI
  // thread.
  void UnmountCrostiniFiles(const ContainerId& container_id,
                            MountCrostiniFilesCallback callback);

  // ContainerShutdownObserver.
  void OnContainerShutdown(const ContainerId& container_id) override;

  // chromeos::disks::DiskMountManager::Observer.
  void OnMountEvent(chromeos::disks::DiskMountManager::MountEvent event,
                    chromeos::MountError error_code,
                    const chromeos::disks::DiskMountManager::MountPointInfo&
                        mount_info) override;

  // Returns true if sshfs is mounted for the specified container, else false.
  bool IsSshfsMounted(const ContainerId& container);

 private:
  void SetSshfsMounted(const ContainerId& container, bool mounted);
  void Finish(bool success);

  void OnRemoveSshfsCrostiniVolume(const ContainerId& container_id,
                                   MountCrostiniFilesCallback callback,
                                   bool success);

  void OnGetContainerSshKeys(bool success,
                             const std::string& container_public_key,
                             const std::string& host_private_key,
                             const std::string& hostname);

  struct InProgressMount {
    std::string source_path;
    ContainerId container_id;
    base::FilePath container_homedir;
    MountCrostiniFilesCallback callback;
    InProgressMount(const ContainerId& container,
                    MountCrostiniFilesCallback callback);
    ~InProgressMount();
  };
  Profile* profile_;

  base::ScopedObservation<chromeos::disks::DiskMountManager,
                          chromeos::disks::DiskMountManager::Observer>
      disk_mount_observer_{this};
  base::ScopedObservation<CrostiniManager,
                          ContainerShutdownObserver,
                          &CrostiniManager::AddContainerShutdownObserver,
                          &CrostiniManager::RemoveContainerShutdownObserver>
      container_shutdown_observer_{this};

  std::unique_ptr<InProgressMount> in_progress_mount_;

  std::set<ContainerId> sshfs_mounted_;
  std::queue<std::pair<ContainerId, MountCrostiniFilesCallback>>
      pending_requests_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrostiniSshfs> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniSshfs);
};
}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SSHFS_H_
