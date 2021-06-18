// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_disk_manager_impl.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_disk_manager_dispatcher.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/infra/transition.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"

namespace borealis {

struct Nothing {};

constexpr int64_t kGiB = 1024 * 1024 * 1024;
constexpr int64_t kDiskHeadroomBytes = 1 * kGiB;
constexpr int64_t kTargetBufferBytes = 2 * kGiB;
constexpr int64_t kDiskRoundingBytes = 2 * 1024 * 1024;
constexpr int64_t kTargetBufferLowerBound = kTargetBufferBytes * 0.9;
constexpr int64_t kTargetBufferUpperBound = kTargetBufferBytes * 1.1;

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
  // If the disk isn't fixed, we shouldn't be resizing it.
  bool has_fixed_size = 0;
};

BorealisDiskManagerImpl::BorealisDiskManagerImpl(const BorealisContext* context)
    : context_(context),
      free_space_provider_(std::make_unique<FreeSpaceProvider>()),
      weak_factory_(this) {
  borealis::BorealisService::GetForProfile(context_->profile())
      ->DiskManagerDispatcher()
      .SetDiskManagerDelegate(this);
}

BorealisDiskManagerImpl::~BorealisDiskManagerImpl() {
  borealis::BorealisService::GetForProfile(context_->profile())
      ->DiskManagerDispatcher()
      .RemoveDiskManagerDelegate(this);
}

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
      Fail("failed to get the amount of free disk space on the host");
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
      Fail("failed to get response from concierge");
      return;
    }
    if (!response->success()) {
      Fail("concierge failed to list vm disks, returned error: " +
           response->failure_reason());
      return;
    }
    const std::string& vm_name = context_->vm_name();
    auto image =
        std::find_if(response->images().begin(), response->images().end(),
                     [&vm_name](const auto& a) { return a.name() == vm_name; });
    if (image == response->images().end()) {
      Fail("no VM found with name " + vm_name);
      return;
    }

    disk_info_->available_space = image->available_space();
    disk_info_->min_size = image->min_size();
    disk_info_->disk_size = image->size();
    disk_info_->disk_type = image->image_type();
    disk_info_->has_fixed_size = image->user_chosen_size();

    Succeed(std::move(disk_info_));
  }

  BorealisDiskManagerImpl::FreeSpaceProvider* free_space_provider_;
  const BorealisContext* const context_;
  std::unique_ptr<BorealisDiskManagerImpl::BorealisDiskInfo> disk_info_;
  base::WeakPtrFactory<BuildDiskInfo> weak_factory_;
};

