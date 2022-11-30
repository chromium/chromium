// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_disk_manager_dispatcher.h"

#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_util.h"

namespace borealis {

// TODO(b/188863477): stop hardcoding these values once they're more
// accessible.
constexpr char kBorealisVmName[] = "borealis";
constexpr char kBorealisContainerName[] = "penguin";

BorealisDiskManagerDispatcher::BorealisDiskManagerDispatcher()
    : disk_manager_delegate_(nullptr) {}

void BorealisDiskManagerDispatcher::GetDiskInfo(
    const std::string& origin_vm_name,
    const std::string& origin_container_name,
    base::OnceCallback<void(Expected<BorealisDiskManager::GetDiskInfoResponse,
                                     Described<BorealisGetDiskInfoResult>>)>
        callback) {
  std::string error = ValidateRequest(origin_vm_name, origin_container_name);
  if (!error.empty()) {
    std::move(callback).Run(
        Expected<BorealisDiskManager::GetDiskInfoResponse,
                 Described<BorealisGetDiskInfoResult>>::
            Unexpected(Described<BorealisGetDiskInfoResult>(
                BorealisGetDiskInfoResult::kInvalidRequest, std::move(error))));
    return;
  }
  disk_manager_delegate_->GetDiskInfo(std::move(callback));
}

void BorealisDiskManagerDispatcher::RequestSpace(
    const std::string& origin_vm_name,
    const std::string& origin_container_name,
    uint64_t bytes_requested,
    base::OnceCallback<void(
        Expected<uint64_t, Described<BorealisResizeDiskResult>>)> callback) {
  std::string error = ValidateRequest(origin_vm_name, origin_container_name);
  if (!error.empty()) {
    std::move(callback).Run(
        Expected<uint64_t, Described<BorealisResizeDiskResult>>::Unexpected(
            Described<BorealisResizeDiskResult>(
                BorealisResizeDiskResult::kInvalidRequest, std::move(error))));
    return;
  }
  disk_manager_delegate_->RequestSpace(bytes_requested, std::move(callback));
}

void BorealisDiskManagerDispatcher::ReleaseSpace(
    const std::string& origin_vm_name,
    const std::string& origin_container_name,
    uint64_t bytes_to_release,
    base::OnceCallback<void(
        Expected<uint64_t, Described<BorealisResizeDiskResult>>)> callback) {
  std::string error = ValidateRequest(origin_vm_name, origin_container_name);
  if (!error.empty()) {
    std::move(callback).Run(
        Expected<uint64_t, Described<BorealisResizeDiskResult>>::Unexpected(
            Described<BorealisResizeDiskResult>(
                BorealisResizeDiskResult::kInvalidRequest, std::move(error))));
    return;
  }
  disk_manager_delegate_->ReleaseSpace(bytes_to_release, std::move(callback));
}

std::string BorealisDiskManagerDispatcher::ValidateRequest(
    const std::string& origin_vm_name,
    const std::string& origin_container_name) {
  if (origin_vm_name != kBorealisVmName ||
      origin_container_name != kBorealisContainerName) {
    return "request does not originate from Borealis";
  }
  if (!disk_manager_delegate_) {
    return "disk manager delegate not set, Borealis probably isn't running";
  }
  return "";
}
void BorealisDiskManagerDispatcher::SetDiskManagerDelegate(
    BorealisDiskManager* disk_manager) {
  DCHECK(!disk_manager_delegate_);
  disk_manager_delegate_ = disk_manager;
}

void BorealisDiskManagerDispatcher::RemoveDiskManagerDelegate(
    BorealisDiskManager* disk_manager) {
  DCHECK(disk_manager == disk_manager_delegate_);
  disk_manager_delegate_ = nullptr;
}

}  // namespace borealis
