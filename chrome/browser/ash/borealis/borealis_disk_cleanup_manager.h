// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_CLEANUP_MANAGER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_CLEANUP_MANAGER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"

class PrefService;

namespace guest_os {
class GuestOsRegistryService;
}  // namespace guest_os

namespace views {
class Widget;
}  // namespace views

namespace borealis {

// Checks whether the primary user has an allocated Borealis disk image
// upon login, records the Borealis.HasAllocatedDisk UMA metric, and prompts
// the user to reclaim the unused disk space until the disk image is deleted.
class BorealisDiskCleanupManager {
 public:
  static constexpr char kCleanupNotificationId[] = "borealis_disk_cleanup";

  // `cryptohome_id` must be for the primary user, as Borealis is only
  // supported for the primary user. `pref_service` and `registry_service`
  // must be non-null and must outlive `this`.
  BorealisDiskCleanupManager(
      std::string cryptohome_id,
      PrefService* pref_service,
      guest_os::GuestOsRegistryService* registry_service);
  BorealisDiskCleanupManager(const BorealisDiskCleanupManager&) = delete;
  BorealisDiskCleanupManager& operator=(const BorealisDiskCleanupManager&) =
      delete;
  ~BorealisDiskCleanupManager();

  void CheckDiskSpace(base::OnceClosure callback = base::DoNothing());

  views::Widget* dialog_widget_for_testing() const {
    return dialog_widget_.get();
  }

 private:
  void OnListVmDisks(
      base::OnceClosure callback,
      std::optional<vm_tools::concierge::ListVmDisksResponse> response);
  void ShowCleanupDialog(uint64_t disk_size_bytes);
  void OnFreeDiskSpace(uint64_t disk_size_bytes);
  void OnDestroyDiskImage(
      uint64_t disk_size_bytes,
      std::optional<vm_tools::concierge::DestroyDiskImageResponse> response);
  void OnDlcUninstalled(uint64_t disk_size_bytes, std::string_view dlc_err);
  void ShowCleanupNotification(uint64_t disk_size_bytes);

  const std::string cryptohome_id_;
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<guest_os::GuestOsRegistryService> registry_service_;
  std::unique_ptr<views::Widget> dialog_widget_;
  base::WeakPtrFactory<BorealisDiskCleanupManager> weak_factory_{this};
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_CLEANUP_MANAGER_H_
