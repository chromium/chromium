// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_disk_manager_impl.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_disk_manager_dispatcher.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/infra/transition.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"

namespace borealis {

struct Nothing {};

enum class DiskManagementVersion {
  UNMANAGED,  // Disk is unmanaged.
  CROSDISK,   // Disk is managed by crosdisk and disk manager.
  BALLOON,    // Disk is managed by a sparse disk and ballooning.
};

// Helper function to evaluate which disk management settings to use.
DiskManagementVersion DiskManagementVersion() {
  if (base::FeatureList::IsEnabled(ash::features::kBorealisStorageBallooning)) {
    return DiskManagementVersion::BALLOON;
  }
  if (base::FeatureList::IsEnabled(ash::features::kBorealisDiskManagement)) {
    return DiskManagementVersion::CROSDISK;
  }
  return DiskManagementVersion::UNMANAGED;
}

constexpr int64_t kGiB = 1024 * 1024 * 1024;
constexpr int64_t kDiskHeadroomBytes = 1 * kGiB;
constexpr int64_t kTargetBufferBytes = 2 * kGiB;
constexpr int64_t kDiskRoundingBytes = 2 * 1024 * 1024;
constexpr int64_t kTargetBufferLowerBound = kTargetBufferBytes * 0.9;
constexpr int64_t kTargetBufferUpperBound = kTargetBufferBytes * 1.1;

void BorealisDiskManagerImpl::FreeSpaceProvider::Get(
    base::OnceCallback<void(absl::optional<int64_t>)> callback) {
  ash::SpacedClient::Get()->GetFreeDiskSpace(crostini::kHomeDirectory,
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
  bool has_fixed_size = false;
};

BorealisDiskManagerImpl::BorealisDiskManagerImpl(const BorealisContext* context)
    : context_(context),
      service_(borealis::BorealisService::GetForProfile(context_->profile())),
      free_space_provider_(std::make_unique<FreeSpaceProvider>()),
      weak_factory_(this) {
  service_->DiskManagerDispatcher().SetDiskManagerDelegate(this);
}

BorealisDiskManagerImpl::~BorealisDiskManagerImpl() {
  service_->DiskManagerDispatcher().RemoveDiskManagerDelegate(this);
}

// Helper function that returns how many bytes the |available_space| would
// need to be expanded by in order to regenreate the buffer.
int64_t MissingBufferBytes(int64_t available_space) {
  return std::max(int64_t(kTargetBufferBytes - available_space), int64_t(0));
}

// Helper function that returns how much space is available when excluding
// the buffer space.
int64_t ExcludeBufferBytes(int64_t available_space) {
  return std::max(int64_t(available_space - kTargetBufferBytes), int64_t(0));
}

class BorealisDiskManagerImpl::BuildDiskInfo
    : public Transition<BorealisDiskInfo,
                        BorealisDiskInfo,
                        Described<BorealisGetDiskInfoResult>> {
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
  void HandleFreeSpaceResult(absl::optional<int64_t> free_space) {
    if (!free_space.has_value() || free_space.value() < 0) {
      Fail(Described<BorealisGetDiskInfoResult>(
          BorealisGetDiskInfoResult::kFailedGettingExpandableSpace,
          "failed to get the amount of free disk space on the host"));
      return;
    }
    disk_info_->expandable_space =
        std::max(int64_t(free_space.value() - kDiskHeadroomBytes), int64_t(0));
    vm_tools::concierge::ListVmDisksRequest request;
    request.set_cryptohome_id(
        ash::ProfileHelper::GetUserIdHashFromProfile(context_->profile()));
    request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
    request.set_vm_name(context_->vm_name());
    ash::ConciergeClient::Get()->ListVmDisks(
        std::move(request),
        base::BindOnce(&BuildDiskInfo::HandleListVmDisksResult,
                       weak_factory_.GetWeakPtr()));
  }

  void HandleListVmDisksResult(
      absl::optional<vm_tools::concierge::ListVmDisksResponse> response) {
    if (!response) {
      Fail(Described<BorealisGetDiskInfoResult>(
          BorealisGetDiskInfoResult::kConciergeFailed,
          "failed to get response from concierge"));
      return;
    }
    if (!response->success()) {
      Fail(Described<BorealisGetDiskInfoResult>(
          BorealisGetDiskInfoResult::kConciergeFailed,
          "concierge failed to list vm disks, returned error: " +
              response->failure_reason()));
      return;
    }
    const std::string& vm_name = context_->vm_name();
    auto image = base::ranges::find(response->images(), vm_name,
                                    &vm_tools::concierge::VmDiskInfo::name);
    if (image == response->images().end()) {
      Fail(Described<BorealisGetDiskInfoResult>(
          BorealisGetDiskInfoResult::kConciergeFailed,
          "no VM found with name " + vm_name));
      return;
    }

    disk_info_->available_space = image->available_space();
    disk_info_->min_size = image->min_size();
    disk_info_->disk_size = image->size();
    disk_info_->disk_type = image->image_type();
    disk_info_->has_fixed_size = image->user_chosen_size();

    Succeed(std::move(disk_info_));
  }

  raw_ptr<BorealisDiskManagerImpl::FreeSpaceProvider, ExperimentalAsh>
      free_space_provider_;
  const raw_ptr<const BorealisContext, ExperimentalAsh> context_;
  std::unique_ptr<BorealisDiskManagerImpl::BorealisDiskInfo> disk_info_;
  base::WeakPtrFactory<BuildDiskInfo> weak_factory_;
};

class BorealisDiskManagerImpl::ResizeDisk
    : public Transition<BorealisDiskInfo,
                        std::pair<BorealisDiskInfo, BorealisDiskInfo>,
                        Described<BorealisResizeDiskResult>>,
      public ash::ConciergeClient::DiskImageObserver {
 public:
  explicit ResizeDisk(
      int64_t space_delta,
      bool client_request,
      BorealisDiskManagerImpl::FreeSpaceProvider* free_space_provider,
      const BorealisContext* context)
      : space_delta_(space_delta),
        client_request_(client_request),
        free_space_provider_(free_space_provider),
        context_(context),
        weak_factory_(this) {}

  ~ResizeDisk() override {
    ash::ConciergeClient::Get()->RemoveDiskImageObserver(this);
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

  void HandleDiskInfo(
      base::expected<std::unique_ptr<BorealisDiskInfo>,
                     Described<BorealisGetDiskInfoResult>> disk_info_or_error) {
    build_disk_info_transition_.reset();
    if (!disk_info_or_error.has_value()) {
      Fail(Described<BorealisResizeDiskResult>(
          BorealisResizeDiskResult::kFailedToGetDiskInfo,
          "BuildDiskInfo failed: " + disk_info_or_error.error().description()));
      return;
    }
    original_disk_info_ = *disk_info_or_error.value();
    if (original_disk_info_.disk_type !=
        vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW) {
      Fail(Described<BorealisResizeDiskResult>(
          BorealisResizeDiskResult::kInvalidDiskType,
          "cannot resize disk: disk type '" +
              base::NumberToString(original_disk_info_.disk_type) +
              "' cannot be resized"));
      return;
    }

    // The information we get on sparse disks is accurate, but needs to be used
    // differently during a resize so that we can:
    // 1. convert the sparse disk to a parameter-conforming fixed size disk.
    // 2. verify that we resized the disk by the requested delta, on top of any
    //    changes we needed to make because of the sparse->fixed conversion.
    ConvertToFixedIfNeeded();
    if (space_delta_ > 0) {
      if (client_request_) {
        // Regenerate the buffer, if needed, by tacking it onto the existing
        // request.
        space_delta_ += MissingBufferBytes(original_disk_info_.available_space);
      }
      if (original_disk_info_.expandable_space < space_delta_) {
        Fail(Described<BorealisResizeDiskResult>(
            BorealisResizeDiskResult::kNotEnoughExpandableSpace,
            "the space requested exceeds the space that is expandable"));
        return;
      }
    }
    if (space_delta_ < 0) {
      if (original_disk_info_.available_space + space_delta_ <
          kTargetBufferBytes) {
        Fail(Described<BorealisResizeDiskResult>(
            BorealisResizeDiskResult::kWouldNotLeaveEnoughSpace,
            "shrinking the disk would not leave enough space available"));
        return;
      }
      if (original_disk_info_.disk_size + space_delta_ <=
          original_disk_info_.min_size) {
        if (original_disk_info_.disk_size < original_disk_info_.min_size) {
          Fail(Described<BorealisResizeDiskResult>(
              BorealisResizeDiskResult::kViolatesMinimumSize,
              "cannot shrink the disk below its minimum size"));
          return;
        }
        space_delta_ = original_disk_info_.min_size -
                       original_disk_info_.disk_size + kDiskRoundingBytes;
      }
    }
    vm_tools::concierge::ResizeDiskImageRequest request;
    request.set_cryptohome_id(
        ash::ProfileHelper::GetUserIdHashFromProfile(context_->profile()));
    request.set_vm_name(context_->vm_name());
    request.set_disk_size(space_delta_ + original_disk_info_.disk_size);
    ash::ConciergeClient::Get()->ResizeDiskImage(
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
      ash::ConciergeClient::Get()->AddDiskImageObserver(this);
    } else {
      GetUpdatedDiskInfo(
          "got an unexpected or error status from concierge when resizing: " +
          base::NumberToString(response->status()));
      return;
    }
  }

  // ash::ConciergeClient::DiskImageObserver
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
            "received failed or unrecognised status when resizing: " +
            base::NumberToString(signal.status()) + " " +
            signal.failure_reason());
    }
  }