class BorealisDiskManagerImpl::ResizeDisk
    : public Transition<BorealisDiskInfo,
                        std::pair<BorealisDiskInfo, BorealisDiskInfo>,
                        std::string>,
      public chromeos::ConciergeClient::DiskImageObserver {
 public:
  explicit ResizeDisk(
      int64_t space_delta,
      BorealisDiskManagerImpl::FreeSpaceProvider* free_space_provider,
      const BorealisContext* context)
      : space_delta_(space_delta),
        free_space_provider_(free_space_provider),
        context_(context),
        weak_factory_(this) {}

  ~ResizeDisk() override {
    chromeos::ConciergeClient::Get()->RemoveDiskImageObserver(this);
  }

  void Start(std::unique_ptr<BorealisDiskManagerImpl::BorealisDiskInfo>
                 start_instance) override {
    DCHECK(!build_disk_info_transition_);
    build_disk_info_transition_ =
        std::make_unique<BuildDiskInfo>(free_space_provider_, context_);
    build_disk_info_transition_->Begin(
        std::move(start_instance), base::BindOnce(&ResizeDisk::HandleDiskInfo,
                                                  weak_factory_.GetWeakPtr()));
  }

 private:
  // Sparse disks can be converted to fixed disks through a regular resize
  // operation. This function adjusts how the resize flow uses the disk
  // information from sparse disks.
  void ConvertToFixedIfNeeded() {
    if (!original_disk_info_.has_fixed_size) {
      // Based on how sparse disks work, it's hard to know exactly what the
      // state of the disk is. I.E sometimes the "minimum size" of the disk is
      // larger than the actual size of the disk and the "available space" on
      // the disk is typically much greater than what the disk actually has. To
      // remedy this, we set the disk size so that it is at least the minimum
      // disk size and then add the target buffer so that we know that the
      // resized disk will have at least the target buffer size as available
      // space (before any additional changes).
      original_disk_info_.disk_size = std::max(original_disk_info_.min_size,
                                               original_disk_info_.disk_size) +
                                      kTargetBufferBytes;
      original_disk_info_.available_space = kTargetBufferBytes;
      original_disk_info_.expandable_space -= kTargetBufferBytes;

      // When shrinking a sparse disk, we are more interested in converting it
      // to a fixed size disk, we set the delta to 0 to loosen the success
      // criteria.
      if (space_delta_ < 0) {
        space_delta_ = 0;
      }
    }
  }

  void HandleDiskInfo(Expected<std::unique_ptr<BorealisDiskInfo>, std::string>
                          disk_info_or_error) {
    build_disk_info_transition_.reset();
    if (!disk_info_or_error) {
      Fail("BuildDiskInfo failed: " + disk_info_or_error.Error());
      return;
    }
    original_disk_info_ = *disk_info_or_error.Value();
    if (original_disk_info_.disk_type !=
        vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW) {
      Fail("cannot resize disk: disk type '" +
           base::NumberToString(original_disk_info_.disk_type) +
           "' cannot be resized");
      return;
    }
    // The information we get on sparse disks is accurate, but needs to be used
    // differently during a resize so that we can:
    // 1. convert the sparse disk to a parameter-conforming fixed size disk.
    // 2. verify that we resized the disk by the requested delta, on top of any
    //    changes we needed to make because of the sparse->fixed conversion.
    ConvertToFixedIfNeeded();
    if (space_delta_ > 0 &&
        original_disk_info_.expandable_space < space_delta_) {
      Fail("the space requested exceeds the space that is expandable");
      return;
    }
    if (space_delta_ < 0) {
      if (original_disk_info_.available_space + space_delta_ <
          kTargetBufferBytes) {
        Fail("shrinking the disk would not leave enough space available");
        return;
      }
      if (original_disk_info_.disk_size + space_delta_ <=
          original_disk_info_.min_size) {
        Fail("cannot shrink the disk below its minimum size");
        return;
      }
    }
    vm_tools::concierge::ResizeDiskImageRequest request;
    request.set_cryptohome_id(
        ash::ProfileHelper::GetUserIdHashFromProfile(context_->profile()));
    request.set_vm_name(context_->vm_name());
    request.set_disk_size(space_delta_ + original_disk_info_.disk_size);
    chromeos::ConciergeClient::Get()->ResizeDiskImage(
        std::move(request), base::BindOnce(&ResizeDisk::HandleResizeResponse,
                                           weak_factory_.GetWeakPtr()));
  }

  void HandleResizeResponse(
      absl::optional<vm_tools::concierge::ResizeDiskImageResponse> response) {
    if (!response) {
      GetUpdatedDiskInfo("got null response from concierge resizing");
      return;
    } else if (response->status() ==
               vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED) {
      GetUpdatedDiskInfo("");
    } else if (response->status() ==
               vm_tools::concierge::DiskImageStatus::DISK_STATUS_IN_PROGRESS) {
      uuid_ = response->command_uuid();
      chromeos::ConciergeClient::Get()->AddDiskImageObserver(this);
    } else {
      GetUpdatedDiskInfo(
          "got an unexpected or error status from concierge when resizing: " +
          base::NumberToString(response->status()));
      return;
    }
  }

  // chromeos::ConciergeClient::DiskImageObserver
  void OnDiskImageProgress(
      const vm_tools::concierge::DiskImageStatusResponse& signal) override {
    if (signal.command_uuid() != uuid_) {
      return;
    }
    switch (signal.status()) {
      case vm_tools::concierge::DiskImageStatus::DISK_STATUS_IN_PROGRESS:
        break;
      case vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED:
        GetUpdatedDiskInfo("");
        break;
      default:
        GetUpdatedDiskInfo(
            "recieved failed or unrecognised status when resizing: " +
            base::NumberToString(signal.status()) + " " +
            signal.failure_reason());
    }
  }

  void GetUpdatedDiskInfo(std::string error) {
    if (!error.empty()) {
      Fail(std::move(error));
      return;
    }
    DCHECK(!build_disk_info_transition_);
    auto disk_info = std::make_unique<BorealisDiskInfo>();
    build_disk_info_transition_ =
        std::make_unique<BuildDiskInfo>(free_space_provider_, context_);
    build_disk_info_transition_->Begin(
        std::move(disk_info), base::BindOnce(&ResizeDisk::HandleUpdatedDiskInfo,
                                             weak_factory_.GetWeakPtr()));
  }

  void HandleUpdatedDiskInfo(Expected<std::unique_ptr<BorealisDiskInfo>,
                                      std::string> disk_info_or_error) {
    if (!disk_info_or_error) {
      Fail("GetUpdatedDiskInfo failed: " + disk_info_or_error.Error());
      return;
    }
    updated_disk_info_ = *disk_info_or_error.Value();
    Succeed(std::make_unique<std::pair<BorealisDiskInfo, BorealisDiskInfo>>(
        std::make_pair(original_disk_info_, updated_disk_info_)));
  }

  int64_t space_delta_;
  BorealisDiskManagerImpl::FreeSpaceProvider* free_space_provider_;
  std::string uuid_;
  BorealisDiskInfo original_disk_info_;
  BorealisDiskInfo updated_disk_info_;
  const BorealisContext* const context_;
  std::unique_ptr<BuildDiskInfo> build_disk_info_transition_;
  base::WeakPtrFactory<ResizeDisk> weak_factory_;
};

