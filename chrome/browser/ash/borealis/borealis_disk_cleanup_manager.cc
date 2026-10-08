// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_disk_cleanup_manager.h"

#include <cstdint>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"

namespace borealis {

namespace {
constexpr char kBorealisVmName[] = "borealis";
constexpr char kHasAllocatedDiskHistogram[] = "Borealis.HasAllocatedDisk";
}  // namespace

BorealisDiskCleanupManager::BorealisDiskCleanupManager(
    std::string cryptohome_id)
    : cryptohome_id_(std::move(cryptohome_id)) {}

BorealisDiskCleanupManager::~BorealisDiskCleanupManager() = default;

void BorealisDiskCleanupManager::CheckDiskSpace(base::OnceClosure callback) {
  CHECK(!cryptohome_id_.empty());

  auto* concierge_client = ash::ConciergeClient::Get();
  CHECK(concierge_client);

  vm_tools::concierge::ListVmDisksRequest request;
  request.set_cryptohome_id(cryptohome_id_);
  request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
  request.set_vm_name(kBorealisVmName);
  concierge_client->ListVmDisks(
      std::move(request),
      base::BindOnce(&BorealisDiskCleanupManager::OnListVmDisks,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BorealisDiskCleanupManager::OnListVmDisks(
    base::OnceClosure callback,
    std::optional<vm_tools::concierge::ListVmDisksResponse> response) {
  base::ScopedClosureRunner runner(std::move(callback));
  if (!response) {
    LOG(ERROR) << "Failed to list Borealis VM disks: No D-Bus response.";
    return;
  }
  if (!response->success()) {
    LOG(ERROR) << "Failed to list Borealis VM disks: "
               << response->failure_reason();
    return;
  }

  bool vm_exists = false;
  uint64_t disk_size_bytes = 0;
  for (const auto& image : response->images()) {
    if (image.name() == kBorealisVmName) {
      vm_exists = true;
      disk_size_bytes += image.size();
    }
  }

  if (!vm_exists) {
    return;
  }

  base::UmaHistogramBoolean(kHasAllocatedDiskHistogram, disk_size_bytes > 0);
}

}  // namespace borealis