  void GetUpdatedDiskInfo(std::string error) {
    if (!error.empty()) {
      Fail(Described<BorealisResizeDiskResult>(
          BorealisResizeDiskResult::kConciergeFailed, std::move(error)));
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

  void HandleUpdatedDiskInfo(
      base::expected<std::unique_ptr<BorealisDiskInfo>,
                     Described<BorealisGetDiskInfoResult>> disk_info_or_error) {
    if (!disk_info_or_error.has_value()) {
      Fail(Described<BorealisResizeDiskResult>(
          BorealisResizeDiskResult::kFailedGettingUpdate,
          "GetUpdatedDiskInfo failed: " +
              disk_info_or_error.error().description()));
      return;
    }
    updated_disk_info_ = *disk_info_or_error.value();
    Succeed(std::make_unique<std::pair<BorealisDiskInfo, BorealisDiskInfo>>(
        std::make_pair(original_disk_info_, updated_disk_info_)));
  }

  int64_t space_delta_;
  // Will emit additional metrics if the transition is for a client request.
  bool client_request_;
  raw_ptr<BorealisDiskManagerImpl::FreeSpaceProvider, ExperimentalAsh>
      free_space_provider_;
  std::string uuid_;
  BorealisDiskInfo original_disk_info_;
  BorealisDiskInfo updated_disk_info_;
  const raw_ptr<const BorealisContext, ExperimentalAsh> context_;
  std::unique_ptr<BuildDiskInfo> build_disk_info_transition_;
  base::WeakPtrFactory<ResizeDisk> weak_factory_;
};

class BorealisDiskManagerImpl::SyncDisk
    : public Transition<Nothing,
                        BorealisSyncDiskSizeResult,
                        Described<BorealisSyncDiskSizeResult>> {
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
  void HandleDiskInfo(
      base::expected<std::unique_ptr<BorealisDiskInfo>,
                     Described<BorealisGetDiskInfoResult>> disk_info_or_error) {
    if (!disk_info_or_error.has_value()) {
      Fail(Described<BorealisSyncDiskSizeResult>(
          BorealisSyncDiskSizeResult::kFailedToGetDiskInfo,
          "BuildDiskInfo failed: " + disk_info_or_error.error().description()));
      return;
    }
    if (!disk_info_or_error.value()->has_fixed_size) {
      // We're currently not handling sparse sized disks.
      Succeed(std::make_unique<BorealisSyncDiskSizeResult>(
          BorealisSyncDiskSizeResult::kDiskNotFixed));
      return;
    }
    const BorealisDiskInfo& disk_info = *disk_info_or_error.value();
    if (IsDiskSizeWithinBounds(disk_info)) {
      Succeed(std::make_unique<BorealisSyncDiskSizeResult>(
          BorealisSyncDiskSizeResult::kNoActionNeeded));
      return;
    }
    int64_t delta = kTargetBufferBytes - disk_info.available_space;
    if (delta > disk_info.expandable_space) {
      delta = disk_info.expandable_space;
    }
    if (delta == 0) {
      LOG(WARNING) << "borealis' available space is not within parameters, "
                      "but there is not enough free space to expand it";
      Succeed(std::make_unique<BorealisSyncDiskSizeResult>(
          BorealisSyncDiskSizeResult::kNotEnoughSpaceToExpand));
      return;
    }
    resize_disk_transition_ = std::make_unique<ResizeDisk>(
        delta, /*client_request=*/false, free_space_provider_, context_);
    resize_disk_transition_->Begin(
        std::move(disk_info_or_error.value()),
        base::BindOnce(&SyncDisk::HandleResizeAttempt,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  void HandleResizeAttempt(
      base::expected<
          std::unique_ptr<std::pair<BorealisDiskInfo, BorealisDiskInfo>>,
          Described<BorealisResizeDiskResult>> disk_info_or_error) {
    if (!disk_info_or_error.has_value()) {
      // Sometimes the disk size can get out of sync, so that btrfs reports that
      // the minimum size of the disk is larger than the actual disk size. In
      // this case we will get a kViolatesMinimumSize from trying to resize the
      // disk. We don't want to block the startup process because of this error
      // so we special case it as a success.
      if (disk_info_or_error.error().error() ==
          BorealisResizeDiskResult::kViolatesMinimumSize) {
        LOG(WARNING) << "disk was unable to be shrunk due to the disk "
                        "already being smaller than the minimum size";
        Succeed(std::make_unique<BorealisSyncDiskSizeResult>(
            BorealisSyncDiskSizeResult::kDiskSizeSmallerThanMin));
        return;
      }
      Fail(Described<BorealisSyncDiskSizeResult>(
          BorealisSyncDiskSizeResult::kResizeFailed,
          "resize failed: " + disk_info_or_error.error().description()));
      return;
    }
    if (!IsDiskSizeWithinBounds(disk_info_or_error.value()->second)) {
      LOG(WARNING) << "disk resized successfully, but available space is not "
                      "within its parameters";
      Succeed(std::make_unique<BorealisSyncDiskSizeResult>(
          BorealisSyncDiskSizeResult::kResizedPartially));
    }
    Succeed(std::make_unique<BorealisSyncDiskSizeResult>(
        BorealisSyncDiskSizeResult::kResizedSuccessfully));
    return;
  }

  bool IsDiskSizeWithinBounds(const BorealisDiskInfo& disk_info) {
    return disk_info.available_space >= kTargetBufferLowerBound &&
           disk_info.available_space <= kTargetBufferUpperBound;
  }

  raw_ptr<BorealisDiskManagerImpl::FreeSpaceProvider, ExperimentalAsh>
      free_space_provider_;
  const raw_ptr<const BorealisContext, ExperimentalAsh> context_;
  BuildDiskInfo build_disk_info_transition_;
  std::unique_ptr<ResizeDisk> resize_disk_transition_;
  base::WeakPtrFactory<SyncDisk> weak_factory_;
};

void BorealisDiskManagerImpl::GetDiskInfo(
    base::OnceCallback<
        void(base::expected<GetDiskInfoResponse,
                            Described<BorealisGetDiskInfoResult>>)> callback) {
  if (!base::FeatureList::IsEnabled(ash::features::kBorealisDiskManagement)) {
    std::move(callback).Run(
        base::unexpected(Described<BorealisGetDiskInfoResult>(
            BorealisGetDiskInfoResult::kInvalidRequest,
            "GetDiskInfo failed: feature not enabled")));
    return;
  }
  auto disk_info = std::make_unique<BorealisDiskInfo>();
  request_count_++;
  if (build_disk_info_transition_) {
    std::move(callback).Run(
        base::unexpected(Described<BorealisGetDiskInfoResult>(
            BorealisGetDiskInfoResult::kAlreadyInProgress,
            "another GetDiskInfo request is in progress")));
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
    base::OnceCallback<
        void(base::expected<GetDiskInfoResponse,
                            Described<BorealisGetDiskInfoResult>>)> callback,
    base::expected<std::unique_ptr<BorealisDiskInfo>,
                   Described<BorealisGetDiskInfoResult>> disk_info_or_error) {
  build_disk_info_transition_.reset();
  if (!disk_info_or_error.has_value()) {
    std::move(callback).Run(
        base::unexpected(Described<BorealisGetDiskInfoResult>(
            std::move(disk_info_or_error.error()))));
    return;
  }
  GetDiskInfoResponse response;
  // We are not supporting the case where users enable the flag and then later
  // disable it (after their disk has been converted to a fixed size). The
  // workaround for this is to reinstall the VM or use VMC to manually manage
  // the disk.
  if (base::FeatureList::IsEnabled(ash::features::kBorealisDiskManagement)) {
    if (disk_info_or_error.value()->has_fixed_size) {
      response.available_bytes =
          ExcludeBufferBytes(disk_info_or_error.value()->available_space);
    } else {
      // If the disk is still sparse, then we set the available space to 0 in
      // order to force the client to request for more space if it needs any.
      response.available_bytes = 0;
    }
    // Space for regenerating the buffer needs to be set aside, so we mark it
    // as non-expandable to the client in our response.
    response.expandable_bytes =
        disk_info_or_error.value()->expandable_space -
        MissingBufferBytes(disk_info_or_error.value()->available_space);
    response.disk_size = disk_info_or_error.value()->disk_size;
  } else {
    // If the flag is not active, then the disk should not be resized and the VM
    // can only make use of what it has available.
    response.available_bytes = disk_info_or_error.value()->available_space;
    response.expandable_bytes = 0;
    response.disk_size = 0;
  }
  std::move(callback).Run(
      base::expected<GetDiskInfoResponse, Described<BorealisGetDiskInfoResult>>(
          response));
}

void BorealisDiskManagerImpl::RequestSpaceDelta(
    int64_t target_delta,
    base::OnceCallback<
        void(base::expected<uint64_t, Described<BorealisResizeDiskResult>>)>
        callback) {
  DCHECK(target_delta != 0);
  if (resize_disk_transition_) {
    std::move(callback).Run(
        base::unexpected(Described<BorealisResizeDiskResult>(
            BorealisResizeDiskResult::kAlreadyInProgress,
            "another ResizeDisk request is in progress")));
    return;
  }

  auto disk_info = std::make_unique<BorealisDiskInfo>();
  int64_t space_delta =
      target_delta + (target_delta > 0 ? kDiskRoundingBytes : 0);
  resize_disk_transition_ =
      std::make_unique<ResizeDisk>(space_delta, /*client_request=*/true,
                                   free_space_provider_.get(), context_);
  resize_disk_transition_->Begin(
      std::move(disk_info),
      base::BindOnce(&BorealisDiskManagerImpl::OnRequestSpaceDelta,
                     weak_factory_.GetWeakPtr(), target_delta,
                     std::move(callback)));
}

void BorealisDiskManagerImpl::OnRequestSpaceDelta(
    int64_t target_delta,
    base::OnceCallback<
        void(base::expected<uint64_t, Described<BorealisResizeDiskResult>>)>
        callback,
    base::expected<
        std::unique_ptr<std::pair<BorealisDiskInfo, BorealisDiskInfo>>,
        Described<BorealisResizeDiskResult>> disk_info_or_error) {
  bool expanding = target_delta > 0;
  resize_disk_transition_.reset();
  if (!disk_info_or_error.has_value()) {
    std::move(callback).Run(
        base::unexpected(std::move(disk_info_or_error.error())));
    return;
  }
  int64_t delta = disk_info_or_error.value()->second.disk_size -
                  disk_info_or_error.value()->first.disk_size;
  if (expanding) {
    // Exclude the space that was required to regenerate the buffer.
    delta -=
        MissingBufferBytes(disk_info_or_error.value()->first.available_space);
    if (delta < target_delta) {
      std::move(callback).Run(
          base::unexpected(Described<BorealisResizeDiskResult>(
              BorealisResizeDiskResult::kFailedToFulfillRequest,
              "requested " + base::NumberToString(target_delta) +
                  " bytes but got " + base::NumberToString(delta) + " bytes")));
      return;
    }
  } else {
    if (delta >= 0) {
      if (!disk_info_or_error.value()->first.has_fixed_size &&
          disk_info_or_error.value()->second.has_fixed_size) {
        // We succeeded in trying to convert the disk to a fixed size.
        std::move(callback).Run(
            base::expected<uint64_t, Described<BorealisResizeDiskResult>>(0));
        return;
      }
      std::move(callback).Run(
          base::unexpected(Described<BorealisResizeDiskResult>(
              BorealisResizeDiskResult::kFailedToFulfillRequest,
              "failed to shrink the disk")));
      return;
    }
  }
  std::move(callback).Run(
      base::expected<uint64_t, Described<BorealisResizeDiskResult>>(
          abs(delta)));
}

void BorealisDiskManagerImpl::RequestSpace(
    uint64_t bytes_requested,
    base::OnceCallback<
        void(base::expected<uint64_t, Described<BorealisResizeDiskResult>>)>
        callback) {
  if (!base::FeatureList::IsEnabled(ash::features::kBorealisDiskManagement)) {
    std::move(callback).Run(
        base::unexpected(Described<BorealisResizeDiskResult>(
            BorealisResizeDiskResult::kInvalidRequest,
            "RequestSpace failed: feature not enabled")));
    return;
  }
  request_count_++;
  if (bytes_requested == 0) {
    std::move(callback).Run(
        base::unexpected(Described<BorealisResizeDiskResult>(
            BorealisResizeDiskResult::kInvalidRequest,
            "requested_bytes must not be 0")));
    return;
  }
  RequestSpaceDelta(bytes_requested, std::move(callback));
}

void BorealisDiskManagerImpl::ReleaseSpace(
    uint64_t bytes_to_release,
    base::OnceCallback<
        void(base::expected<uint64_t, Described<BorealisResizeDiskResult>>)>
        callback) {
  if (!base::FeatureList::IsEnabled(ash::features::kBorealisDiskManagement)) {
    std::move(callback).Run(
        base::unexpected(Described<BorealisResizeDiskResult>(
            BorealisResizeDiskResult::kInvalidRequest,
            "ReleaseSpace failed: feature not enabled")));
    return;
  }
  request_count_++;
  if (bytes_to_release == 0) {
    std::move(callback).Run(
        base::unexpected(Described<BorealisResizeDiskResult>(
            BorealisResizeDiskResult::kInvalidRequest,
            "bytes_to_release must not be 0")));
    return;
  }
  if (bytes_to_release > std::numeric_limits<int64_t>::max()) {
    std::move(callback).Run(
        base::unexpected(Described<BorealisResizeDiskResult>(
            BorealisResizeDiskResult::kOverflowError,
            "bytes_to_release overflowed")));
    return;
  }
  RequestSpaceDelta(int64_t(bytes_to_release) * -1, std::move(callback));
}

void BorealisDiskManagerImpl::SyncDiskSize(
    base::OnceCallback<
        void(base::expected<BorealisSyncDiskSizeResult,
                            Described<BorealisSyncDiskSizeResult>>)> callback) {
  if (!base::FeatureList::IsEnabled(ash::features::kBorealisDiskManagement)) {
    std::move(callback).Run(
        base::expected<BorealisSyncDiskSizeResult,
                       Described<BorealisSyncDiskSizeResult>>(
            BorealisSyncDiskSizeResult::kNoActionNeeded));
    return;
  }
  if (sync_disk_transition_) {
    std::move(callback).Run(
        base::unexpected(Described<BorealisSyncDiskSizeResult>(
            BorealisSyncDiskSizeResult::kAlreadyInProgress,
            "another SyncDiskSize request is in progress")));
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
    base::OnceCallback<
        void(base::expected<BorealisSyncDiskSizeResult,
                            Described<BorealisSyncDiskSizeResult>>)> callback,
    base::expected<std::unique_ptr<BorealisSyncDiskSizeResult>,
                   Described<BorealisSyncDiskSizeResult>> result) {
  sync_disk_transition_.reset();
  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(std::move(result.error())));
    return;
  }
  std::move(callback).Run(*result.value());
}

}  // namespace borealis
