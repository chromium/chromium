// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_disk_manager_impl.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/infra/transition.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"

namespace borealis {

constexpr int64_t kGiB = 1024 * 1024 * 1024;
constexpr int64_t kDiskHeadroomBytes = 1 * kGiB;
constexpr int64_t kTargetBufferBytes = 2 * kGiB;

void BorealisDiskManagerImpl::FreeSpaceProvider::Get(
    base::OnceCallback<void(int64_t)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                     base::FilePath(crostini::kHomeDirectory)),
      std::move(callback));
}

struct BorealisDiskManagerImpl::BorealisDiskInfo {
  // How much the disk can be expanded by (free space on the host device minus
  // a |kDiskHeadroomBytes| buffer).
  int64_t expandable_space = 0;
  // How much available space is left on the disk.
  int64_t available_space = 0;
  // The current size of the disk.
  int64_t disk_size = 0;
  // The minimum size that the disk can currently be shrunk to,
  int64_t min_size = 0;
  // Image type of the disk.
  vm_tools::concierge::DiskImageType disk_type;
};

BorealisDiskManagerImpl::BorealisDiskManagerImpl(const BorealisContext* context)
    : context_(context),
      free_space_provider_(std::make_unique<FreeSpaceProvider>()),
      weak_factory_(this) {}

BorealisDiskManagerImpl::~BorealisDiskManagerImpl() {}

class BorealisDiskManagerImpl::BuildDiskInfo
    : public Transition<BorealisDiskInfo, BorealisDiskInfo, std::string> {
 public:
  explicit BuildDiskInfo(
      BorealisDiskManagerImpl::FreeSpaceProvider* free_space_provider,
      const BorealisContext* context)
      : free_space_provider_(free_space_provider),
        context_(context),
        weak_factory_(this) {}

  void Start(std::unique_ptr<BorealisDiskManagerImpl::BorealisDiskInfo>
                 start_instance) override {
    disk_info_ = std::move(start_instance);
    free_space_provider_->Get(base::BindOnce(
        &BuildDiskInfo::HandleFreeSpaceResult, weak_factory_.GetWeakPtr()));
  }

 private:
  void HandleFreeSpaceResult(int64_t free_space) {
    if (free_space < 0) {
      Fail(
          "OnGetExpandableSpace failed: failed to get the amount of free disk "
          "space on the host");
      return;
    }
    disk_info_->expandable_space =
        std::max(int64_t(free_space - kDiskHeadroomBytes), int64_t(0));
    vm_tools::concierge::ListVmDisksRequest request;
    request.set_cryptohome_id(
        ash::ProfileHelper::GetUserIdHashFromProfile(context_->profile()));
    request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
    request.set_vm_name(context_->vm_name());
    chromeos::ConciergeClient::Get()->ListVmDisks(
        std::move(request),
        base::BindOnce(&BuildDiskInfo::HandleListVmDisksResult,
                       weak_factory_.GetWeakPtr()));
  }

  void HandleListVmDisksResult(
      absl::optional<vm_tools::concierge::ListVmDisksResponse> response) {
    if (!response) {
      Fail("OnGetVmDiskInfo failed: failed to get response from concierge");
      return;
    }
    if (!response->success()) {
      Fail("OnGetVmDiskInfo failed: concierge returned error: " +
           response->failure_reason());
      return;
    }
    const std::string& vm_name = context_->vm_name();
    auto image =
        std::find_if(response->images().begin(), response->images().end(),
                     [&vm_name](const auto& a) { return a.name() == vm_name; });
    if (image == response->images().end()) {
      Fail("OnGetVmDiskInfo failed: no VM found with name " + vm_name);
      return;
    }

    disk_info_->available_space = image->available_space();
    disk_info_->min_size = image->min_size();
    disk_info_->disk_size = image->size();
    disk_info_->disk_type = image->image_type();

    Succeed(std::move(disk_info_));
  }

  BorealisDiskManagerImpl::FreeSpaceProvider* free_space_provider_;
  const BorealisContext* context_;
  std::unique_ptr<BorealisDiskManagerImpl::BorealisDiskInfo> disk_info_;
  base::WeakPtrFactory<BuildDiskInfo> weak_factory_;
};

void BorealisDiskManagerImpl::GetDiskInfo(
    base::OnceCallback<void(Expected<GetDiskInfoResponse, std::string>)>
        callback) {
  auto disk_info = std::make_unique<BorealisDiskInfo>();
  if (build_disk_info_transition_) {
    std::string error = "another GetDiskInfo request is in progress";
    LOG(ERROR) << error;
    std::move(callback).Run(
        Expected<GetDiskInfoResponse, std::string>::Unexpected(
            std::move(error)));
    return;
  }

  build_disk_info_transition_ =
      std::make_unique<BuildDiskInfo>(free_space_provider_.get(), context_);
  build_disk_info_transition_->Begin(
      std::move(disk_info),
      base::BindOnce(&BorealisDiskManagerImpl::BuildGetDiskInfoResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BorealisDiskManagerImpl::BuildGetDiskInfoResponse(
    base::OnceCallback<void(Expected<GetDiskInfoResponse, std::string>)>
        callback,
    Expected<std::unique_ptr<BorealisDiskInfo>, std::string>
        disk_info_or_error) {
  build_disk_info_transition_.reset();
  if (!disk_info_or_error) {
    std::string error = "GetDiskInfo failed: " + disk_info_or_error.Error();
    LOG(ERROR) << error;
    std::move(callback).Run(
        Expected<GetDiskInfoResponse, std::string>::Unexpected(
            std::move(error)));
    return;
  }
  GetDiskInfoResponse response;
  response.available_bytes = std::max(
      int64_t(disk_info_or_error.Value()->available_space - kTargetBufferBytes),
      int64_t(0));
  response.expandable_bytes = disk_info_or_error.Value()->expandable_space;
  std::move(callback).Run(Expected<GetDiskInfoResponse, std::string>(response));
}

}  // namespace borealis
