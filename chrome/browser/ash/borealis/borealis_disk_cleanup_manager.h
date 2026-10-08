// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_CLEANUP_MANAGER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_CLEANUP_MANAGER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"

namespace borealis {

// Checks whether the primary user has an allocated Borealis disk image
// upon login and records the Borealis.HasAllocatedDisk UMA metric.
class BorealisDiskCleanupManager {
 public:
  // `cryptohome_id` must be for the primary user, as Borealis is only
  // supported for the primary user.
  explicit BorealisDiskCleanupManager(std::string cryptohome_id);
  BorealisDiskCleanupManager(const BorealisDiskCleanupManager&) = delete;
  BorealisDiskCleanupManager& operator=(const BorealisDiskCleanupManager&) =
      delete;
  ~BorealisDiskCleanupManager();

  void CheckDiskSpace(base::OnceClosure callback = base::DoNothing());

 private:
  void OnListVmDisks(
      base::OnceClosure callback,
      std::optional<vm_tools::concierge::ListVmDisksResponse> response);

  const std::string cryptohome_id_;
  base::WeakPtrFactory<BorealisDiskCleanupManager> weak_factory_{this};
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_CLEANUP_MANAGER_H_