class BorealisDiskManagerImpl::SyncDisk
    : public Transition<Nothing, BorealisDiskInfo, std::string> {
 public:
  explicit SyncDisk(
      BorealisDiskManagerImpl::FreeSpaceProvider* free_space_provider,
      const BorealisContext* context)
      : free_space_provider_(free_space_provider),
        context_(context),
        build_disk_info_transition_(free_space_provider_, context_),
        weak_factory_(this) {}

  void Start(std::unique_ptr<Nothing> start_instance) override {
    build_disk_info_transition_.Begin(
        std::make_unique<BorealisDiskInfo>(),
        base::BindOnce(&SyncDisk::HandleDiskInfo, weak_factory_.GetWeakPtr()));
  }

 private:
  void HandleDiskInfo(Expected<std::unique_ptr<BorealisDiskInfo>, std::string>
                          disk_info_or_error) {
    if (!disk_info_or_error) {
      Fail("BuildDiskInfo failed: " + disk_info_or_error.Error());
      return;
    }
    if (!disk_info_or_error.Value()->has_fixed_size) {
      // We're currently not handling sparse sized disks.
      Succeed(std::move(disk_info_or_error.Value()));
      return;
    }
    const BorealisDiskInfo& disk_info = *disk_info_or_error.Value();
    if (IsDiskSizeWithinBounds(disk_info)) {
      Succeed(std::move(disk_info_or_error.Value()));
      return;
    }
    int64_t delta = kTargetBufferBytes - disk_info.available_space;
    if (delta > disk_info.expandable_space) {
      delta = disk_info.expandable_space;
    }
    if (delta == 0) {
      LOG(WARNING) << "borealis' available space is not within parameters, "
                      "but there is not enough free space to expand it";
      Succeed(std::move(disk_info_or_error.Value()));
      return;
    }
    resize_disk_transition_ =
        std::make_unique<ResizeDisk>(delta, free_space_provider_, context_);
    resize_disk_transition_->Begin(
        std::move(disk_info_or_error.Value()),
        base::BindOnce(&SyncDisk::HandleResizeAttempt,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  void HandleResizeAttempt(
      Expected<std::unique_ptr<std::pair<BorealisDiskInfo, BorealisDiskInfo>>,
               std::string> disk_info_or_error) {
    if (!disk_info_or_error) {
      Fail("resize failed: " + disk_info_or_error.Error());
      return;
    }
    if (!IsDiskSizeWithinBounds(disk_info_or_error.Value()->second)) {
      LOG(WARNING) << "disk resized successfully, but available space is not "
                      "within its parameters";
    }
    Succeed(
        std::make_unique<BorealisDiskInfo>(disk_info_or_error.Value()->second));
    return;
  }

  bool IsDiskSizeWithinBounds(const BorealisDiskInfo& disk_info) {
    return disk_info.available_space >= kTargetBufferLowerBound &&
           disk_info.available_space <= kTargetBufferUpperBound;
  }

  BorealisDiskManagerImpl::FreeSpaceProvider* free_space_provider_;
  const BorealisContext* const context_;
  BuildDiskInfo build_disk_info_transition_;
  std::unique_ptr<ResizeDisk> resize_disk_transition_;
  base::WeakPtrFactory<SyncDisk> weak_factory_;
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

void BorealisDiskManagerImpl::RequestSpaceDelta(
    int64_t target_delta,
    base::OnceCallback<void(Expected<uint64_t, std::string>)> callback) {
  if (resize_disk_transition_) {
    std::string error = "another ResizeDisk request is in progress";
    LOG(ERROR) << error;
    std::move(callback).Run(Expected<uint64_t, std::string>::Unexpected(error));
    return;
  }
  if (target_delta == 0) {
    std::string error = "requested delta must not be 0";
    LOG(ERROR) << error;
    std::move(callback).Run(Expected<uint64_t, std::string>::Unexpected(error));
    return;
  }

  auto disk_info = std::make_unique<BorealisDiskInfo>();
  int64_t space_delta =
      target_delta + (target_delta > 0 ? kDiskRoundingBytes : 0);
  resize_disk_transition_ = std::make_unique<ResizeDisk>(
      space_delta, free_space_provider_.get(), context_);
  resize_disk_transition_->Begin(
      std::move(disk_info),
      base::BindOnce(&BorealisDiskManagerImpl::OnRequestSpaceDelta,
                     weak_factory_.GetWeakPtr(), target_delta,
                     std::move(callback)));
}

void BorealisDiskManagerImpl::OnRequestSpaceDelta(
    int64_t target_delta,
    base::OnceCallback<void(Expected<uint64_t, std::string>)> callback,
    Expected<std::unique_ptr<std::pair<BorealisDiskInfo, BorealisDiskInfo>>,
             std::string> disk_info_or_error) {
  resize_disk_transition_.reset();
  if (!disk_info_or_error) {
    std::string error =
        "RequestSpaceDelta failed: " + disk_info_or_error.Error();
    LOG(ERROR) << error;
    std::move(callback).Run(Expected<uint64_t, std::string>::Unexpected(error));
    return;
  }
  int64_t delta = disk_info_or_error.Value()->second.disk_size -
                  disk_info_or_error.Value()->first.disk_size;
  if (target_delta > 0) {
    if (delta < target_delta) {
      std::string error = "RequestSpaceDelta failed: requested " +
                          base::NumberToString(target_delta) +
                          " bytes but got " + base::NumberToString(delta) +
                          " bytes";
      LOG(ERROR) << error;
      std::move(callback).Run(
          Expected<uint64_t, std::string>::Unexpected(error));
      return;
    }
    std::move(callback).Run(Expected<uint64_t, std::string>(delta));
  } else {
    if (delta >= 0) {
      if (!disk_info_or_error.Value()->first.has_fixed_size &&
          disk_info_or_error.Value()->second.has_fixed_size) {
        // We succeeded in trying to convert the disk to a fixed size.
        std::move(callback).Run(Expected<uint64_t, std::string>(0));
        return;
      }
      std::string error = "RequestSpaceDelta failed: failed to shrink the disk";
      LOG(ERROR) << error;
      std::move(callback).Run(
          Expected<uint64_t, std::string>::Unexpected(error));
      return;
    }
    std::move(callback).Run(Expected<uint64_t, std::string>(abs(delta)));
  }
}

void BorealisDiskManagerImpl::RequestSpace(
    uint64_t bytes_requested,
    base::OnceCallback<void(Expected<uint64_t, std::string>)> callback) {
  RequestSpaceDelta(bytes_requested, std::move(callback));
}

void BorealisDiskManagerImpl::ReleaseSpace(
    uint64_t bytes_to_release,
    base::OnceCallback<void(Expected<uint64_t, std::string>)> callback) {
  if (bytes_to_release > std::numeric_limits<int64_t>::max()) {
    std::string error = "ReleaseSpace failed: bytes_to_release overflowed";
    LOG(ERROR) << error;
    std::move(callback).Run(Expected<uint64_t, std::string>::Unexpected(error));
    return;
  }
  RequestSpaceDelta(int64_t(bytes_to_release) * -1, std::move(callback));
}

void BorealisDiskManagerImpl::SyncDiskSize(
    base::OnceCallback<void(std::string)> callback) {
  if (sync_disk_transition_) {
    std::string error = "another SyncDiskSize request is in progress";
    LOG(ERROR) << error;
    std::move(callback).Run(std::move(error));
    return;
  }
  sync_disk_transition_ =
      std::make_unique<SyncDisk>(free_space_provider_.get(), context_);
  sync_disk_transition_->Begin(
      std::make_unique<Nothing>(),
      base::BindOnce(&BorealisDiskManagerImpl::OnSyncDiskSize,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BorealisDiskManagerImpl::OnSyncDiskSize(
    base::OnceCallback<void(std::string)> callback,
    Expected<std::unique_ptr<BorealisDiskInfo>, std::string>
        disk_info_or_error) {
  sync_disk_transition_.reset();
  std::string error = "";
  if (!disk_info_or_error) {
    error = "SyncDiskSize failed: " + disk_info_or_error.Error();
    LOG(ERROR) << error;
  }
  std::move(callback).Run(std::move(error));
}

}  // namespace borealis
