// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SSHFS_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SSHFS_H_

#include <queue>
#include <set>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"

namespace crostini {

class CrostiniSshfs : ContainerShutdownObserver {
 public:
  explicit CrostiniSshfs(Profile* profile);

  CrostiniSshfs(const CrostiniSshfs&) = delete;
  CrostiniSshfs& operator=(const CrostiniSshfs&) = delete;

  ~CrostiniSshfs() override;
  using MountCrostiniFilesCallback = base::OnceCallback<void(bool succeeded)>;

  // Mounts the user's Crostini home directory so it's accessible from the host.
  // Must be called from the UI thread, no-op if the home directory is already
  // mounted. If this is something running in the background set background to
  // true, if failures are user-visible set it to false. If you're setting
  // base::DoNothing as the callback then background should be true.
  void MountCrostiniFiles(const guest_os::GuestId& container_id,
                          MountCrostiniFilesCallback callback,
                          bool background);

  // Unmounts the user's Crostini home directory. Must be called from the UI
  // thread.
  void UnmountCrostiniFiles(const guest_os::GuestId& container_id,
                            MountCrostiniFilesCallback callback);

  // ContainerShutdownObserver.
  void OnContainerShutdown(const guest_os::GuestId& container_id) override;

  void OnMountEvent(ash::MountError error_code,
                    const ash::disks::DiskMountManager::MountPoint& mount_info);

  // Returns true if sshfs is mounted for the specified container, else false.
  bool IsSshfsMounted(const guest_os::GuestId& container);

  // Only public so unit tests can reference them without needing to FRIEND_TEST
  // every single test case.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CrostiniSshfsResult {
    kSuccess = 0,
    kNotDefaultContainer = 1,
    kContainerNotRunning = 2,
    kGetSshKeysFailed = 3,
    kGetContainerInfoFailed = 4,
    kMountErrorInternal = 5,
    kMountErrorProgramFailed = 6,
    kMountErrorOther = 7,
    kMaxValue = kMountErrorOther,
  };

 private:
  void SetSshfsMounted(const guest_os::GuestId& container, bool mounted);
  void Finish(CrostiniSshfsResult result);

  void OnRemoveSshfsCrostiniVolume(const guest_os::GuestId& container_id,
                                   MountCrostiniFilesCallback callback,
                                   base::Time started,
                                   bool success);

  struct InProgressMount {
    std::string source_path;
    guest_os::GuestId container_id;
    base::FilePath container_homedir;
    MountCrostiniFilesCallback callback;
    base::Time started;
    bool background;
    InProgressMount(const guest_os::GuestId& container,
                    MountCrostiniFilesCallback callback,
                    bool background);
    InProgressMount(InProgressMount&& other) noexcept;
    InProgressMount& operator=(InProgressMount&& other) noexcept;
    ~InProgressMount();
  };
  struct PendingRequest {
    guest_os::GuestId container_id;
    MountCrostiniFilesCallback callback;
    bool background;
    PendingRequest(const guest_os::GuestId& container_id,
                   MountCrostiniFilesCallback callback,
                   bool background);
    PendingRequest(PendingRequest&& other) noexcept;
    PendingRequest& operator=(PendingRequest&& other) noexcept;
    ~PendingRequest();
  };
  raw_ptr<Profile> profile_;

  base::ScopedObservation<CrostiniManager, ContainerShutdownObserver>
      container_shutdown_observer_{this};

  std::unique_ptr<InProgressMount> in_progress_mount_;

  std::set<guest_os::GuestId> sshfs_mounted_;
  std::queue<PendingRequest> pending_requests_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrostiniSshfs> weak_ptr_factory_{this};
};
}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SSHFS_H_
